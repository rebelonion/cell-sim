#pragma once
#include <cstddef>
#include <new>
#include <cstdlib>

template <typename T, std::size_t Alignment>
class aligned_allocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    // Rebind allocator to type U
    template <typename U>
    struct rebind {
        using other = aligned_allocator<U, Alignment>;
    };

    aligned_allocator() = default;

    template <typename U>
    explicit aligned_allocator(const aligned_allocator<U, Alignment>&) noexcept {}

    static pointer allocate(const size_type n) {
        if (n == 0) return nullptr;

        if (void* ptr = std::aligned_alloc(Alignment, n * sizeof(T))) {
            return static_cast<pointer>(ptr);
        }

        throw std::bad_alloc();
    }

    static void deallocate(const pointer p, size_type) noexcept {
        std::free(p);
    }
};

template <typename T1, typename T2, std::size_t Alignment>
bool operator==(const aligned_allocator<T1, Alignment>&,
                const aligned_allocator<T2, Alignment>&) noexcept {
    return true;
}

template <typename T1, typename T2, std::size_t Alignment>
bool operator!=(const aligned_allocator<T1, Alignment>&,
                const aligned_allocator<T2, Alignment>&) noexcept {
    return false;
}