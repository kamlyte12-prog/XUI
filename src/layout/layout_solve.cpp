// Layout resolvers: one code path serves measure (doArrange=false) and arrange (doArrange=true).
#include "layout_internal.hpp"
#include <algorithm>
namespace xui {
	namespace {
		inline bool fin(f32 v) { return std::isfinite(v); }
		inline f32 clampv(f32 v, f32 lo, f32 hi) { return v < lo ? lo : (v > hi ? hi : v); }   // lo wins if lo > hi (CSS rule)
		inline f32 sub(f32 a, f32 b) { return fin(a) ? std::max(0.f, a - b) : a; }            // INF - x = INF
		inline f32 inset(const Length& l, f32 base) {                                          // NaN = unset
			return l.k == LenKind::Px ? l.v : l.k == LenKind::Percent ? l.v * 0.01f * base : std::nanf("");
		}
		// Percentages resolve against the containing block's ACTUAL size, and only when that size is definite. Returns NaN when
		// the length is not a percentage, or cannot be resolved yet (then it behaves as `auto`, and the caller taints its result).
		inline f32 resolvePct(const Length& l, f32 base, bool definite, u32& taint) {
			if (l.k != LenKind::Percent) return std::nanf("");
			if (!definite || !fin(base)) { ++taint; return std::nanf(""); }
			return l.v * 0.01f * base;
		}
		Align effAlign(const LayoutStyle& parent, const LayoutStyle& child) {
			Align a = child.alignSelf != Align::Auto ? child.alignSelf : parent.alignItems;
			return a == Align::Auto ? Align::Stretch : a;
		}
		// Split solver: sizes proportional to weights, none below its minimum, summing to `total`.
		void solveSplit(u32 n, const f32* w, const f32* mn, const f32* mx, f32 total, f32* out) {
			f32 sumMin = 0; for (u32 k = 0; k < n; ++k) sumMin += mn[k];
			if (sumMin >= total) { for (u32 k = 0; k < n; ++k) out[k] = sumMin > 0 ? mn[k] * total / sumMin : total / f32(n); return; }
			for (u32 k = 0; k < n; ++k) out[k] = -1;
			for (u32 iter = 0; iter <= n; ++iter) {
				f32 frozen = 0, wsum = 0; u32 open = 0;
				for (u32 k = 0; k < n; ++k) { if (out[k] >= 0) frozen += out[k]; else { wsum += w[k]; ++open; } }
				if (!open) return;
				f32 rem = total - frozen; bool viol = false;
				for (u32 k = 0; k < n; ++k) if (out[k] < 0) {
					const f32 share = rem * w[k] / wsum;
					if (share < mn[k]) { out[k] = mn[k]; viol = true; }
					else if (share > mx[k]) { out[k] = mx[k]; viol = true; }
				}
				if (!viol) { for (u32 k = 0; k < n; ++k) if (out[k] < 0) out[k] = rem * w[k] / wsum; return; }
			}
		}
	}

	// ---- entry points ---------------------------------------------------------------------------------------
	void LayoutTree::layout(NodeId root, f32 w, f32 h) {
		u32 i = idx(root);
		if (i == kNone) return;
		m_rect[i].x = m_rect[i].y = 0;
		arrange(i, w, h);
	}

	Size LayoutTree::measure(u32 i, const Constraint& c) {
		++m_stats.measureCalls;
		Size out;
		if (cacheLookup(i, c, out)) { ++m_stats.measureCacheHits; return out; }
		const u32 taint0 = m_taint;
		const LayoutStyle& st = m_style[i];
		const f32 minW = layoutMin(st, true, c.w), maxW = layoutMax(st, true, c.w);
		const f32 minH = layoutMin(st, false, c.h), maxH = layoutMax(st, false, c.h);
		const Edges insetEdges = contentInsets(st);
		const f32 padX = insetEdges.l + insetEdges.r, padY = insetEdges.t + insetEdges.b;
		bool fw = false, fh = false; f32 W = 0, H = 0;
		if (c.fw) { fw = true; W = c.w; }
		else if (st.width.k == LenKind::Px) { fw = true; W = clampv(st.width.v, minW, maxW); }
		else if (st.width.k == LenKind::Percent) ++m_taint;      // the parent's resolver resolves percentages and passes a fixed constraint; here: auto
		if (c.fh) { fh = true; H = c.h; }
		else if (st.height.k == LenKind::Px) { fh = true; H = clampv(st.height.v, minH, maxH); }
		else if (st.height.k == LenKind::Percent) ++m_taint;
		const f32 cw = fw ? std::max(0.f, W - padX) : sub(c.w, padX), ch = fh ? std::max(0.f, H - padY) : sub(c.h, padY);
		Size content = contentFor(i, cw, ch, fw, fh, false);
		// A min/max clamp applied to a content-derived size means the box no longer matches what the content was laid out for:
		// taint the result (and, through cache hits and this counter, every ancestor's).
		if (!fw) { const f32 raw = content.w + padX; out.w = clampv(raw, minW, maxW); if (out.w != raw) ++m_taint; }
		else out.w = W;
		if (!fh) { const f32 raw = content.h + padY; out.h = clampv(raw, minH, maxH); if (out.h != raw) ++m_taint; }
		else out.h = H;
		if (m_kind[i] == static_cast<u8>(LayoutKind::Scroll)) {   // a scroller never grows past the space it is offered
			const bool row = st.scrollX && !st.scrollY;
			if (row && !fw && fin(c.w) && out.w > c.w) { out.w = c.w; ++m_taint; }
			if (!row && !fh && fin(c.h) && out.h > c.h) { out.h = c.h; ++m_taint; }
		}
		cacheStore(i, c, out, m_taint != taint0);
		return out;
	}

	void LayoutTree::arrange(u32 i, f32 w, f32 h) {
		++m_stats.arrangeCalls;
		Rect& r = m_rect[i];
		const bool changed = r.w != w || r.h != h;
		r.w = w; r.h = h;
		const u8 f = m_flags[i];
		if (!(f & F_ARR) && !changed) {
			if (f & F_DESC) {   // only descendants are dirty: visit exactly the flagged children
				m_flags[i] &= static_cast<u8>(~F_DESC);
				for (u32 c = m_first[i]; c != kNone; c = m_next[c])
					if (m_flags[c] & (F_ARR | F_DESC)) arrange(c, m_rect[c].w, m_rect[c].h);
			}
			return;
		}
		++m_stats.nodesArranged;
		layoutChildren(i, w, h);
		m_flags[i] &= static_cast<u8>(~(F_ARR | F_DESC));
	}

	void LayoutTree::layoutChildren(u32 i, f32 w, f32 h) {
		const LayoutStyle& st = m_style[i];
		const Edges insetEdges = contentInsets(st);
		const f32 padX = insetEdges.l + insetEdges.r, padY = insetEdges.t + insetEdges.b;
		if (m_kind[i] == static_cast<u8>(LayoutKind::Leaf)) { m_content[i] = { w, h }; return; }
		Size used = contentFor(i, std::max(0.f, w - padX), std::max(0.f, h - padY), true, true, true);
		(void)used;
		m_content[i] = { m_content[i].x + padX, m_content[i].y + padY };    // resolvers store the children's extent
		arrangeAbsolute(i, w, h);
		if (m_kind[i] == static_cast<u8>(LayoutKind::Scroll)) clampScroll(i, m_scroll[i]);
	}

	Size LayoutTree::contentFor(u32 i, f32 cw, f32 ch, bool fw, bool fh, bool doArrange) {
		switch (static_cast<LayoutKind>(m_kind[i])) {
		case LayoutKind::Leaf:
			return m_measure ? m_measure(m_measureCtx, idOf(i), Constraint{ cw, ch, fw, fh }) : Size{};
		case LayoutKind::Row: case LayoutKind::Column: case LayoutKind::Scroll: return flexResolve(i, cw, ch, fw, fh, doArrange);
		case LayoutKind::Stack: return stackResolve(i, cw, ch, fw, fh, doArrange);
		case LayoutKind::Grid: return gridResolve(i, cw, ch, fw, fh, doArrange);
		case LayoutKind::Split: return splitResolve(i, cw, ch, fw, fh, doArrange);
		}
		return {};
	}

	// ---- flex -----------------------------------------------------------------------------------------------
	Size LayoutTree::flexResolve(u32 i, f32 cw, f32 ch, bool fw, bool fh, bool doArrange) {
		const LayoutStyle& st = m_style[i];
		const Edges insetEdges = contentInsets(st);
		const bool scroll = m_kind[i] == static_cast<u8>(LayoutKind::Scroll);
		const bool row = m_kind[i] == static_cast<u8>(LayoutKind::Row) || (scroll && st.scrollX && !st.scrollY);
		const f32 mainAvail = row ? cw : ch, crossAvail = row ? ch : cw;
		const bool mainFixed = row ? fw : fh, crossFixed = row ? fh : fw;
		const f32 gap = row ? st.gap.x : st.gap.y;
		auto mk = [&](f32 m, f32 c, bool fm, bool fc) { return row ? Constraint{ m, c, fm, fc } : Constraint{ c, m, fc, fm }; };

		const std::size_t base = m_flexItems.size();
		// 1. collect flow children and compute each one's flex base size
		for (u32 c = m_first[i]; c != kNone; c = m_next[c]) {
			const LayoutStyle& cs = m_style[c];
			if (cs.position == Position::Absolute) continue;
			FlexItem it; it.node = c;
			it.msM = row ? cs.margin.l : cs.margin.t; it.meM = row ? cs.margin.r : cs.margin.b;
			it.msC = row ? cs.margin.t : cs.margin.l; it.meC = row ? cs.margin.b : cs.margin.r;
			it.grow = cs.grow; it.shrink = cs.shrink;
			it.minMain = row ? layoutMin(cs, true, mainAvail) : layoutMin(cs, false, mainAvail);
			it.maxMain = row ? layoutMax(cs, true, mainAvail) : layoutMax(cs, false, mainAvail);
			it.minCross = row ? layoutMin(cs, false, crossAvail) : layoutMin(cs, true, crossAvail);
			it.maxCross = row ? layoutMax(cs, false, crossAvail) : layoutMax(cs, true, crossAvail);
			it.align = effAlign(st, cs);
			const Length lm = cs.basis.k != LenKind::Auto ? cs.basis : (row ? cs.width : cs.height), lc = row ? cs.height : cs.width;
			it.stretch = it.align == Align::Stretch && lc.k == LenKind::Auto;
			const f32 crossAvailChild = sub(crossAvail, it.msC + it.meC);
			const f32 mainAvailChild = scroll ? kInf : sub(mainAvail, it.msM + it.meM);
			const f32 pm = resolvePct(lm, mainAvail, mainFixed && !scroll, m_taint);
			const f32 pc = resolvePct(lc, crossAvail, crossFixed, m_taint);
			const bool pcOk = !std::isnan(pc);
			f32 main;
			if (lm.k == LenKind::Px) main = lm.v;
			else if (!std::isnan(pm)) main = pm;
			else {
				Size s = measure(c, mk(mainAvailChild, pcOk ? clampv(pc, it.minCross, it.maxCross) : crossAvailChild, false, pcOk || (crossFixed && it.stretch)));
				main = row ? s.w : s.h; it.measuredCross = row ? s.h : s.w; it.haveCross = true;
			}
			// CSS flex: the *base* size (before min/max) seeds free-space distribution; the *hypothetical* size is the clamped one.
			it.basis = it.measuredMain = main;
			it.hyp = clampv(main, it.minMain, it.maxMain);
			m_flexItems.push_back(it);
		}
		const std::size_t n = m_flexItems.size() - base;
		FlexItem* items = m_flexItems.data() + base;    // valid until the next push_back; we only measure/arrange (which restore size)

		// 2. resolve flexible lengths (CSS flexbox algorithm: choose grow/shrink once, freeze inflexible items, then
		//    repeatedly distribute free space and freeze min/max violators until nothing violates)
		const f32 gaps = n > 1 ? gap * f32(n - 1) : 0.f;
		f32 sumHyp = gaps;
		for (std::size_t k = 0; k < n; ++k) sumHyp += items[k].hyp + items[k].msM + items[k].meM;
		const f32 target = mainFixed ? mainAvail : (scroll || !fin(mainAvail) ? sumHyp : std::min(sumHyp, mainAvail));
		const bool growing = sumHyp < target;
		for (std::size_t k = 0; k < n; ++k) {
			FlexItem& it = items[k];
			const f32 factor = growing ? it.grow : it.shrink;
			const bool inflexible = factor == 0 || (growing ? it.basis > it.hyp : it.basis < it.hyp) || (scroll && !growing);
			it.frozen = inflexible; it.main = inflexible ? it.hyp : it.basis;
		}
		for (std::size_t iter = 0; iter <= n; ++iter) {
			f32 used = gaps, totalW = 0;
			for (std::size_t k = 0; k < n; ++k) {
				used += items[k].main + items[k].msM + items[k].meM;
				if (!items[k].frozen) totalW += growing ? items[k].grow : items[k].shrink * items[k].basis;
			}
			const f32 rem = target - used;
			f32 viol = 0; bool anyOpen = false;
			for (std::size_t k = 0; k < n; ++k) {
				FlexItem& it = items[k]; if (it.frozen) continue;
				anyOpen = true;
				const f32 wgt = growing ? it.grow : it.shrink * it.basis;
				it.raw = it.main + (totalW > 0 ? rem * wgt / totalW : 0.f);
				it.tent = clampv(it.raw, it.minMain, it.maxMain);
				viol += it.tent - it.raw;
			}
			if (!anyOpen) break;
			if (viol == 0) {
				for (std::size_t k = 0; k < n; ++k) if (!items[k].frozen) { items[k].main = items[k].tent; items[k].frozen = true; } break;
			}
			for (std::size_t k = 0; k < n; ++k) {
				FlexItem& it = items[k]; if (it.frozen) continue;
				if (viol > 0 ? it.tent > it.raw : it.tent < it.raw) { it.main = it.tent; it.frozen = true; }   // freeze the violators
			}
		}
		f32 usedMain = gaps;
		for (std::size_t k = 0; k < n; ++k) usedMain += items[k].main + items[k].msM + items[k].meM;

		// 3. cross sizes
		f32 crossContent = crossFixed ? crossAvail : 0.f, crossExtent = 0.f;
		if (doArrange || !crossFixed) {
			for (std::size_t k = 0; k < n; ++k) {
				FlexItem it = items[k];
				const LayoutStyle& cs = m_style[it.node];
				const Length lc = row ? cs.height : cs.width;
				f32 cross;
				const f32 pc = resolvePct(lc, crossAvail, crossFixed, m_taint);
				if (it.stretch && crossFixed) cross = std::max(0.f, crossAvail - it.msC - it.meC);
				else if (lc.k == LenKind::Px) cross = lc.v;
				else if (!std::isnan(pc)) cross = pc;
				else if (it.haveCross && it.main == it.measuredMain) cross = it.measuredCross;   // main unchanged: reuse the earlier measure
				else {
					Size s = measure(it.node, mk(it.main, sub(crossAvail, it.msC + it.meC), true, false));
					cross = row ? s.h : s.w;
				}
				it.cross = clampv(cross, it.minCross, it.maxCross);
				items = m_flexItems.data() + base; items[k] = it;
				crossExtent = std::max(crossExtent, it.cross + it.msC + it.meC);
				if (!crossFixed) crossContent = std::max(crossContent, it.cross + it.msC + it.meC);
			}
			if (!crossFixed)   // widest sibling defines the container; stretch items grow to match
				for (std::size_t k = 0; k < n; ++k) if (items[k].stretch) items[k].cross = clampv(std::max(0.f, crossContent - items[k].msC - items[k].meC), items[k].minCross, items[k].maxCross);
		}

		const f32 mainSize = mainFixed ? mainAvail : target;
		if (doArrange) {
			m_content[i] = row ? Vec2{ usedMain, crossExtent } : Vec2{ crossExtent, usedMain };
			f32 leftover = mainSize - usedMain, off = 0, extra = 0;
			if (leftover > 0) switch (st.justify) {
			case Justify::Start: break;
			case Justify::Center: off = leftover * 0.5f; break;
			case Justify::End: off = leftover; break;
			case Justify::SpaceBetween: if (n > 1) extra = leftover / f32(n - 1); break;
			case Justify::SpaceAround: extra = leftover / f32(n); off = extra * 0.5f; break;
			case Justify::SpaceEvenly: extra = leftover / f32(n + 1); off = extra; break;
			}
			f32 pos = (row ? insetEdges.l : insetEdges.t) + off;
			const f32 crossStart = row ? insetEdges.t : insetEdges.l;
			for (std::size_t k = 0; k < n; ++k) {
				const FlexItem it = m_flexItems[base + k];
				f32 co;
				switch (it.align) {
				case Align::Center: co = it.msC + (crossContent - it.cross - it.msC - it.meC) * 0.5f; break;
				case Align::End: co = crossContent - it.cross - it.meC; break;
				default: co = it.msC; break;
				}
				Rect& r = m_rect[it.node];
				r.x = row ? pos + it.msM : crossStart + co; r.y = row ? crossStart + co : pos + it.msM;
				arrange(it.node, row ? it.main : it.cross, row ? it.cross : it.main);
				pos += it.msM + it.main + it.meM + gap + extra;
			}
		}
		m_flexItems.erase(m_flexItems.begin() + static_cast<std::ptrdiff_t>(base), m_flexItems.end());
		return row ? Size{ mainSize, crossContent } : Size{ crossContent, mainSize };
	}

	// ---- stack ----------------------------------------------------------------------------------------------
	Size LayoutTree::stackResolve(u32 i, f32 cw, f32 ch, bool fw, bool fh, bool doArrange) {
		const LayoutStyle& st = m_style[i];
		const Edges insetEdges = contentInsets(st);
		f32 maxW = 0, maxH = 0;
		for (u32 c = m_first[i]; c != kNone; c = m_next[c]) {
			const LayoutStyle& cs = m_style[c];
			if (cs.position == Position::Absolute) continue;
			const f32 mW = cs.margin.l + cs.margin.r, mH = cs.margin.t + cs.margin.b;
			const Align a = effAlign(st, cs);
			const bool sw = a == Align::Stretch && cs.width.k == LenKind::Auto && fw, sh = a == Align::Stretch && cs.height.k == LenKind::Auto && fh;
			const f32 pw = resolvePct(cs.width, cw, fw, m_taint), ph = resolvePct(cs.height, ch, fh, m_taint);
			const f32 childMinW = layoutMin(cs, true, cw), childMaxW = layoutMax(cs, true, cw);
			const f32 childMinH = layoutMin(cs, false, ch), childMaxH = layoutMax(cs, false, ch);
			Size s = measure(c, Constraint{ std::isnan(pw) ? sub(cw, mW) : clampv(pw, childMinW, childMaxW), std::isnan(ph) ? sub(ch, mH) : clampv(ph, childMinH, childMaxH),
										   sw || !std::isnan(pw), sh || !std::isnan(ph) });
			maxW = std::max(maxW, s.w + mW); maxH = std::max(maxH, s.h + mH);
			if (doArrange) {
				// ms/me = margin at the start/end of the axis; centring happens between the margins.
				auto place = [&](f32 room, f32 size, f32 ms, f32 me) {
					return a == Align::Center ? ms + (room - ms - me - size) * 0.5f : a == Align::End ? room - me - size : ms;
					};
				Rect& r = m_rect[c];
				r.x = insetEdges.l + place(cw, s.w, cs.margin.l, cs.margin.r);
				r.y = insetEdges.t + place(ch, s.h, cs.margin.t, cs.margin.b);
				arrange(c, s.w, s.h);
			}
		}
		if (doArrange) m_content[i] = { maxW, maxH };
		return { fw ? cw : maxW, fh ? ch : maxH };
	}

	// ---- grid -----------------------------------------------------------------------------------------------
	Size LayoutTree::gridResolve(u32 i, f32 cw, f32 ch, bool fw, bool fh, bool doArrange) {
		const LayoutStyle& st = m_style[i];
		const Edges insetEdges = contentInsets(st);
		const u32 ncols = std::min<u32>(kMaxTracks, std::max<u32>(1, st.colCount));
		const std::size_t ibase = m_gridItems.size();
		// 1. Place explicit cells first so row-major auto-placement can skip them. Explicit
		// cells may overlap each other; automatic cells choose the next unoccupied area.
		i32 cc = 0, cr = 0, maxRow = -1;
		for (u32 c = m_first[i]; c != kNone; c = m_next[c]) {
			const LayoutStyle& cs = m_style[c];
			if (cs.position == Position::Absolute) continue;
			GridItem g; g.node = c; g.cs = std::min<u32>(std::max<u32>(1, cs.colSpan), ncols); g.rs = std::max<u32>(1, cs.rowSpan);
			g.ml = cs.margin.l; g.mt = cs.margin.t; g.mr = cs.margin.r; g.mb = cs.margin.b;
			g.explicitCell = cs.gridCol >= 0 && cs.gridRow >= 0;
			if (!g.explicitCell) continue;
			g.col = cs.gridCol; g.row = cs.gridRow;
			g.col = std::min<i32>(g.col, i32(ncols - g.cs));
			maxRow = std::max<i32>(maxRow, g.row + i32(g.rs) - 1);
			m_gridItems.push_back(g);
		}
		const std::size_t occupancyBase = m_scratchU.size();
		m_scratchU.resize(occupancyBase + std::size_t(std::max(0, maxRow + 1)), 0u);
		auto markOccupied = [&](const GridItem& item) {
			const u32 mask = ((u32{ 1 } << item.cs) - 1u) << u32(item.col);
			const std::size_t required = occupancyBase + std::size_t(item.row + i32(item.rs));
			if (m_scratchU.size() < required) m_scratchU.resize(required, 0u);
			for (i32 row = item.row; row < item.row + i32(item.rs); ++row)
				m_scratchU[occupancyBase + std::size_t(row)] |= mask;
		};
		for (std::size_t k = ibase; k < m_gridItems.size(); ++k) markOccupied(m_gridItems[k]);
		for (u32 c = m_first[i]; c != kNone; c = m_next[c]) {
			const LayoutStyle& cs = m_style[c];
			if (cs.position == Position::Absolute || (cs.gridCol >= 0 && cs.gridRow >= 0)) continue;
			GridItem g; g.node = c; g.cs = std::min<u32>(std::max<u32>(1, cs.colSpan), ncols); g.rs = std::max<u32>(1, cs.rowSpan);
			g.ml = cs.margin.l; g.mt = cs.margin.t; g.mr = cs.margin.r; g.mb = cs.margin.b;
			if (cs.gridCol >= 0 || cs.gridRow >= 0) {
				g.col = cs.gridCol >= 0 ? cs.gridCol : 0; g.row = cs.gridRow >= 0 ? cs.gridRow : cr;
				g.col = std::min<i32>(g.col, i32(ncols - g.cs));
			}
			else {
				for (;;) {
					if (cc + i32(g.cs) > i32(ncols)) { cc = 0; ++cr; }
					g.col = cc; g.row = cr;
					const u32 mask = ((u32{ 1 } << g.cs) - 1u) << u32(g.col);
					const std::size_t required = occupancyBase + std::size_t(g.row + i32(g.rs));
					if (m_scratchU.size() < required) m_scratchU.resize(required, 0u);
					bool occupied = false;
					for (i32 row = g.row; row < g.row + i32(g.rs); ++row)
						if (m_scratchU[occupancyBase + std::size_t(row)] & mask) { occupied = true; break; }
					if (!occupied) break;
					++cc;
				}
				const u32 mask = ((u32{ 1 } << g.cs) - 1u) << u32(g.col);
				for (i32 row = g.row; row < g.row + i32(g.rs); ++row)
					m_scratchU[occupancyBase + std::size_t(row)] |= mask;
				cc += i32(g.cs);
				if (cc >= i32(ncols)) { cc = 0; ++cr; }
			}
			if (cs.gridCol >= 0 || cs.gridRow >= 0) markOccupied(g);
			maxRow = std::max<i32>(maxRow, g.row + i32(g.rs) - 1);
			m_gridItems.push_back(g);
		}
		m_scratchU.resize(occupancyBase);
		const std::size_t nItems = m_gridItems.size() - ibase;
		const u32 nrows = static_cast<u32>(std::max<i32>(maxRow + 1, i32(st.rowCount)));
		// scratch layout: colSize[ncols] rowSize[nrows] colMax[ncols] rowMax[nrows]
		const std::size_t fbase = m_scratchF.size();
		m_scratchF.resize(fbase + 2 * (ncols + nrows), 0.f);
		auto colTrack = [&](u32 k) { return k < st.colCount ? st.cols[k] : GridTrack::autoTrack(); };
		auto rowTrack = [&](u32 k) { return k < st.rowCount ? st.rows[k] : st.autoRow; };
		auto CS = [&](u32 k) -> f32& { return m_scratchF[fbase + k]; };
		auto RS = [&](u32 k) -> f32& { return m_scratchF[fbase + ncols + k]; };
		auto CM = [&](u32 k) -> f32& { return m_scratchF[fbase + ncols + nrows + k]; };
		auto RM = [&](u32 k) -> f32& { return m_scratchF[fbase + 2 * ncols + nrows + k]; };
		const f32 gx = st.gap.x, gy = st.gap.y;

		// size one axis of tracks: Px fixed, Auto = content, Fr = share of leftover (or content/fr when unbounded)
		auto sizeTracks = [&](u32 count, auto&& track, auto&& size, auto&& maxc, f32 avail, bool fixed, f32 gapv) {
			f32 fixedSum = 0, frSum = 0, unit = 0;
			for (u32 k = 0; k < count; ++k) {
				GridTrack t = track(k);
				if (t.kind == GridTrack::Px) size(k) = t.v;
				else if (t.kind == GridTrack::Auto) size(k) = maxc(k);
				else { frSum += t.v; unit = std::max(unit, t.v > 0 ? maxc(k) / t.v : 0.f); continue; }
				fixedSum += size(k);
			}
			const f32 gaps = count > 1 ? gapv * f32(count - 1) : 0.f;
			for (u32 k = 0; k < count; ++k) {
				GridTrack t = track(k);
				if (t.kind != GridTrack::Fr) continue;
				if (fixed && frSum > 0) size(k) = std::max(0.f, avail - fixedSum - gaps) * t.v / frSum;
				else size(k) = unit * t.v;
			}
			};
		auto spanSum = [&](auto&& size, u32 first, u32 span, f32 gapv) {
			f32 s = 0; for (u32 k = 0; k < span; ++k) s += size(first + k);
			return s + (span > 1 ? gapv * f32(span - 1) : 0.f);
			};
		// 2. columns: content width of span-1 items in Auto/Fr tracks
		for (std::size_t k = 0; k < nItems; ++k) {
			const GridItem g = m_gridItems[ibase + k];
			if (g.cs != 1 || colTrack(u32(g.col)).kind == GridTrack::Px) continue;
			Size s = measure(g.node, Constraint{ kInf, kInf, false, false });
			CM(u32(g.col)) = std::max(CM(u32(g.col)), s.w + g.ml + g.mr);
		}
		sizeTracks(ncols, colTrack, CS, CM, cw, fw, gx);
		// 3. rows: measured at their (stretched) column width
		for (std::size_t k = 0; k < nItems; ++k) {
			const GridItem g = m_gridItems[ibase + k];
			if (g.rs != 1 || rowTrack(u32(g.row)).kind == GridTrack::Px) continue;
			const LayoutStyle& cs = m_style[g.node];
			const f32 cellW = spanSum(CS, u32(g.col), g.cs, gx) - g.ml - g.mr;
			const bool stretchW = effAlign(st, cs) == Align::Stretch && cs.width.k == LenKind::Auto;
			const f32 pw = resolvePct(cs.width, cellW, true, m_taint);          // percentages are of the grid cell
			Size s = measure(g.node, Constraint{ std::isnan(pw) ? std::max(0.f, cellW) : clampv(pw, layoutMin(cs, true, cellW), layoutMax(cs, true, cellW)), kInf, stretchW || !std::isnan(pw), false });
			RM(u32(g.row)) = std::max(RM(u32(g.row)), s.h + g.mt + g.mb);
		}
		sizeTracks(nrows, rowTrack, RS, RM, ch, fh, gy);
		const f32 totalW = spanSum(CS, 0, ncols, gx), totalH = nrows ? spanSum(RS, 0, nrows, gy) : 0.f;

		if (doArrange) {
			m_content[i] = { totalW, totalH };
			for (std::size_t k = 0; k < nItems; ++k) {
				const GridItem g = m_gridItems[ibase + k];
				const LayoutStyle& cs = m_style[g.node];
				f32 x = 0, y = 0;
				for (u32 c = 0; c < u32(g.col); ++c) x += CS(c) + gx;
				for (u32 r = 0; r < u32(g.row); ++r) y += RS(r) + gy;
				const f32 cellW = spanSum(CS, u32(g.col), g.cs, gx) - g.ml - g.mr, cellH = spanSum(RS, u32(g.row), std::min(g.rs, nrows - u32(g.row)), gy) - g.mt - g.mb;
				const Align a = effAlign(st, cs);
				const bool sw = a == Align::Stretch && cs.width.k == LenKind::Auto, sh = a == Align::Stretch && cs.height.k == LenKind::Auto;
				const f32 pw = resolvePct(cs.width, cellW, true, m_taint), ph = resolvePct(cs.height, cellH, true, m_taint);
				Size s = measure(g.node, Constraint{ std::isnan(pw) ? std::max(0.f, cellW) : clampv(pw, layoutMin(cs, true, cellW), layoutMax(cs, true, cellW)), std::isnan(ph) ? std::max(0.f, cellH) : clampv(ph, layoutMin(cs, false, cellH), layoutMax(cs, false, cellH)),
													sw || !std::isnan(pw), sh || !std::isnan(ph) });
				auto off = [&](f32 room, f32 size) { return a == Align::Center ? (room - size) * 0.5f : a == Align::End ? room - size : 0.f; };
				Rect& r = m_rect[g.node];
				r.x = insetEdges.l + x + g.ml + off(cellW, s.w); r.y = insetEdges.t + y + g.mt + off(cellH, s.h);
				arrange(g.node, s.w, s.h);
			}
		}
		m_scratchF.resize(fbase);
		m_gridItems.erase(m_gridItems.begin() + static_cast<std::ptrdiff_t>(ibase), m_gridItems.end());
		return { fw ? cw : totalW, fh ? ch : totalH };
	}

	// ---- split ----------------------------------------------------------------------------------------------
	Size LayoutTree::splitResolve(u32 i, f32 cw, f32 ch, bool fw, bool fh, bool doArrange) {
		const LayoutStyle& st = m_style[i];
		const Edges insetEdges = contentInsets(st);
		const bool horiz = st.splitDir == SplitDir::Horizontal;
		u32 n = 0; for (u32 c = m_first[i]; c != kNone; c = m_next[c]) if (m_style[c].position != Position::Absolute) ++n;
		const f32 gutters = n > 1 ? st.gutter * f32(n - 1) : 0.f;
		const bool mainFixed = horiz ? fw : fh, crossFixed = horiz ? fh : fw;
		const f32 mainAvail = horiz ? cw : ch, crossAvail = horiz ? ch : cw;

		f32 mainSize = mainAvail, crossSize = crossAvail;
		if (!mainFixed || !crossFixed) {              // intrinsic size: panes at their natural size plus gutters
			f32 sumMain = gutters, maxCross = 0;
			for (u32 c = m_first[i]; c != kNone; c = m_next[c]) {
				if (m_style[c].position == Position::Absolute) continue;
				Size s = measure(c, Constraint{ cw, ch, false, false });
				sumMain += horiz ? s.w : s.h; maxCross = std::max(maxCross, horiz ? s.h : s.w);
			}
			if (!mainFixed) mainSize = sumMain;
			if (!crossFixed) crossSize = maxCross;
		}
		if (doArrange && n) {
			const std::size_t fbase = m_scratchF.size();
			m_scratchF.resize(fbase + 4 * n);
			f32* w = &m_scratchF[fbase], * mn = w + n, * mx = mn + n, * out = mx + n;
			u32 k = 0;
			for (u32 c = m_first[i]; c != kNone; c = m_next[c]) {
				if (m_style[c].position == Position::Absolute) continue;
				w[k] = std::max(1e-6f, m_style[c].splitWeight);
				const f32 contentMain = std::max(0.f, mainSize - gutters);
				mn[k] = std::max(m_style[c].splitMin, horiz ? layoutMin(m_style[c], true, contentMain) : layoutMin(m_style[c], false, contentMain));
				mx[k] = std::max(mn[k], horiz ? layoutMax(m_style[c], true, contentMain) : layoutMax(m_style[c], false, contentMain));
				++k;
			}
			solveSplit(n, w, mn, mx, std::max(0.f, mainSize - gutters), out);
			f32 pos = horiz ? insetEdges.l : insetEdges.t, ext = 0;
			k = 0;
			for (u32 c = m_first[i]; c != kNone; c = m_next[c]) {
				if (m_style[c].position == Position::Absolute) continue;
				const f32 sz = m_scratchF[fbase + 3 * n + k];   // re-read: arrange() may grow m_scratchF
				Rect& r = m_rect[c];
				r.x = horiz ? pos : insetEdges.l; r.y = horiz ? insetEdges.t : pos;
				arrange(c, horiz ? sz : crossSize, horiz ? crossSize : sz);
				pos += sz + st.gutter; ext += sz + st.gutter; ++k;
			}
			m_content[i] = horiz ? Vec2{ std::max(0.f, ext - st.gutter), crossSize } : Vec2{ crossSize, std::max(0.f, ext - st.gutter) };
			m_scratchF.resize(fbase);
		}
		return horiz ? Size{ mainSize, crossSize } : Size{ crossSize, mainSize };
	}

	// ---- absolute children ----------------------------------------------------------------------------------
	void LayoutTree::arrangeAbsolute(u32 i, f32 w, f32 h) {
		const LayoutStyle& st = m_style[i];
		const Edges insetEdges = contentInsets(st);
		for (u32 c = m_first[i]; c != kNone; c = m_next[c]) {
			const LayoutStyle& cs = m_style[c];
			if (cs.position != Position::Absolute) continue;
			const f32 l = inset(cs.left, w), r = inset(cs.right, w), t = inset(cs.top, h), b = inset(cs.bottom, h);
			const f32 mW = cs.margin.l + cs.margin.r, mH = cs.margin.t + cs.margin.b;
			f32 W, H;
			const f32 contentW = std::max(0.f, w - insetEdges.l - insetEdges.r);
			const f32 contentH = std::max(0.f, h - insetEdges.t - insetEdges.b);
			if (cs.width.k == LenKind::Px) W = cs.width.v;
			else if (cs.width.k == LenKind::Percent) W = cs.width.v * 0.01f * contentW;
			else if (!std::isnan(l) && !std::isnan(r)) W = std::max(0.f, w - l - r - mW);
			else W = measure(c, Constraint{ std::max(0.f, w - mW), kInf, false, false }).w;
			W = clampv(W, layoutMin(cs, true, contentW), layoutMax(cs, true, contentW));
			if (cs.height.k == LenKind::Px) H = cs.height.v;
			else if (cs.height.k == LenKind::Percent) H = cs.height.v * 0.01f * contentH;
			else if (!std::isnan(t) && !std::isnan(b)) H = std::max(0.f, h - t - b - mH);
			else H = measure(c, Constraint{ W, std::max(0.f, h - mH), true, false }).h;
			H = clampv(H, layoutMin(cs, false, contentH), layoutMax(cs, false, contentH));
			Rect& rc = m_rect[c];
			rc.x = !std::isnan(l) ? l + cs.margin.l : !std::isnan(r) ? w - r - W - cs.margin.r : insetEdges.l + cs.margin.l;
			rc.y = !std::isnan(t) ? t + cs.margin.t : !std::isnan(b) ? h - b - H - cs.margin.b : insetEdges.t + cs.margin.t;
			arrange(c, W, H);
		}
	}
}
