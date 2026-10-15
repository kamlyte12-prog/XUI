#include <xui/text/rasterizer.hpp>
#include <algorithm>
#include <cmath>

namespace xui {

	// 4 vertical samples per pixel row; horizontal AA is analytic (fractional span coverage).
	static constexpr int kSubY = 4;

	void rasterizeContours(const Vec2* pts, u32 n, const u32* contourStarts, u32 contourCount,
		u8* bits, u32 w, u32 h, std::vector<f32>& scratch) {
		std::fill(bits, bits + std::size_t(w) * h, u8(0));
		if (!n || !contourCount || !w || !h) return;

		// acc[y][x] accumulates coverage in 1/16 units (sum of kSubY subsample coverages, each 0..4)
		scratch.assign(std::size_t(w) * h, 0.f);
		f32* acc = scratch.data();

		std::vector<f32> xs;   // crossings at one sample line (reused)
		// Build a flat edge list: consecutive points within each contour, plus the closing segment.
		struct Edge { f32 x0, y0, x1, y1; };
		std::vector<Edge> edges;
		edges.reserve(n);
		for (u32 ci = 0; ci < contourCount; ++ci) {
			u32 s = contourStarts[ci];
			u32 e = ci + 1 < contourCount ? contourStarts[ci + 1] : n;
			if (e <= s + 1) continue;
			for (u32 j = s; j < e; ++j) {
				const Vec2& a = pts[j];
				const Vec2& b = pts[j + 1 < e ? j + 1 : s];
				if (a.y != b.y) edges.push_back({ a.x, a.y, b.x, b.y });
			}
		}
		if (edges.empty()) return;

		f32 yMax = f32(h);
		for (u32 py = 0; py < h; ++py) {
			f32* row = acc + std::size_t(py) * w;
			for (int sub = 0; sub < kSubY; ++sub) {
				f32 sample = f32(py) + (f32(sub) + 0.5f) / f32(kSubY);
				if (sample >= yMax) break;
				xs.clear();
				for (const Edge& e : edges) {
					f32 y0 = e.y0, y1 = e.y1;
					if ((sample < y0 && sample < y1) || (sample >= y0 && sample >= y1)) continue;
					f32 t = (sample - y0) / (y1 - y0);
					f32 x = e.x0 + t * (e.x1 - e.x0);
					xs.push_back(x);
				}
				if (xs.size() < 2) continue;
				std::sort(xs.begin(), xs.end());
				// Nonzero winding for simple outlines == fill between crossing pairs (contours are closed
				// and non-self-crossing in practice; overlaps still composite sanely for UI glyphs).
				for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
					f32 xa = xs[k], xb = xs[k + 1];
					if (xb <= 0 || xa >= f32(w)) continue;
					xa = std::max(xa, 0.f);
					xb = std::min(xb, f32(w));
					i32 ix0 = i32(xa), ix1 = i32(std::ceil(xb) - 1e-6f);
					for (i32 x = ix0; x <= ix1 && x < i32(w); ++x) {
						f32 covL = std::min(f32(x + 1), xb) - std::max(f32(x), xa);   // overlap with pixel x
						if (covL > 0) row[x] += covL / f32(kSubY);
					}
				}
			}
		}
		for (std::size_t i = 0; i < std::size_t(w) * h; ++i) {
			f32 c = std::clamp(acc[i], 0.f, 1.f);
			// 1 vertical subsample only approximates AA at extreme slants; sharpen the midpoint ramp.
			bits[i] = u8(c * 255.f + 0.5f);
		}
	}

} // namespace xui
