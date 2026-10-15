#pragma once
// Shared by tests and the long-running fuzzer: a software-renderer environment with known textures, a DrawList validator,
// and a random scene generator that exercises every primitive, clips, layers and opacity.
#include <xui/render/painter.hpp>
#include <xui/render/soft_renderer.hpp>
#include <cmath>
#include <cstdio>
namespace paintfuzz {
	using namespace xui;
	// ---- test environment: a software renderer with a few known textures ------------------------------------
	struct Env {
		SoftRenderer r; TextureHandle checker, blobs, half, red;
		explicit Env(u32 w, u32 h, f32 dpi = 1.f) {
			r.init({ w, h, dpi });
			std::vector<u8> c(32 * 32 * 4);
			for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) { u8* p = &c[std::size_t(y * 32 + x) * 4]; const bool k = ((x / 8) + (y / 8)) & 1; p[0] = k ? 230 : 40; p[1] = u8(x * 8); p[2] = u8(y * 8); p[3] = 255; }
			checker = r.createTexture({ 32, 32, TextureFormat::RGBA8, c.data(), true });
			std::vector<u8> a(64 * 16);
			for (int g = 0; g < 4; ++g) for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
				const f32 dx = f32(x) - 7.5f, dy = f32(y) - 7.5f, d = std::sqrt(dx * dx + dy * dy);
				a[std::size_t(y * 64 + g * 16 + x)] = u8(std::fmax(0.f, std::fmin(1.f, (7.f - d) * f32(g + 1) * 0.5f)) * 255.f);
			}
			blobs = r.createTexture({ 64, 16, TextureFormat::A8, a.data(), true });
			std::vector<u8> h128(8 * 8, 128); half = r.createTexture({ 8, 8, TextureFormat::A8, h128.data(), true });
			std::vector<u8> rd(4 * 4 * 4); for (std::size_t i = 0; i < rd.size(); i += 4) { rd[i] = 255; rd[i + 3] = 255; }
			red = r.createTexture({ 4, 4, TextureFormat::RGBA8, rd.data(), true });
		}
		Image draw(Painter& p, Color clear = { 0, 0, 0, 0 }, f32 dpi = 1.f) {
			const DrawList dl = p.finish();
			r.beginFrame({ r.width(), r.height(), dpi, clear, 0 }); r.submit(dl); r.endFrame();
			return r.snapshot();
		}
	};
	// A DrawList is well formed if indices form quads (0,1,2,0,2,3) into valid vertices and commands tile the index buffer exactly.
	bool wellFormed(const DrawList& dl) {
		if (dl.nIdx % 6 != 0 || dl.nVerts % 4 != 0) return false;
		u32 expect = 0;
		for (u32 c = 0; c < dl.nCmds; ++c) { if (dl.cmds[c].idxOffset != expect) return false; expect += dl.cmds[c].idxCount; }
		if (expect != dl.nIdx) return false;
		for (u32 i = 0; i + 6 <= dl.nIdx; i += 6) {
			if (dl.idx[i] != dl.idx[i + 3] || dl.idx[i + 2] != dl.idx[i + 4] || dl.idx[i + 1] != dl.idx[i] + 1 || dl.idx[i + 2] != dl.idx[i] + 2 || dl.idx[i + 5] != dl.idx[i] + 3) return false;
			if (dl.idx[i + 5] >= dl.nVerts) return false;
		}
		return true;
	}
	struct Rng {
		u64 s; explicit Rng(u64 seed) : s(seed * 6364136223846793005ull + 1442695040888963407ull) {}
		u32 next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return u32(s >> 11); }
		f32 f(f32 lo, f32 hi) { return lo + (hi - lo) * f32(next() % 10000) / 10000.f; } int range(int lo, int hi) { return lo + int(next() % u32(hi - lo + 1)); } bool chance(int pct) { return int(next() % 100) < pct; }
	};

	void randomScene(Painter& p, Env& e, Rng& r, int n) {
		int clips = 0;
		for (int i = 0; i < n; ++i) {
			if (r.chance(8) && clips < 3) { p.pushClip({ r.f(0, 120), r.f(0, 90), r.f(20, 120), r.f(20, 90) }); ++clips; }
			else if (r.chance(8) && clips > 0) { p.popClip(); --clips; }
			if (r.chance(10)) p.setLayer(u32(r.range(0, 2)));
			if (r.chance(6)) p.pushOpacity(r.f(0.3f, 1)); else if (r.chance(6)) p.popOpacity();
			const Rect rc{ r.f(-10, 190), r.f(-10, 140), r.f(2, 70), r.f(2, 50) };
			const Color col{ r.f(0, 1), r.f(0, 1), r.f(0, 1), r.f(0.3f, 1) };
			switch (r.range(0, 6)) {
			case 0: p.fillRect(rc, col, CornerRadii::all(r.f(0, 12))); break;
			case 1: { RectStyle s; s.fill = col; s.border = { r.f(0, 1), r.f(0, 1), r.f(0, 1), 1 }; s.borderWidth = r.f(0.5f, 3); s.radius = { r.f(0, 8), r.f(0, 8), r.f(0, 8), r.f(0, 8) }; s.dashed = r.chance(30); p.roundRect(rc, s); break; }
			case 2: p.shadow(rc, { col, {r.f(-3, 3), r.f(-3, 3)}, r.f(2, 14), r.f(-2, 3), CornerRadii::all(r.f(0, 8)), r.chance(25) }); break;
			case 3: { GlyphQuad q[3]; for (int k = 0; k < 3; ++k) q[k] = { {f32(k) * 9.f, 0, 8, 8}, {f32(k) * .25f, 0, .25f, 1} }; p.glyphRun(e.blobs, q, 3, { rc.x, rc.y }, col); break; }
			case 4: p.image(r.chance(50) ? e.checker : e.red, rc, { 0, 0, 1, 1 }, { 1, 1, 1, col.a }, CornerRadii::all(r.f(0, 10))); break;
			case 5: p.line({ rc.x, rc.y }, { rc.x + rc.w, rc.y + rc.h }, r.f(1, 6), col); break;
			case 6: { RectStyle s; s.gradient = true; s.fill = col; s.gradientEnd = { col.b, col.r, col.g, 1 }; s.gradientAngle = r.f(0, 6); s.radius = CornerRadii::all(r.f(0, 10)); p.roundRect(rc, s); break; }
			}
		}
		while (clips-- > 0) p.popClip();
	}

	/// Renders the same random scene with and without reordering; returns "" if pixel-identical and well-formed.
	inline std::string reorderCase(Env& e, u64 seed, int prims, u32* callsOn = nullptr, u32* callsOff = nullptr) {
		Image img[2]; u32 calls[2];
		for (int mode = 0; mode < 2; ++mode) {
			Rng r(seed); Painter p(Painter::Options{ mode == 0, true });
			p.begin(200, 150, 1); randomScene(p, e, r, prims);
			const DrawList dl = p.finish();
			if (!wellFormed(dl)) return "malformed DrawList (mode " + std::to_string(mode) + ")";
			img[mode] = e.draw(p, { 0.1f, 0.1f, 0.12f, 1 }); calls[mode] = p.stats().drawCalls;
		}
		if (callsOn) *callsOn = calls[0];
		if (callsOff) *callsOff = calls[1];
		const ImageDiff d = compareImages(img[0], img[1], 0);
		if (d.badPixels) return std::to_string(d.badPixels) + " pixels differ (first " + std::to_string(d.firstX) + "," + std::to_string(d.firstY) + ", max delta " + std::to_string(d.maxDelta) + ")";
		if (calls[0] > calls[1]) return "reordering produced MORE draw calls";
		return {};
	}
}
