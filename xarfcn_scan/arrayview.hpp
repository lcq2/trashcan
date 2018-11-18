#pragma once

#include <stdexcept>

namespace gg
{

template<typename T>
class ArrayView
{
  public:
    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using const_iterator = const T *;
    using iterator = T *;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    constexpr ArrayView() : mPtr{nullptr}, mLen{0} {}
    constexpr ArrayView(const ArrayView &) = default;
    constexpr ArrayView(T *buf, size_t len)
        : mPtr(buf), mLen(len)
    {
    }

    ArrayView &operator=(const ArrayView &) noexcept = default;

    iterator
    begin() noexcept
    {
        return this->mPtr;
    }

    constexpr const_iterator
    begin() const noexcept
    {
        return this->mPtr;
    }

    iterator
    end() noexcept
    {
        return this->mPtr + this->mLen;
    }

    constexpr const_iterator
    end() const noexcept
    {
        return this->mPtr + this->mLen;
    }

    constexpr const_iterator
    cbegin() const noexcept
    {
        return this->mPtr;
    }

    constexpr const_iterator
    cend() const noexcept
    {
        return this->mPtr + this->mLen;
    }

    reverse_iterator
    rbegin() noexcept
    {
        return reverse_iterator(this->end());
    }

    const_reverse_iterator
    rebegin() const noexcept
    {
        return const_reverse_iterator(this->end());
    }

    reverse_iterator
    rend() noexcept
    {
        return reverse_iterator(this->begin());
    }

    const_reverse_iterator
    rend() const noexcept
    {
        return const_reverse_iterator(this->begin());
    }

    const_reverse_iterator
    crbegin() const noexcept
    {
        return const_reverse_iterator(this->end());
    }

    const_reverse_iterator
    crend() const noexcept
    {
        return const_reverse_iterator(this->begin());
    }

    constexpr size_type
    size() const noexcept
    {
        return mLen;
    }

    constexpr size_type
    length() const noexcept
    {
        return mLen;
    }

    constexpr bool
    empty() const noexcept
    {
        return this->mLen == 0;
    }

    constexpr const T &
    operator[](size_type __pos) const noexcept
    {
        return *(this->mPtr + __pos);
    }

    T &operator[](size_type __pos) noexcept
    {
        return *(this->mPtr + __pos);
    }

    constexpr const T &
    at(size_type __pos) const
    {
        return __pos < this->mLen
                   ? *(this->mPtr + __pos)
                   : (throw std::out_of_range("ArrayView::at"));
    }

    constexpr const T *data() const { return mPtr; }

  private:
    T *mPtr;
    size_t mLen;
};

}