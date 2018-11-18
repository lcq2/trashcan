
#include <stdlib.h>
#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <fftw3.h>
#include <complex>
#include <tuple>
#include <iostream>

#include "xarfcnscanner.hpp"

int UHD_SAFE_MAIN(int argc, char *argv[])
{
    using namespace gg;

    uhd::set_thread_priority_safe();

    // rough approximation for the number of samples
    // needed to catch one TDMA burst (4.615ms), rounded to 4 samples
    // at a resolution of 1 MS/s
    // of course we could set this to 4096 or 8192 and speed up the fft
    // but that's not an issue
    constexpr const size_t samplesPerBurst = 4616;
    constexpr const double sampleRate = 1_MHz;
    double analogBandwidth = 210_kHz;

    // with an average over 500 power spectra the error will be ~ 4.5%
    // which is ok for what we need here...
    size_t niters = 500;

    // slightly more than 1 second of data
    size_t numSamples = samplesPerBurst*niters/2;

    XARFCNScanner scanner(sampleRate, numSamples, samplesPerBurst);
    scanner.initRadio();

    auto results = scanner.scan(GSMBandClass::GSM900);
    for(auto& res : results) {
        printf("%d - %.2f\n", res.ARFCN, res.InbandPower/res.TotalPower*100.0);
    }
    
    return EXIT_SUCCESS;
}