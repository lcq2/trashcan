#pragma once

#include "types.hpp"
#include <memory>

namespace gg {

class WindowFunction
{
public:
    WindowFunction(size_t sample_rate, size_t len, float a0 = 0.35875f, float a1 = 0.48829f, float a2 = 0.14128f, float a3 = 0.01168f)
        : m_samples(len), m_s1{0.0f}, m_s2{0.0f}, m_a0{a0}, m_a1{a1}, m_a2{a2}, m_a3{a3}
    {
        for(size_t n = 0; n < len; ++n) {
            const float t = float(n)/float(len - 1);
            const float s = a0
                - a1 * std::cos(2.0f * M_PI * t) 
                + a2 * std::cos(4.0f * M_PI * t) 
                - a3 * std::cos(6.0f * M_PI * t);
            // blackman-harris windowing function, will smooth out ripples in the FFT due to the obvious non-periodicity of captured signals...
            m_samples[n] = s;
            m_s1 += s;
            m_s2 += s*s;
        }
        m_enbw = sample_rate * (m_s2/(m_s1*m_s1));
    }

    WindowFunction(const WindowFunction&) = default;
    WindowFunction& operator=(const WindowFunction&) = default;
    
    WindowFunction(WindowFunction&& m)
        : m_samples{std::move(m.m_samples)}, 
            m_s1{m.m_s1}, m_s2{m.m_s2}, 
            m_a0{m.m_a0}, m_a1{m.m_a1}, m_a2{m.m_a2}, m_a3{m.m_a3},
            m_enbw{m.m_enbw}
    {

    }
    WindowFunction& operator=(WindowFunction&& m)
    {
        m_samples = std::move(m.m_samples);
        m_s1 = m.m_s1;
        m_s2 = m.m_s2;
        m_a0 = m.m_a0;
        m_a1 = m.m_a1;
        m_a2 = m.m_a2;
        m_a3 = m.m_a3;
        m_enbw = m.m_enbw;
        return *this;
    }

    size_t length() const { return m_samples.size(); }
    size_t size() const { return m_samples.size(); }

    constexpr float s1() const { return m_s1; }
    constexpr float s2() const { return m_s2; }
    constexpr float enbw() const { return m_enbw; }

    const Arrayf& samples() const { return m_samples; }

    float operator[](const std::size_t idx) const { return m_samples[idx]; }
    
private:
    Arrayf m_samples;
    float m_s1;
    float m_s2;
    float m_a0;
    float m_a1;
    float m_a2;
    float m_a3;
    float m_enbw;
};

}