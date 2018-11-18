#pragma once

constexpr double operator""_kHz(long double v)
{
    return double(v)*1e3;
}
constexpr double operator""_kHz(unsigned long long v)
{
    return double(v)*1e3;
}


constexpr double operator""_MHz(long double v)
{
    return double(v)*1e6;
}
constexpr double operator""_MHz(unsigned long long v)
{
    return double(v)*1e6;
}

constexpr double operator""_GHz(long double v)
{
    return v*1e9;
}
constexpr double operator""_GHz(unsigned long long v)
{
    return v*1e9;
}