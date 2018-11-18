#pragma once

#include <cstdlib>
#include <new>

namespace xg {
  
template <class T, std::size_t Align>
struct AlignedAllocator
{
  typedef T value_type;
  AlignedAllocator() = default;
  template <class U> constexpr AlignedAllocator(const AlignedAllocator<U,Align>&) noexcept {}
  T* allocate(std::size_t n)
  {
    if(n > std::size_t(-1) / sizeof(T))
        throw std::bad_alloc();
    if(auto p = static_cast<T*>(::aligned_alloc(Align, n*sizeof(T))))
        return p;
    throw std::bad_alloc();
  }
  void deallocate(T* p, std::size_t) noexcept { std::free(p); }

  template<class U> struct rebind { typedef AlignedAllocator<U,Align> other; };  
};

template <class T, class U, std::size_t Align>
bool operator==(const AlignedAllocator<T,Align>&, const AlignedAllocator<U,Align>&) { return true; }
template <class T, class U, std::size_t Align>
bool operator!=(const AlignedAllocator<T,Align>&, const AlignedAllocator<U,Align>&) { return false; }

}