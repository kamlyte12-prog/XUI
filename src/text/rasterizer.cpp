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

		struct Crossing { f32 x; i32 winding; };
		std::vector<Crossing> crossings; // crossings at one sample line (reused)
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
				crossings.clear();
				for (const Edge& e : edges) {
					f32 y0 = e.y0, y1 = e.y1;
					if ((sample < y0 && sample < y1) || (sample >= y0 && sample >= y1)) continue;
					f32 t = (sample - y0) / (y1 - y0);
					f32 x = e.x0 + t * (e.x1 - e.x0);
					crossings.push_back({ x, y1 > y0 ? 1 : -1 });
				}
				if (crossings.size() < 2) continue;
				std::sort(crossings.begin(), crossings.end(), [](const Crossing& a, const Crossing& b) { return a.x < b.x; });
				// TrueType outlines use the non-zero winding rule. Pairing crossings (even-odd fill)
				// incorrectly cuts holes into overlapping contours in variable and composite glyphs.
				i32 winding = 0;
				f32 previousX = crossings.front().x;
				for (std::size_t k = 0; k < crossings.size();) {
					const f32 x = crossings[k].x;
					if (winding != 0) {
						f32 xa = previousX, xb = x;
						if (xb > 0 && xa < f32(w)) {
							xa = std::max(xa, 0.f);
							xb = std::min(xb, f32(w));
							i32 ix0 = i32(xa), ix1 = i32(std::ceil(xb) - 1e-6f);
							for (i32 pixel = ix0; pixel <= ix1 && pixel < i32(w); ++pixel) {
								f32 covL = std::min(f32(pixel + 1), xb) - std::max(f32(pixel), xa);
								if (covL > 0) row[pixel] += covL / f32(kSubY);
							}
						}
					}
					std::size_t next = k;
					while (next < crossings.size() && std::abs(crossings[next].x - x) <= 1e-5f) {
						winding += crossings[next].winding;
						++next;
					}
					previousX = x;
					k = next;
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
