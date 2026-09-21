#pragma once

#include <cstddef>
#include <type_traits>
#include <vector>

namespace Engine::Base {

    // C++17 用の軽量 span（std::span の代替）
    // - 非所有 (non-owning)
    // - pointer + size
    template <class T>
    class Span final {
    public:
        using element_type = T;
        using value_type = std::remove_cv_t<T>;
        using pointer = T*;
        using reference = T&;
        using iterator = T*;
        using size_type = std::size_t;

        constexpr Span() noexcept : data_(nullptr), size_(0) {}
        constexpr Span(pointer p, size_type n) noexcept : data_(p), size_(n) {}

        template <std::size_t N>
        constexpr Span(T (&arr)[N]) noexcept : data_(arr), size_(N) {}

        template <class Alloc>
        Span(std::vector<value_type, Alloc>& v) noexcept : data_(v.data()), size_(v.size()) {}

        template <class Alloc>
        Span(const std::vector<value_type, Alloc>& v) noexcept
            : data_(const_cast<pointer>(v.data())), size_(v.size()) {}

        constexpr pointer data() const noexcept { return data_; }
        constexpr size_type size() const noexcept { return size_; }
        constexpr bool empty() const noexcept { return size_ == 0; }

        constexpr iterator begin() const noexcept { return data_; }
        constexpr iterator end() const noexcept { return data_ + size_; }

        constexpr reference operator[](size_type i) const noexcept { return data_[i]; }

        constexpr Span<T> subspan(size_type offset, size_type count = static_cast<size_type>(-1)) const noexcept {
            if (offset > size_) return Span<T>{};
            const size_type remaining = size_ - offset;
            const size_type n = (count == static_cast<size_type>(-1) || count > remaining) ? remaining : count;
            return Span<T>{ data_ + offset, n };
        }

    private:
        pointer data_;
        size_type size_;
    };

    template <class T>
    using ConstSpan = Span<const T>;

} // namespace Engine::Asset::Detail
