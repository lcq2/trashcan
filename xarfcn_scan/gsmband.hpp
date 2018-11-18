#pragma once

#include <tuple>
#include <string>
#include <unordered_map>
#include "literals.hpp"

namespace gg {
    
enum class GSMBandClass : std::size_t
{
    Unknown,
    GSM450,
    GSM480,
    GSM750,
    GSM850,
    GSM900,
    GSM1800,
    GSM1900
};

class GSMBand
{
public:
    GSMBand() = default;

    GSMBand(GSMBandClass bandClass, std::string bandName, double baseFreq, uint16_t minArfcn, uint16_t maxArfcn) noexcept
        :   m_bandClass{bandClass}, 
            m_bandName{std::move(bandName)},
            m_baseFreq{baseFreq},
            m_minArfcn{minArfcn},
            m_maxArfcn{maxArfcn}
    { }

    GSMBandClass bandClass() const { return m_bandClass; }
    std::tuple<double,double> arfcnToFreq(uint16_t arfcn) const;
    constexpr const std::string& bandName() const { return m_bandName; }
    constexpr double baseFreq() const { return m_baseFreq; }
    constexpr uint16_t minArfcn() const { return m_minArfcn; }
    constexpr uint16_t maxArfcn() const { return m_maxArfcn; }

private:
    GSMBandClass m_bandClass;
    std::string m_bandName;
    double m_baseFreq;
    uint16_t m_minArfcn;
    uint16_t m_maxArfcn;

    std::tuple<double,double> arfcn_to_freq(uint16_t arfcn);
};

const std::unordered_map<GSMBandClass,GSMBand> g_supportedBands = {
    { GSMBandClass::GSM450, { GSMBandClass::GSM450, "GSM 450 (GSM 500)", 450.6_MHz, 259, 293 } },
    { GSMBandClass::GSM480, { GSMBandClass::GSM480, "GSM 480 (GSM 500)", 479.0_MHz, 306, 340 } },
    { GSMBandClass::GSM750, { GSMBandClass::GSM750, "GSM 750 (GSM 700)", 747.2_MHz, 438, 511 } },
    { GSMBandClass::GSM850, { GSMBandClass::GSM850, "GSM 850 (GSM 850)", 824.2_MHz, 128, 251 } },
    { GSMBandClass::GSM900, { GSMBandClass::GSM900, "GSM 900 (P-GSM/E-GSM/GSM-R)", 890.0_MHz, 0, 1023 } },
    { GSMBandClass::GSM1800, { GSMBandClass::GSM1800, "GSM 1800 (DCS 1800)", 1710.2_MHz, 512, 885 } },
    { GSMBandClass::GSM1900, { GSMBandClass::GSM1900, "GSM 1900 (PCS 1900)", 1850.2_MHz, 512, 810 } }
};

}