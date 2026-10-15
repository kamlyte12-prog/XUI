#pragma once
// Differential layout fuzzer. Builds random trees, applies random mutations, runs the INCREMENTAL layout, then
// clones the tree into a fresh LayoutTree and runs a FULL layout. The two must agree. A mismatch means a dirty flag,
// relayout boundary or measure-cache entry went stale - bugs that otherwise produce plausible-looking wrong output.
#include <xui/layout/layout.hpp>
#include <cmath>
#include <cstdio>
#include <string>
namespace layoutfuzz {
	using namespace xui;

	struct Rng {
		u64 s;
		explicit Rng(u64 seed) : s(seed * 2654435761u + 88172645463325252ull) {}
		u32 next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return u32(s >> 11); }
		int range(int lo, int hi) { return lo + int(next() % u32(hi - lo + 1)); }
		f32 f(f32 lo, f32 hi) { return lo + (hi - lo) * f32(next() % 10000) / 10000.f; }
		bool chance(int pct) { return int(next() % 100) < pct; }
	};
	// Feature gate: regression seeds replay with the generator exactly as it was when they were found (basis off).
	inline bool gUseBasis = true;
	struct LeafCtx { std::vector<Size> size; std::vector<u8> wrap; };
	inline Size leafCb(void* c, NodeId id, Constraint k) {
		const LeafCtx& L = *static_cast<LeafCtx*>(c);
		Size s = id.index < L.size.size() ? L.size[id.index] : Size{ 10, 10 };
		if (id.index < L.wrap.size() && L.wrap[id.index] && std::isfinite(k.w) && s.w > 0) {   // "text": height follows width
			f32 w = std::min(s.w, std::max(k.w, 1.f)); return { w, s.h * std::ceil(s.w / w) };
		}
		return s;
	}
	inline void ensure(LeafCtx& L, u32 i) { if (L.size.size() <= i) { L.size.resize(i + 1, Size{ 10, 10 }); L.wrap.resize(i + 1, 0); } }

	inline Length randLen(Rng& r, int pAuto, bool allowPct) {
		int v = r.range(0, 99);
		if (v < pAuto) return {};
		if (allowPct && v > 90) return Length::pct(r.f(0, 120));
		return Length::px(r.f(0, 160));
	}
	inline LayoutStyle randStyle(Rng& r, LayoutKind kind) {
		LayoutStyle s;
		s.width = randLen(r, 55, true); s.height = randLen(r, 55, true);
		if (gUseBasis && r.chance(15)) s.basis = randLen(r, 0, true);   // short-circuit: consumes no randomness when disabled
		if (r.chance(20)) s.minW = r.f(0, 60);
		if (r.chance(20)) s.minH = r.f(0, 60);
		if (r.chance(15)) s.maxW = r.f(20, 200);
		if (r.chance(15)) s.maxH = r.f(20, 200);
		if (r.chance(35)) s.grow = r.f(0, 3);
		if (r.chance(35)) s.shrink = r.f(0, 3);
		if (r.chance(40)) s.margin = { r.f(0, 8), r.f(0, 8), r.f(0, 8), r.f(0, 8) };
		if (r.chance(40)) s.padding = { r.f(0, 8), r.f(0, 8), r.f(0, 8), r.f(0, 8) };
		if (r.chance(40)) s.gap = { r.f(0, 8), r.f(0, 8) };
		s.alignItems = Align(r.range(0, 4)); s.alignSelf = Align(r.range(0, 4)); s.justify = Justify(r.range(0, 5));
		if (r.chance(12)) {
			s.position = Position::Absolute; s.left = r.chance(50) ? Length::px(r.f(0, 50)) : Length{}; s.top = r.chance(50) ? Length::px(r.f(0, 50)) : Length{};
			s.right = r.chance(30) ? Length::px(r.f(0, 50)) : Length{}; s.bottom = r.chance(30) ? Length::px(r.f(0, 50)) : Length{};
		}
		if (kind == LayoutKind::Grid) {
			s.colCount = u8(r.range(1, 4)); s.rowCount = u8(r.range(0, 3));
			for (int i = 0; i < s.colCount; ++i) s.cols[i] = r.chance(40) ? GridTrack::px(r.f(10, 80)) : r.chance(50) ? GridTrack::fr(r.f(0.5f, 3)) : GridTrack::autoTrack();
			for (int i = 0; i < s.rowCount; ++i) s.rows[i] = r.chance(40) ? GridTrack::px(r.f(10, 50)) : r.chance(50) ? GridTrack::fr(r.f(0.5f, 3)) : GridTrack::autoTrack();
		}
		if (r.chance(25)) { s.gridCol = r.range(-1, 3); s.gridRow = r.range(-1, 3); s.colSpan = u8(r.range(1, 3)); s.rowSpan = u8(r.range(1, 2)); }
		s.splitDir = r.chance(50) ? SplitDir::Horizontal : SplitDir::Vertical; s.gutter = r.f(0, 8);
		s.splitWeight = r.f(0.2f, 4); if (r.chance(30)) s.splitMin = r.f(0, 70);
		s.scrollX = r.chance(30); s.scrollY = r.chance(80);
		return s;
	}
	inline NodeId build(Rng& r, LayoutTree& t, LeafCtx& L, int depth) {
		bool leaf = depth >= 4 || r.chance(depth == 0 ? 0 : 35 + depth * 10);
		LayoutKind k = leaf ? LayoutKind::Leaf : LayoutKind(r.range(1, 6));
		NodeId n = t.create(k, randStyle(r, k));
		if (leaf) { ensure(L, n.index); L.size[n.index] = { r.f(0, 120), r.f(0, 60) }; L.wrap[n.index] = r.chance(30); return n; }
		int kids = r.range(0, 5);
		for (int i = 0; i < kids; ++i) t.append(n, build(r, t, L, depth + 1));
		return n;
	}
	inline NodeId clone(LayoutTree& dst, LeafCtx& dl, const LayoutTree& src, const LeafCtx& sl, NodeId s) {
		NodeId d = dst.create(src.kind(s), src.style(s));
		if (src.kind(s) == LayoutKind::Leaf) { ensure(dl, d.index); dl.size[d.index] = sl.size[s.index]; dl.wrap[d.index] = sl.wrap[s.index]; }
		for (NodeId c = src.firstChild(s); c.valid(); c = src.nextSibling(c)) dst.append(d, clone(dst, dl, src, sl, c));
		return d;
	}
	inline void collect(const LayoutTree& t, NodeId n, std::vector<NodeId>& out) {
		out.push_back(n);
		for (NodeId c = t.firstChild(n); c.valid(); c = t.nextSibling(c)) collect(t, c, out);
	}
	inline bool sane(Rect r) { return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) && std::isfinite(r.h) && r.w >= -0.001f && r.h >= -0.001f; }
	inline bool same(Rect a, Rect b) { auto e = [](f32 x, f32 y) { return std::fabs(x - y) <= 0.002f + 1e-5f * std::fabs(x); }; return e(a.x, b.x) && e(a.y, b.y) && e(a.w, b.w) && e(a.h, b.h); }

	/// Returns an empty string when incremental == full for every node, else a description of the first mismatch.
	inline std::string compare(LayoutTree& live, LeafCtx& ll, NodeId root) {
		LayoutTree fresh; LeafCtx fl; fresh.setLeafMeasure(leafCb, &fl);
		NodeId froot = clone(fresh, fl, live, ll, root);
		Rect rr = live.rect(root);
		fresh.layout(froot, rr.w, rr.h);
		std::vector<NodeId> a, b; collect(live, root, a); collect(fresh, froot, b);
		if (a.size() != b.size()) return "clone has a different node count";
		for (std::size_t i = 0; i < a.size(); ++i) {
			Rect x = live.rect(a[i]), y = fresh.rect(b[i]);
			if (!sane(x)) { char m[160]; std::snprintf(m, sizeof m, "node #%zu has a non-finite/negative rect (%g,%g %gx%g)", i, x.x, x.y, x.w, x.h); return m; }
			if (!same(x, y)) { char m[200]; std::snprintf(m, sizeof m, "node #%zu: incremental (%g,%g %gx%g) != full (%g,%g %gx%g)", i, x.x, x.y, x.w, x.h, y.x, y.y, y.w, y.h); return m; }
		}
		return {};
	}

	/// One fuzz case: random tree + `steps` random mutations, checking after each layout. Returns "" on success.
	inline std::string runCase(u64 seed, int steps, bool useBasis = true) {
		gUseBasis = useBasis;
		Rng r(seed);
		LayoutTree t; LeafCtx L; t.setLeafMeasure(leafCb, &L);
		NodeId root = build(r, t, L, 0);
		f32 W = r.f(60, 600), H = r.f(60, 600);
		t.layout(root, W, H);
		if (auto e = compare(t, L, root); !e.empty()) return "initial layout: " + e;
		for (int step = 0; step < steps; ++step) {
			int nops = r.range(1, 3); std::string ops;
			for (int o = 0; o < nops; ++o) {
				std::vector<NodeId> all; collect(t, root, all);
				NodeId pick = all[r.next() % all.size()];
				switch (r.range(0, 7)) {
				case 0: case 1: { t.setStyle(pick, randStyle(r, t.kind(pick))); ops += " setStyle"; break; }
				case 2: if (t.kind(pick) == LayoutKind::Leaf) { L.size[pick.index] = { r.f(0, 120), r.f(0, 60) }; t.markContentChanged(pick); ops += " leafSize"; } break;
				case 3: if (t.kind(pick) != LayoutKind::Leaf) { t.append(pick, build(r, t, L, 3)); ops += " addSubtree"; } break;
				case 4: if (pick != root) { t.destroy(pick); ops += " destroy"; } break;
				case 5: if (pick != root) { NodeId to = all[r.next() % all.size()]; if (t.kind(to) != LayoutKind::Leaf && t.append(to, pick)) ops += " move"; } break;
				case 6: if (t.kind(pick) == LayoutKind::Split && t.gutterCount(pick) > 0) {
					SplitDrag d = t.splitDragBegin(pick, u32(r.next() % t.gutterCount(pick))); t.splitDragUpdate(d, r.f(-200, 200)); ops += " splitDrag";
				} break;
				case 7: W = r.f(60, 600); H = r.f(60, 600); ops += " resize"; break;
				}
			}
			t.layout(root, W, H);
			if (auto e = compare(t, L, root); !e.empty()) return "step " + std::to_string(step) + " after:" + ops + " -> " + e;
		}
		return {};
	}
}
