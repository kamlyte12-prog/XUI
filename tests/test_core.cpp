#include "xtest.hpp"
#include <xui/core/arena.hpp>
#include <xui/core/callback.hpp>
#include <xui/core/color.hpp>
#include <xui/core/diagnostic.hpp>
#include <xui/core/handle.hpp>
#include <xui/core/interner.hpp>
#include <xui/core/math.hpp>
#include <xui/core/small_vector.hpp>
#include <string>
using namespace xui;

TEST(handle_pool_detects_stale_handles) {
    SlotPool<int, struct T1> p;
    auto a = p.create(7);
    CHECK(p.get(a) && *p.get(a) == 7);
    CHECK(p.destroy(a));
    CHECK(p.get(a) == nullptr);            // stale
    auto b = p.create(9);                  // reuses slot, new generation
    CHECK_EQ(b.index, a.index);
    CHECK(b.gen != a.gen);
    CHECK(p.get(a) == nullptr && *p.get(b) == 9);
    CHECK(!p.destroy(a));
}
TEST(handle_pool_addresses_are_stable) {
    SlotPool<int, struct T2, 4> p;
    auto first = p.create(1); int* addr = p.get(first);
    for (int i = 0; i < 100; ++i) p.create(i);
    CHECK(p.get(first) == addr);
    CHECK_EQ(p.size(), 101u);
}
TEST(arena_bump_and_overflow) {
    FrameArena a(64);
    void* p1 = a.alloc(10, 1); void* p2 = a.alloc(8, 8);
    CHECK(p1 && p2 && reinterpret_cast<std::uintptr_t>(p2) % 8 == 0);
    CHECK(a.alloc(1000, 1) == nullptr);
    CHECK_EQ(a.overflows(), 1u);
    a.reset(); CHECK_EQ(a.used(), 0u);
}
TEST(small_vector_inline_then_heap) {
    SmallVector<std::string, 2> v;
    v.push_back("a"); v.push_back("b");
    CHECK(!v.onHeap());
    v.push_back("this string is long enough to avoid SSO entirely");
    CHECK(v.onHeap() && v.size() == 3 && v[0] == "a" && v[2].size() > 20);
    SmallVector<std::string, 2> w = v; SmallVector<std::string, 2> m = std::move(v);
    CHECK(w.size() == 3 && m.size() == 3 && v.size() == 0);
}
TEST(interner_dedupes) {
    StringInterner in;
    StrId a = in.intern("hello"), b = in.intern("hello"), c = in.intern("world");
    CHECK(a == b && a != c && in.view(a) == "hello");
    CHECK(in.intern("").v == 0);
    bool found = true; in.find("nope", &found); CHECK(!found);
}
struct Counter { int n = 0; void hit(int d) { n += d; } };
TEST(signal_calls_member_callbacks) {
    Counter c; Signal<int> s;
    auto cb = Callback<void(int)>::member<&Counter::hit>(&c);
    s.connect(cb); s.emit(3); s.emit(4);
    CHECK_EQ(c.n, 7);
    CHECK(s.disconnect(cb)); s.emit(100); CHECK_EQ(c.n, 7);
}
TEST(color_hex_forms) {
    Color c;
    CHECK(Color::parseHex("#0b0b0b", c) && c.toRGBA8() == 0x0b0b0bffu);
    CHECK(Color::parseHex("#f00", c) && c.toRGBA8() == 0xff0000ffu);
    CHECK(Color::parseHex("#00000066", c) && c.toRGBA8() == 0x00000066u);
    CHECK(!Color::parseHex("#12", c)); CHECK(!Color::parseHex("#gggggg", c)); CHECK(!Color::parseHex("abc", c));
}
TEST(color_oklab_roundtrip_and_ops) {
    Color c = Color::fromRGBA8(0x3b82f6ff);
    Color r = fromOKLab(toOKLab(c), 1.f);
    CHECK_NEAR(r.r, c.r, 0.002); CHECK_NEAR(r.g, c.g, 0.002); CHECK_NEAR(r.b, c.b, 0.002);
    CHECK(toOKLab(c.darken(0.2f)).L < toOKLab(c).L);
    CHECK(toOKLab(c.lighten(0.2f)).L > toOKLab(c).L);
    Color black{0, 0, 0, 1}, white{1, 1, 1, 1};
    CHECK_NEAR(toOKLab(black.mix(white, 0.5f)).L, 0.5, 0.01);
    CHECK_NEAR(c.withAlpha(0.25f).a, 0.25, 1e-6);
}
TEST(math_rect_and_mat3) {
    Rect a{0, 0, 10, 10}, b{5, 5, 10, 10};
    CHECK(a.intersects(b) && a.intersect(b) == (Rect{5, 5, 5, 5}));
    CHECK(!a.intersects(Rect{10, 0, 5, 5}));
    Mat3 m = Mat3::translate(10, 0) * Mat3::scale(2, 2);
    CHECK(m.apply({1, 1}) == (Vec2{12, 2}));
}
TEST(diagnostic_format_has_caret_snippet) {
    DiagnosticList d("a.xss", "Button {\n  bgg: red\n}\n");
    d.error(11, 3, "unknown property 'bgg'", "did you mean 'bg'?");
    std::string s = d.format();
    CHECK(s.find("a.xss:2:3: error: unknown property 'bgg'") != std::string::npos);
    CHECK(s.find("  bgg: red") != std::string::npos);
    CHECK(s.find("^~~") != std::string::npos);
    CHECK(s.find("did you mean 'bg'?") != std::string::npos);
}
TEST(did_you_mean) {
    std::string_view c[] = {"placeholder", "value", "label"};
    CHECK_EQ(didYouMean("palceholder", c), std::string("placeholder"));
    CHECK(didYouMean("zzzzzz", c).empty());
}
