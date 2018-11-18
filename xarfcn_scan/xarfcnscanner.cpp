#include "xarfcnscanner.hpp"

using namespace gg;

XARFCNScanner::XARFCNScanner(double rate, size_t numsamples, size_t winsize)
    :   m_sampleRate{rate}, 
        m_numberOfSamples{numsamples},
        m_window(rate, winsize)
{
    if(winsize % 2 != 0)
        throw std::invalid_argument("XARFCNScanner::XARFCNScanner: winsize should be a multiple of two");
    if(numsamples % winsize != 0)
        throw std::invalid_argument("XARFCNScanner::XARFCNScanner: numsamples should be a multiple of winsize");

    m_fftInBuffer.resize(winsize);
    m_fftOutBuffer.resize(winsize);
    m_sampleBuffer.resize(numsamples);

    m_fftPlan = fftwf_plan_dft_1d(
        winsize, 
        reinterpret_cast<fftwf_complex*>(&m_fftInBuffer[0]),
        reinterpret_cast<fftwf_complex*>(&m_fftOutBuffer[0]),
        FFTW_FORWARD,
        FFTW_MEASURE
    );
    if(m_fftPlan == nullptr) {
        throw std::runtime_error("XARFCNScanner::XARFCNScanner: could not create fftw3 plan");
    }
}

void XARFCNScanner::initRadio(const XARFCNRadioArguments& args)
{
    std::string clockSource = "internal";

    m_usrp = uhd::usrp::multi_usrp::make(args.DeviceArgs);
    auto clockSources = m_usrp->get_clock_sources(0);
    for(auto& s : clockSources) {
        if(args.PreferredClockSource == s) {
            clockSource = s;
            break;
        }
    }
    
    m_usrp->set_clock_source(clockSource);
    m_usrp->set_rx_rate(args.SampleRate);
    m_usrp->set_rx_gain(args.Gain);
    m_usrp->set_rx_bandwidth(args.AnalogBandwidth);
    m_usrp->set_rx_antenna(args.PreferredAntenna);
    sleep(1);
}

void XARFCNScanner::tuneAndWait(double freq)
{
    uhd::tune_request_t tune_request(freq);
    m_usrp->set_rx_freq(tune_request);
    while(1) {
        auto locked = m_usrp->get_rx_sensor("lo_locked");
        if(locked.to_bool())
            break;
        usleep(150000);
    }
    sleep(5);
}

void XARFCNScanner::windowSignal(const ComplexArrayViewf& signal)
{
    for(size_t i = 0, e = m_window.size(); i < e; ++i) {
        m_fftInBuffer[i] = signal[i]*m_window[i];
    }
}

std::tuple<double,double> XARFCNScanner::measurePowerRatio(const Arrayf& ps)
{
    auto n = ps.size();
    double binres = m_sampleRate/double(n);
    double dx = binres/2.0;
    size_t l = size_t(std::ceil(95_kHz/binres));

    auto halfn = n/2;

    double totalPower = ps[halfn] + ps[halfn-1];
    double inbandPower = 0.0f;

    for(size_t i = 0; i < n-1; ++i) {
        auto j = i < halfn ? halfn + i : -halfn + i;
        totalPower += 2.0*ps[j];

        if(i > (halfn - l) && i < (halfn + l))
            inbandPower += 2.0*ps[j];
    }

    totalPower *= dx;
    inbandPower *= dx;
    return {totalPower, inbandPower};
}

std::optional<XARFCNScanResult> XARFCNScanner::scanARFCN(const GSMBand& band, uint16_t arfcn)
{
    XARFCNScanResult result;

    auto [ul, dl] = band.arfcnToFreq(arfcn);
    if(dl == 0.0)
        return std::optional<XARFCNScanResult>();

    result.ARFCN = arfcn;
    result.BandClass = band.bandClass();
    tuneAndWait(dl);

    size_t remaining = m_sampleBuffer.size();
    size_t offset = 0;

    ComplexArrayf tmpbuf(20000);

    auto rxStream = m_usrp->get_rx_stream(uhd::stream_args_t("fc32"));
    rxStream->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    while(remaining > 0) {
        uhd::rx_metadata_t md;
        auto rxsamps = rxStream->recv(&tmpbuf.front(), tmpbuf.size(), md, 3.0, false);
        if(md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW) {
            // just restart in case of overflow
            remaining = m_sampleBuffer.size();
            offset = 0;
            continue;
        }
        else if(md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
            throw std::runtime_error(md.strerror());
        }
        if(remaining < rxsamps)
            rxsamps = remaining;

        memcpy(&m_sampleBuffer[offset], &tmpbuf[0], sizeof(float)*2*rxsamps);
        offset += rxsamps;
        remaining -= rxsamps;
    }
    rxStream->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    rxStream.reset();

    auto wsize = windowSize();

    Arrayf welch(wsize, 0.0f);

    size_t niters = (m_numberOfSamples/wsize)*2 - 1;
    const float s12 = m_window.s1()*m_window.s1();
    const float scale = 1.0f/float(niters);
    for(size_t i = 0; i < niters; ++i) {
        windowSignal(ComplexArrayViewf(&m_sampleBuffer[i*wsize/2], wsize));
        fftwf_execute(m_fftPlan);

        for(size_t j = 0; j < wsize; ++j) {
            const auto re = m_fftOutBuffer[j].real();
            const auto im = m_fftOutBuffer[j].imag();
            const auto power = 2.0*(re*re+im*im)/s12;

            welch[j] += power*scale;
        }
    }

    result.MaxPower = *std::max_element(welch.begin(), welch.end());
    std::tie(result.TotalPower, result.InbandPower) = measurePowerRatio(welch);
    return std::optional<XARFCNScanResult>(result);
}

std::vector<XARFCNScanResult> XARFCNScanner::scan(GSMBandClass bandClass)
{
    const GSMBand& band = g_supportedBands.at(bandClass);

    std::vector<XARFCNScanResult> res;    
    for(auto arfcn = band.minArfcn(); arfcn <= band.maxArfcn(); ++arfcn) {
        auto scanResult = scanARFCN(band, arfcn);
        if(scanResult) {
            printf("%d - %.2f\n", scanResult->ARFCN, scanResult->InbandPower/scanResult->TotalPower*100.0);
            res.emplace_back(*scanResult);
        }
    }

    return res;
}