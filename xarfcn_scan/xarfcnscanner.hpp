#pragma once
#include <fftw3.h>
#include <uhd/usrp/multi_usrp.hpp>
#include <string>
#include <optional>
#include "windowfunction.hpp"
#include "gsmband.hpp"
#include "literals.hpp"

namespace gg {

struct XARFCNRadioArguments
{
    std::string DeviceArgs;
    std::string PreferredClockSource = "gpsdo";
    std::string PreferredAntenna = "RX2";
    double SampleRate = 1_MHz;
    double AnalogBandwidth = 210_kHz;
    double Gain = 20;
};

struct XARFCNScanResult
{
    GSMBandClass BandClass = GSMBandClass::Unknown;
    uint16_t ARFCN = 0;
    double TotalPower = 0.0;
    double InbandPower = 0.0;
    double MaxPower = 0.0;
};

class XARFCNScanner
{
public:
    XARFCNScanner(double rate, size_t numsamples, size_t winsize);
    XARFCNScanner(const XARFCNScanner&) = delete;

    XARFCNScanner& operator=(const XARFCNScanner&) = delete;

    void initRadio(const XARFCNRadioArguments& args = XARFCNRadioArguments());
    std::vector<XARFCNScanResult> scan(GSMBandClass bandClass = GSMBandClass::GSM900);

    size_t windowSize() const { return m_window.size(); }
    double sampleRate() const { return m_sampleRate; }
    size_t numberOfSamples() const { return m_numberOfSamples; }

    const WindowFunction& window() const { return m_window; }

private:
    std::optional<XARFCNScanResult> scanARFCN(const GSMBand& band, uint16_t arfcn);
    void tuneAndWait(double freq);
    void windowSignal(const ComplexArrayViewf& signal);
    std::tuple<double,double> measurePowerRatio(const Arrayf& ps);

private:
    double m_sampleRate;
    size_t m_numberOfSamples;
    ComplexArrayf m_sampleBuffer;
    ComplexArrayf m_fftInBuffer;
    ComplexArrayf m_fftOutBuffer;
    WindowFunction m_window;
    fftwf_plan m_fftPlan;
    uhd::usrp::multi_usrp::sptr m_usrp;
};

}