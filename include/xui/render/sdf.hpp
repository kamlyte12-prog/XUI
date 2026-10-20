#pragma once
/// @file sdf.hpp  CPU reference of the uber-shader's fragment stage. The GLSL in the GL backend is a line-for-line port.
///
/// Conventions: signed distance d is in physical pixels, negative inside. Coverage = clamp(0.5 - d, 0, 1), i.e. a
/// 1-pixel linear ramp centred on the edge. Output is PREMULTIPLIED RGBA. Blending is dst = src + dst*(1 - src.a).
#include <xui/render/drawlist.hpp>
#include <algorithm>
#include <cmath>

namespace xui::sdf {

	struct Rgba { f32 r = 0, g = 0, b = 0, a = 0; };
	inline Rgba unpack(u32 c) { return { f32((c >> 24) & 255) / 255.f, f32((c >> 16) & 255) / 255.f, f32((c >> 8) & 255) / 255.f, f32(c & 255) / 255.f }; }
	inline Rgba premul(Rgba c) { return { c.r * c.a, c.g * c.a, c.b * c.a, c.a }; }
	inline Rgba lerp(Rgba a, Rgba b, f32 t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t }; }
	inline Rgba scale(Rgba c, f32 s) { return { c.r * s, c.g * s, c.b * s, c.a * s }; }
	inline f32 clamp01(f32 v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
	inline f32 coverage(f32 d) { return clamp01(0.5f - d); }
	/// Soft edge for shadows: `blur` is the full width of the transition (px). Centred on the edge (d=0 -> 0.5).
	inline f32 falloff(f32 d, f32 blur) {
		if (blur <= 0.f) return coverage(d);
		f32 a = clamp01(0.5f - d / blur);
		return a * a * (3.f - 2.f * a);
	}

	/// Distance to a rounded rectangle centred at the origin with half size (hx,hy) and per-corner radii r[TL,TR,BR,BL].
	inline f32 roundRectDist(f32 px, f32 py, f32 hx, f32 hy, const f32 r[4]) {
		const f32 rad = px < 0 ? (py < 0 ? r[0] : r[3]) : (py < 0 ? r[1] : r[2]);
		const f32 qx = std::fabs(px) - hx + rad, qy = std::fabs(py) - hy + rad;
		return std::sqrt(std::max(qx, 0.f) * std::max(qx, 0.f) + std::max(qy, 0.f) * std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - rad;
	}
	/// Distance to a segment from (-hx,-hy) to (hx,hy) minus the radius (a capsule). Also returns t in [0,1] along the segment.
	inline f32 capsuleDist(f32 px, f32 py, f32 hx, f32 hy, f32 radius, f32& t) {
		const f32 ax = -hx, ay = -hy, bx = hx - ax, by = hy - ay, len2 = bx * bx + by * by;
		t = len2 > 0 ? clamp01(((px - ax) * bx + (py - ay) * by) / len2) : 0.f;
		const f32 dx = px - (ax + bx * t), dy = py - (ay + by * t);
		return std::sqrt(dx * dx + dy * dy) - radius;
	}

	/// Everything the fragment stage receives for one pixel (vertex attributes are flat except x,y,u,v, which interpolate).
	struct Frag {
		u8 prim = 0, flags = 0;
		f32 x = 0, y = 0;            ///< pixel centre
		f32 u = 0, v = 0;
		f32 hx = 0, hy = 0;
		f32 rad[4] = { 0, 0, 0, 0 };   ///< px
		f32 border = 0;              ///< px
		f32 aux0 = 0, aux1 = 0;
		u32 c0 = 0, c1 = 0, c2 = 0, c3 = 0;
	};
	/// Samples a texture at normalised (u,v). A8 textures return coverage in .a; RGBA8 return straight-alpha colour.
	struct SampleResult { f32 r = 1, g = 1, b = 1, a = 1; };

	template <class Sampler>
	Rgba shade(const Frag& f, Sampler&& sample) {
		const Rgba c0 = unpack(f.c0), c1 = unpack(f.c1);
		switch (static_cast<Prim>(f.prim)) {
		case Prim::RoundRect: {
			const f32 d = roundRectDist(f.u, f.v, f.hx, f.hy, f.rad);
			const f32 outer = coverage(d);
			if (f.flags & FlagGradient) {
				const f32 dx = f.aux0, dy = f.aux1;
				f32 t = 0.f;
				if (f.flags & FlagGradientConic) {
					t = std::atan2(f.v, f.u) / 6.28318530718f + 0.5f - dx / 6.28318530718f;
					t -= std::floor(t);
				}
				else if (f.flags & FlagGradientRadial) t = std::sqrt(f.u * f.u + f.v * f.v) / std::max(std::sqrt(f.hx * f.hx + f.hy * f.hy), 1e-4f);
				else {
					const f32 den = std::fabs(f.hx * dx) + std::fabs(f.hy * dy);
					t = den > 0 ? 0.5f + 0.5f * (f.u * dx + f.v * dy) / den : 0.f;
				}
				const int stopCount = (f.flags & FlagGradientFourStops) ? 4 : (f.flags & FlagGradientThreeStops) ? 3 : 2;
				t = clamp01(t) * f32(stopCount - 1);
				const Rgba stops[4] = { c0, c1, unpack(f.c2), unpack(f.c3) };
				const int segment = std::min(stopCount - 2, static_cast<int>(t));
				return scale(premul(lerp(stops[segment], stops[segment + 1], t - f32(segment))), outer);
			}
			if (f.border <= 0.f) return scale(premul(c0), outer);
			const f32 inner = coverage(d + f.border);           // 1 inside the fill region, 0 in the border band
			Rgba ring = premul(c1);
			if (f.flags & FlagDashed) {                          // dash coordinate runs along the nearest edge
				const f32 ex = f.hx - std::fabs(f.u), ey = f.hy - std::fabs(f.v);
				const f32 t = (ey < ex ? f.u + f.hx : f.v + f.hy) / std::max(f.aux0, 1e-3f);
				if (t - std::floor(t) >= 0.5f) ring = {};
			}
			const Rgba fill = premul(c0);
			return scale({ fill.r * inner + ring.r * (1 - inner), fill.g * inner + ring.g * (1 - inner), fill.b * inner + ring.b * (1 - inner), fill.a * inner + ring.a * (1 - inner) }, outer);
		}
		case Prim::Shadow: {
			const f32 d = roundRectDist(f.u, f.v, f.hx, f.hy, f.rad);
			f32 a;
			if (f.flags & FlagInner) {                           // darkness inside the shape, away from a shifted copy of itself
				const f32 hole = roundRectDist(f.u - f.aux0, f.v - f.aux1, f.hx, f.hy, f.rad);
				a = (1.f - falloff(hole, f.border)) * coverage(d);
			}
			else a = falloff(d, f.border);
			return scale(premul(c0), a);
		}
		case Prim::Glyph: {
			const SampleResult s = sample(f.u, f.v);
			return scale(premul(c0), s.a);
		}
		case Prim::Image: {
			const SampleResult s = sample(f.u, f.v);
			// Always masked (not only when rounded): a primitive must never touch pixels outside its declared bounds - the
			// batcher's overlap test relies on it - and it gives correctly anti-aliased edges at fractional positions.
			const f32 mask = coverage(roundRectDist(f.x - f.aux0, f.y - f.aux1, f.hx, f.hy, f.rad));
			const Rgba tex{ s.r * c0.r, s.g * c0.g, s.b * c0.b, s.a * c0.a };
			return scale(premul(tex), mask);
		}
		case Prim::Capsule: {
			f32 t;
			const f32 d = capsuleDist(f.u, f.v, f.hx, f.hy, f.rad[0], t);
			const Rgba col = (f.flags & FlagGradient) ? lerp(c0, c1, t) : c0;
			return scale(premul(col), coverage(d));
		}
		}
		return {};
	}
	/// Source-over for premultiplied colours.
	inline Rgba over(Rgba src, Rgba dst) { const f32 k = 1.f - src.a; return { src.r + dst.r * k, src.g + dst.g * k, src.b + dst.b * k, src.a + dst.a * k }; }
}
