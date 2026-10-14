#pragma once
/// @file callback.hpp  Allocation-free callables and a small signal type (no std::function).
#include <xui/core/small_vector.hpp>
namespace xui {
template <class Sig> struct Callback;
/// Function pointer + context. Trivially copyable; safe to call in per-frame paths.
template <class R, class... A> struct Callback<R(A...)> {
    R (*fn)(void*, A...) = nullptr;
    void* ctx = nullptr;
    explicit operator bool() const { return fn != nullptr; }
    R operator()(A... a) const { return fn(ctx, a...); }
    /// Bind a member function: Callback<void(int)>::member<&Foo::onX>(&foo)
    template <auto Fn, class C> static Callback member(C* obj) {
        return {[](void* c, A... a) -> R { return (static_cast<C*>(c)->*Fn)(a...); }, obj};
    }
    friend bool operator==(const Callback&, const Callback&) = default;
};
/// Multicast list of callbacks (4 inline slots).
template <class... A> class Signal {
public:
    using Cb = Callback<void(A...)>;
    void connect(Cb cb) { m_cbs.push_back(cb); }
    bool disconnect(Cb cb) {
        for (std::size_t i = 0; i < m_cbs.size(); ++i)
            if (m_cbs[i] == cb) { m_cbs[i] = m_cbs.back(); m_cbs.pop_back(); return true; }
        return false;
    }
    void emit(A... a) const { for (const Cb& c : m_cbs) c(a...); }
    std::size_t size() const { return m_cbs.size(); }
private:
    SmallVector<Cb, 4> m_cbs;
};
}
