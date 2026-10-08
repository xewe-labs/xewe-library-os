// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Utils/Span.h
#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#if defined(__has_include)
#  if __has_include(<span>)
#    include <span>
#  endif
#endif

#if defined(__cpp_lib_span)
#  include <span>
#endif


namespace xewe {

#if defined(__cpp_lib_span)

// C++20 and up: the real thing.
template <typename T>
using span = std::span<T>;

#else

// C++17 fallback: a non-owning view over contiguous elements, carrying only the
// operations the XeWe libraries use (size, empty, data, indexing, iteration).
// Enough to keep the public signatures identical on a C++17 core; not a
// std::span implementation.
template <typename T>
class span {
public:
    using element_type    = T;
    using value_type      = typename std::remove_cv<T>::type;
    using size_type       = std::size_t;
    using pointer         = T*;
    using reference       = T&;
    using iterator        = T*;
    using const_iterator  = const T*;

    constexpr span        () noexcept = default;
    constexpr span        (T* first, size_type count) noexcept
        : ptr(first)
        , len(count) {}

    // Implicit from any contiguous container (vector, array) or C array, so call
    // sites read the same as they do with std::span.
    template <typename C, typename = decltype(static_cast<T*>(std::declval<C&>().data()))>
    constexpr span        (C& c) noexcept
        : ptr(c.data())
        , len(c.size()) {}

    template <typename C, typename = decltype(static_cast<T*>(std::declval<const C&>().data()))>
    constexpr span        (const C& c) noexcept
        : ptr(c.data())
        , len(c.size()) {}

    template <std::size_t N>
    constexpr span        (T (&arr)[N]) noexcept
        : ptr(arr)
        , len(N) {}

    constexpr size_type   size     () const noexcept { return len; }
    constexpr bool        empty    () const noexcept { return len == 0; }
    constexpr pointer     data     () const noexcept { return ptr; }
    constexpr reference   operator[](size_type i) const { return ptr[i]; }
    constexpr iterator    begin    () const noexcept { return ptr; }
    constexpr iterator    end      () const noexcept { return ptr + len; }

private:
    pointer               ptr = nullptr;
    size_type             len = 0;
};

#endif

} // namespace xewe
