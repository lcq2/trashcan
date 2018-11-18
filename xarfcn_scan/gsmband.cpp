#include "gsmband.hpp"
#include "literals.hpp"
#include <boost/format.hpp>
using namespace gg;

std::tuple<double,double> GSMBand::arfcnToFreq(uint16_t arfcn) const
{
    double ul = 0.0;
    double dl = 0.0;
    assert(arfcn >= m_minArfcn && arfcn <= m_maxArfcn);
    if(arfcn < m_minArfcn || arfcn > m_maxArfcn)
        throw std::invalid_argument(boost::str(boost::format("arfcn (%d) outside range [%d,%d] for band %s") % arfcn % m_minArfcn % m_maxArfcn % m_bandName));

    if(m_bandClass == GSMBandClass::GSM900) {
        if(arfcn >= 0 && arfcn <= 124) {
            ul = m_baseFreq + 200_kHz*arfcn;
        }
        else if (arfcn >= 955 && arfcn <= 1023) {
            ul = m_baseFreq + 200_kHz*(arfcn - 1024);
        }
    }
    else {
        ul = m_baseFreq + 200_kHz*(arfcn - m_minArfcn);
    }

    switch(m_bandClass) {
        case GSMBandClass::GSM450:
        case GSMBandClass::GSM480:
            dl = ul + 10_MHz;
            break;

        case GSMBandClass::GSM750:
            dl = ul + 30_MHz;
            break;

        case GSMBandClass::GSM850:
        case GSMBandClass::GSM900:
            dl = ul + 45_MHz;
            break;

        case GSMBandClass::GSM1800:
            dl = ul + 95_MHz;
            break;

        case GSMBandClass::GSM1900:
            dl = ul + 80_MHz;
            break;
    }

    return {ul,dl};
}