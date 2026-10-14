#pragma once
/// @file small_vector.hpp  Vector with N inline slots; spills to the heap only when exceeded.
#include <xui/core/types.hpp>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <utility>
namespace xui {
template <class T, std::size_t N>
class SmallVector {
public:
    SmallVector() = default;
    SmallVector(std::initializer_list<T> il) { reserve(il.size()); for (const T& v : il) push_back(v); }
    SmallVector(const SmallVector& o) { reserve(o.m_size); for (const T& v : o) push_back(v); }
    SmallVector(SmallVector&& o) noexcept { steal(o); }
    SmallVector& operator=(const SmallVector& o) {
        if (this != &o) { clear(); reserve(o.m_size); for (const T& v : o) push_back(v); }
        return *this;
    }
    SmallVector& operator=(SmallVector&& o) noexcept {
        if (this != &o) { clear(); release(); steal(o); }
        return *this;
    }
    ~SmallVector() { clear(); release(); }

    T* data() { return m_heap ? m_heap : inl(); }
    const T* data() const { return m_heap ? m_heap : inl(); }
    std::size_t size() const { return m_size; }
    std::size_t capacity() const { return m_heap ? m_cap : N; }
    bool empty() const { return m_size == 0; }
    bool onHeap() const { return m_heap != nullptr; }
    T& operator[](std::size_t i) { return data()[i]; }
    const T& operator[](std::size_t i) const { return data()[i]; }
    T& back() { return data()[m_size - 1]; }
    T* begin() { return data(); }             T* end() { return data() + m_size; }
    const T* begin() const { return data(); } const T* end() const { return data() + m_size; }

    void reserve(std::size_t n) {
        if (n <= capacity()) return;
        std::size_t nc = capacity() * 2 > n ? capacity() * 2 : n;
        T* nh = static_cast<T*>(std::malloc(nc * sizeof(T)));
        if (!nh) std::abort();
        for (std::size_t i = 0; i < m_size; ++i) { ::new (nh + i) T(std::move(data()[i])); data()[i].~T(); }
        release();
        m_heap = nh; m_cap = nc;
    }
    template <class... A> T& emplace_back(A&&... a) {
        if (m_size == capacity()) reserve(m_size + 1);
        T* p = ::new (data() + m_size) T(std::forward<A>(a)...);
        ++m_size; return *p;
    }
    void push_back(const T& v) { emplace_back(v); }
    void push_back(T&& v) { emplace_back(std::move(v)); }
    void pop_back() { data()[--m_size].~T(); }
    void clear() { for (std::size_t i = 0; i < m_size; ++i) data()[i].~T(); m_size = 0; }
private:
    T* inl() { return std::launder(reinterpret_cast<T*>(m_inline)); }
    const T* inl() const { return std::launder(reinterpret_cast<const T*>(m_inline)); }
    void release() { if (m_heap) { std::free(m_heap); m_heap = nullptr; m_cap = 0; } }
    void steal(SmallVector& o) {
        if (o.m_heap) { m_heap = o.m_heap; m_cap = o.m_cap; m_size = o.m_size; o.m_heap = nullptr; o.m_cap = 0; o.m_size = 0; }
        else { for (std::size_t i = 0; i < o.m_size; ++i) { ::new (inl() + i) T(std::move(o.inl()[i])); } m_size = o.m_size; o.clear(); }
    }
    alignas(T) unsigned char m_inline[sizeof(T) * N];
    T* m_heap = nullptr;
    std::size_t m_size = 0, m_cap = 0;
};
}
