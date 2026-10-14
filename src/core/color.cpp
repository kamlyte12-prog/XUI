#include <xui/core/color.hpp>
#include <algorithm>
#include <cmath>
namespace xui {
namespace {
f32 toLinear(f32 c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
f32 toSrgb(f32 c) { return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.f / 2.4f) - 0.055f; }
f32 clamp01(f32 v) { return std::min(1.f, std::max(0.f, v)); }
int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
}
Color Color::fromRGBA8(u32 v) {
    return {((v >> 24) & 255) / 255.f, ((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f};
}
u32 Color::toRGBA8() const {
    auto q = [](f32 c) { return static_cast<u32>(clamp01(c) * 255.f + 0.5f); };
    return (q(r) << 24) | (q(g) << 16) | (q(b) << 8) | q(a);
}
bool Color::parseHex(std::string_view s, Color& out) {
    if (s.size() < 2 || s[0] != '#') return false;
    s.remove_prefix(1);
    u32 v[8]; std::size_t n = s.size();
    if (n != 3 && n != 4 && n != 6 && n != 8) return false;
    for (std::size_t i = 0; i < n; ++i) { int h = hexVal(s[i]); if (h < 0) return false; v[i] = static_cast<u32>(h); }
    u32 c[4] = {0, 0, 0, 255};
    if (n <= 4) for (std::size_t i = 0; i < n; ++i) c[i] = v[i] * 17;
    else for (std::size_t i = 0; i < n / 2; ++i) c[i] = v[2 * i] * 16 + v[2 * i + 1];
    out = fromRGBA8((c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3]);
    return true;
}
OKLab toOKLab(const Color& c) {
    f32 r = toLinear(c.r), g = toLinear(c.g), b = toLinear(c.b);
    f32 l = std::cbrt(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
    f32 m = std::cbrt(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
    f32 s = std::cbrt(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);
    return {0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s,
            1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s,
            0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s};
}
Color fromOKLab(const OKLab& o, f32 alpha) {
    f32 l = o.L + 0.3963377774f * o.a + 0.2158037573f * o.b;
    f32 m = o.L - 0.1055613458f * o.a - 0.0638541728f * o.b;
    f32 s = o.L - 0.0894841775f * o.a - 1.2914855480f * o.b;
    l = l * l * l; m = m * m * m; s = s * s * s;
    f32 r = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    f32 g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    f32 b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
    return {toSrgb(clamp01(r)), toSrgb(clamp01(g)), toSrgb(clamp01(b)), alpha};
}
Color Color::darken(f32 t) const { OKLab o = toOKLab(*this); o.L *= (1.f - t); return fromOKLab(o, a); }
Color Color::lighten(f32 t) const { OKLab o = toOKLab(*this); o.L += (1.f - o.L) * t; return fromOKLab(o, a); }
Color Color::saturate(f32 t) const { OKLab o = toOKLab(*this); o.a *= (1.f + t); o.b *= (1.f + t); return fromOKLab(o, a); }
Color Color::desaturate(f32 t) const { OKLab o = toOKLab(*this); o.a *= (1.f - t); o.b *= (1.f - t); return fromOKLab(o, a); }
Color Color::withAlpha(f32 alpha) const { Color c = *this; c.a = clamp01(alpha); return c; }
Color Color::mix(const Color& x, f32 t) const {
    OKLab p = toOKLab(*this), q = toOKLab(x);
    OKLab m{p.L + (q.L - p.L) * t, p.a + (q.a - p.a) * t, p.b + (q.b - p.b) * t};
    return fromOKLab(m, a + (x.a - a) * t);
}
}
