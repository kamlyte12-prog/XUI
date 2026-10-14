#pragma once
/// @file color.hpp  Linear-agnostic sRGB float colour with OKLab-based manipulation.
#include <xui/core/types.hpp>
#include <string_view>
namespace xui {
struct Color {
    f32 r = 0, g = 0, b = 0, a = 1;
    static Color fromRGBA8(u32 rgba);           ///< 0xRRGGBBAA
    u32 toRGBA8() const;                        ///< clamped, rounded
    /// Parses #rgb #rgba #rrggbb #rrggbbaa (leading '#' required).
    static bool parseHex(std::string_view s, Color& out);
    // All manipulations happen in OKLab (perceptual); amounts are 0..1.
    Color darken(f32 amount) const;      ///< L *= (1-amount)
    Color lighten(f32 amount) const;     ///< L += (1-L)*amount
    Color saturate(f32 amount) const;    ///< chroma *= (1+amount)
    Color desaturate(f32 amount) const;  ///< chroma *= (1-amount)
    Color withAlpha(f32 alpha) const;
    Color mix(const Color& other, f32 t) const;   ///< t=0 -> this, t=1 -> other
    friend bool operator==(const Color&, const Color&) = default;
};
struct OKLab { f32 L, a, b; };
OKLab toOKLab(const Color& c);
Color fromOKLab(const OKLab& l, f32 alpha);
}
