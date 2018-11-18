#pragma once
#include <vector>
#include <complex>

#include "alignedallocator.hpp"
#include "arrayview.hpp"

#define SIMD_ALIGNMENT 32

namespace gg {

template<typename T>
using ComplexArray = std::vector<std::complex<T>, AlignedAllocator<std::complex<T>, SIMD_ALIGNMENT>>;

template<typename T>
using Array = std::vector<T, AlignedAllocator<T, SIMD_ALIGNMENT>>;

template<typename T>
using ComplexArrayView = ArrayView<std::complex<T>>;

using ComplexArrayf = ComplexArray<float>;
using ComplexArrayd = ComplexArray<double>;

using Arrayf = Array<float>;
using Arrayd = Array<double>;

using ComplexArrayViewf = ComplexArrayView<float>;
using ComplexArrayViewd = ComplexArrayView<double>;

}