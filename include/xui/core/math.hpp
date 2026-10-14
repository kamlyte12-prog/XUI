#pragma once
/// @file math.hpp  Small POD math types: Vec2, Vec4, Rect, Mat3 (2D affine).
#include <xui/core/types.hpp>
#include <algorithm>
#include <cmath>
namespace xui {
struct Vec2 {
    f32 x = 0, y = 0;
    friend constexpr Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
    friend constexpr Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
    friend constexpr Vec2 operator*(Vec2 a, f32 s) { return {a.x * s, a.y * s}; }
    friend constexpr bool operator==(Vec2, Vec2) = default;
};
struct Vec4 { f32 x = 0, y = 0, z = 0, w = 0; friend constexpr bool operator==(Vec4, Vec4) = default; };
struct Vec2i {
    i32 x = 0, y = 0;
    friend constexpr Vec2i operator+(Vec2i a, Vec2i b) { return {a.x + b.x, a.y + b.y}; }
    friend constexpr Vec2i operator-(Vec2i a, Vec2i b) { return {a.x - b.x, a.y - b.y}; }
    friend constexpr bool operator==(Vec2i, Vec2i) = default;
};
struct Rect {
    f32 x = 0, y = 0, w = 0, h = 0;
    constexpr f32 right() const { return x + w; }
    constexpr f32 bottom() const { return y + h; }
    constexpr bool contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
    constexpr bool empty() const { return w <= 0 || h <= 0; }
    constexpr bool intersects(const Rect& o) const { return x < o.right() && o.x < right() && y < o.bottom() && o.y < bottom(); }
    Rect intersect(const Rect& o) const {
        f32 nx = std::max(x, o.x), ny = std::max(y, o.y);
        f32 r = std::min(right(), o.right()), b = std::min(bottom(), o.bottom());
        return {nx, ny, std::max(0.f, r - nx), std::max(0.f, b - ny)};
    }
    friend constexpr bool operator==(const Rect&, const Rect&) = default;
};
/// Row-major 2D affine: [a c tx; b d ty; 0 0 1]
struct Mat3 {
    f32 a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
    static constexpr Mat3 translate(f32 x, f32 y) { Mat3 m; m.tx = x; m.ty = y; return m; }
    static constexpr Mat3 scale(f32 sx, f32 sy) { Mat3 m; m.a = sx; m.d = sy; return m; }
    static Mat3 rotate(f32 rad) { Mat3 m; m.a = std::cos(rad); m.b = std::sin(rad); m.c = -m.b; m.d = m.a; return m; }
    /// this * o : applies o first, then this.
    constexpr Mat3 operator*(const Mat3& o) const {
        return {a * o.a + c * o.b, b * o.a + d * o.b, a * o.c + c * o.d, b * o.c + d * o.d,
                a * o.tx + c * o.ty + tx, b * o.tx + d * o.ty + ty};
    }
    constexpr Vec2 apply(Vec2 p) const { return {a * p.x + c * p.y + tx, b * p.x + d * p.y + ty}; }
};
}
