// LayoutTree: structure, invalidation, cache, queries, splitter drag. The resolvers live in layout_solve.cpp.
#include "layout_internal.hpp"
#include <algorithm>
namespace xui {

	LayoutTree::LayoutTree() = default;
	LayoutTree::~LayoutTree() = default;

	void LayoutTree::reserve(u32 n) {
		m_parent.reserve(n); m_first.reserve(n); m_last.reserve(n); m_next.reserve(n); m_prev.reserve(n); m_gen.reserve(n);
		m_childCount.reserve(n); m_kind.reserve(n); m_flags.reserve(n); m_alive.reserve(n); m_style.reserve(n); m_rect.reserve(n);
		m_content.reserve(n); m_scroll.reserve(n); m_cache.reserve(n); m_free.reserve(n);
		m_flexItems.reserve(n); m_gridItems.reserve(n); m_scratchF.reserve(4 * n + 64); m_scratchU.reserve(n + 16);
	}

	u32 LayoutTree::idx(NodeId n) const {
		if (n.index >= m_gen.size() || !m_alive[n.index] || m_gen[n.index] != n.gen) return kNone;
		return n.index;
	}
	bool LayoutTree::alive(NodeId n) const { return idx(n) != kNone; }
	NodeId LayoutTree::parent(NodeId n) const { u32 i = idx(n); return i == kNone || m_parent[i] == kNone ? NodeId{} : idOf(m_parent[i]); }
	NodeId LayoutTree::firstChild(NodeId n) const { u32 i = idx(n); return i == kNone || m_first[i] == kNone ? NodeId{} : idOf(m_first[i]); }
	NodeId LayoutTree::nextSibling(NodeId n) const { u32 i = idx(n); return i == kNone || m_next[i] == kNone ? NodeId{} : idOf(m_next[i]); }
	u32 LayoutTree::childCount(NodeId n) const { u32 i = idx(n); return i == kNone ? 0 : m_childCount[i]; }
	const LayoutStyle& LayoutTree::style(NodeId n) const { static const LayoutStyle dflt; u32 i = idx(n); return i == kNone ? dflt : m_style[i]; }

	LayoutKind LayoutTree::kind(NodeId n) const { u32 i = idx(n); return i == kNone ? LayoutKind::Leaf : static_cast<LayoutKind>(m_kind[i]); }

	NodeId LayoutTree::create(LayoutKind kind, const LayoutStyle& s) {
		u32 i;
		if (!m_free.empty()) { i = m_free.back(); m_free.pop_back(); }
		else {
			i = static_cast<u32>(m_gen.size());
			m_parent.push_back(kNone); m_first.push_back(kNone); m_last.push_back(kNone); m_next.push_back(kNone); m_prev.push_back(kNone);
			m_gen.push_back(1); m_childCount.push_back(0); m_kind.push_back(0); m_flags.push_back(0); m_alive.push_back(0);
			m_style.emplace_back(); m_rect.emplace_back(); m_content.emplace_back(); m_scroll.emplace_back(); m_cache.emplace_back();
		}
		m_parent[i] = m_first[i] = m_last[i] = m_next[i] = m_prev[i] = kNone; m_childCount[i] = 0;
		m_kind[i] = static_cast<u8>(kind); m_flags[i] = F_ARR; m_alive[i] = 1;
		m_style[i] = s; m_rect[i] = {}; m_content[i] = {}; m_scroll[i] = {}; m_cache[i] = {};
		++m_live;
		return idOf(i);
	}

	void LayoutTree::unlink(u32 c) {
		u32 p = m_parent[c];
		if (p == kNone) return;
		if (m_prev[c] != kNone) m_next[m_prev[c]] = m_next[c]; else m_first[p] = m_next[c];
		if (m_next[c] != kNone) m_prev[m_next[c]] = m_prev[c]; else m_last[p] = m_prev[c];
		m_parent[c] = m_prev[c] = m_next[c] = kNone; --m_childCount[p];
	}
	void LayoutTree::link(u32 p, u32 c, u32 before) {
		m_parent[c] = p;
		if (before == kNone) {
			m_prev[c] = m_last[p]; m_next[c] = kNone;
			if (m_last[p] != kNone) m_next[m_last[p]] = c; else m_first[p] = c;
			m_last[p] = c;
		}
		else {
			m_next[c] = before; m_prev[c] = m_prev[before];
			if (m_prev[before] != kNone) m_next[m_prev[before]] = c; else m_first[p] = c;
			m_prev[before] = c;
		}
		++m_childCount[p];
	}
	bool LayoutTree::insertBefore(NodeId parent, NodeId child, NodeId before) {
		u32 p = idx(parent), c = idx(child);
		if (p == kNone || c == kNone || p == c || m_kind[p] == static_cast<u8>(LayoutKind::Leaf)) return false;
		u32 b = before.valid() ? idx(before) : kNone;
		if (before.valid() && (b == kNone || m_parent[b] != p)) return false;
		if (b == c) return false;
		for (u32 a = p; a != kNone; a = m_parent[a]) if (a == c) return false;   // would create a cycle
		if (m_parent[c] != kNone) { u32 old = m_parent[c]; unlink(c); invalidateContent(old); }
		link(p, c, b);
		m_flags[c] |= F_ARR;
		invalidateContent(p);
		return true;
	}
	bool LayoutTree::append(NodeId parent, NodeId child) { return insertBefore(parent, child, NodeId{}); }
	void LayoutTree::detach(NodeId child) {
		u32 c = idx(child);
		if (c == kNone || m_parent[c] == kNone) return;
		u32 old = m_parent[c]; unlink(c); invalidateContent(old);
	}
	void LayoutTree::destroyRec(u32 i) {
		for (u32 c = m_first[i]; c != kNone;) { u32 nx = m_next[c]; destroyRec(c); c = nx; }
		m_alive[i] = 0; ++m_gen[i]; m_cache[i] = {}; m_free.push_back(i); --m_live;
	}
	void LayoutTree::destroy(NodeId n) {
		u32 i = idx(n);
		if (i == kNone) return;
		u32 p = m_parent[i];
		unlink(i);
		if (p != kNone) invalidateContent(p);
		destroyRec(i);
	}

	// ---- invalidation ---------------------------------------------------------------------------------------
	bool LayoutTree::isBoundary(u32 i) const {
		const LayoutStyle& s = m_style[i];
		return s.width.k == LenKind::Px && s.height.k == LenKind::Px;
	}
	// The node's *content* changed. Its own outer size only changes if it is not a fixed-size boundary.
	void LayoutTree::invalidateContent(u32 i) {
		u32 cur = i;
		for (;;) {
			m_cache[cur] = {}; m_flags[cur] |= F_ARR;
			if (isBoundary(cur) || m_parent[cur] == kNone) break;
			cur = m_parent[cur];
		}
		for (u32 p = m_parent[cur]; p != kNone; p = m_parent[p]) {
			if (m_flags[p] & F_DESC) break;      // invariant: F_DESC implies all ancestors are already marked
			m_flags[p] |= F_DESC;
		}
	}
	// The node's own style changed: its outer size may change, so its parent's content changed too.
	void LayoutTree::invalidateStyle(u32 i) {
		m_cache[i] = {}; m_flags[i] |= F_ARR;
		if (m_parent[i] != kNone) invalidateContent(m_parent[i]);
	}
	void LayoutTree::markArrangeOnly(u32 i) {
		m_flags[i] |= F_ARR;
		for (u32 p = m_parent[i]; p != kNone; p = m_parent[p]) { if (m_flags[p] & F_DESC) break; m_flags[p] |= F_DESC; }
	}
	void LayoutTree::setStyle(NodeId n, const LayoutStyle& s) { u32 i = idx(n); if (i == kNone) return; m_style[i] = s; invalidateStyle(i); }
	void LayoutTree::markContentChanged(NodeId n) { u32 i = idx(n); if (i != kNone) invalidateContent(i); }
	void LayoutTree::markAllDirty(NodeId root) {
		u32 i = idx(root);
		if (i == kNone) return;
		u32 n = i;                                   // iterative pre-order walk: no recursion, so tree depth cannot overflow the stack
		for (;;) {
			m_cache[n] = {}; m_flags[n] |= F_ARR;
			if (m_first[n] != kNone) { n = m_first[n]; continue; }
			while (n != i && m_next[n] == kNone) n = m_parent[n];
			if (n == i) break;
			n = m_next[n];
		}
		if (m_parent[i] != kNone) invalidateContent(m_parent[i]);
	}
	bool LayoutTree::needsLayout(NodeId root) const { u32 i = idx(root); return i != kNone && (m_flags[i] & (F_ARR | F_DESC)); }

	// ---- measure cache --------------------------------------------------------------------------------------
	namespace {
		// Can a result computed under (ea,ef)->es answer a query (qa,qf) on this axis?
		//  1. identical constraint: always.
		//  2. max-content: measured with an unbounded axis, queried with any bound the result fits in - the content saw the
		//     same (unconstrained) space either way. Only for clean entries. The bound must exceed the result by a small margin:
		//     content measure functions are discontinuous where text exactly fits (ceil() of lines), and `bound - padding` can
		//     land one ulp under the natural width, so the exact-boundary case is always recomputed instead of answered from cache.
		// A size-forcing rule ("natural size == forced size") was deliberately REMOVED: a flex container stretches children after
		// measuring them, whereas a forced layout measures them at the stretched size, so natural and forced layouts can differ.
		bool axisOk(f32 ea, bool ef, f32 es, f32 qa, bool qf, bool tainted) {
			if (ef == qf && ea == qa) return true;
			if (tainted) return false;
			return !qf && !ef && !std::isfinite(ea) && qa >= es + 1e-3f + 1e-5f * es;
		}
	}
	bool LayoutTree::cacheLookup(u32 i, const Constraint& q, Size& out) {
		for (const CacheEntry& e : m_cache[i].e)
			if (e.valid && axisOk(e.c.w, e.c.fw, e.s.w, q.w, q.fw, e.tainted) && axisOk(e.c.h, e.c.fh, e.s.h, q.h, q.fh, e.tainted)) {
				if (e.tainted) ++m_taint;      // a cached bound-dependent child taints whoever consumed it
				out = e.s; return true;
			}
		return false;
	}
	void LayoutTree::cacheStore(u32 i, const Constraint& c, Size s, bool tainted) {
		Cache& k = m_cache[i]; k.e[k.next] = { c, s, true, tainted }; k.next ^= 1;
	}

	// ---- queries --------------------------------------------------------------------------------------------
	Rect LayoutTree::rect(NodeId n) const { u32 i = idx(n); return i == kNone ? Rect{} : m_rect[i]; }
	Rect LayoutTree::absRect(NodeId n) const {
		u32 i = idx(n);
		if (i == kNone) return {};
		Rect r = m_rect[i];
		for (u32 p = m_parent[i]; p != kNone; p = m_parent[p]) { r.x += m_rect[p].x - m_scroll[p].x; r.y += m_rect[p].y - m_scroll[p].y; }
		return r;
	}
	Vec2 LayoutTree::contentSize(NodeId n) const { u32 i = idx(n); return i == kNone ? Vec2{} : m_content[i]; }
	Vec2 LayoutTree::scrollOffset(NodeId n) const { u32 i = idx(n); return i == kNone ? Vec2{} : m_scroll[i]; }
	void LayoutTree::setScrollOffset(NodeId n, Vec2 o) {
		u32 i = idx(n);
		if (i == kNone) return;
		clampScroll(i, o);
	}
	void LayoutTree::clampScroll(u32 i, Vec2 o) {
		f32 mx = std::max(0.f, m_content[i].x - m_rect[i].w), my = std::max(0.f, m_content[i].y - m_rect[i].h);
		m_scroll[i] = { std::min(std::max(o.x, 0.f), mx), std::min(std::max(o.y, 0.f), my) };
	}

	// ---- splitters ------------------------------------------------------------------------------------------
	u32 LayoutTree::flowKid(u32 split, u32 ordinal) const {
		u32 k = 0;
		for (u32 c = m_first[split]; c != kNone; c = m_next[c]) {
			if (m_style[c].position == Position::Absolute) continue;
			if (k++ == ordinal) return c;
		}
		return kNone;
	}
	u32 LayoutTree::gutterCount(NodeId split) const {
		u32 i = idx(split);
		if (i == kNone || m_kind[i] != static_cast<u8>(LayoutKind::Split)) return 0;
		u32 n = 0; for (u32 c = m_first[i]; c != kNone; c = m_next[c]) if (m_style[c].position != Position::Absolute) ++n;
		return n ? n - 1 : 0;
	}
	Rect LayoutTree::gutterRect(NodeId split, u32 g) const {
		u32 i = idx(split);
		if (i == kNone || g >= gutterCount(split)) return {};
		u32 a = flowKid(i, g), b = flowKid(i, g + 1);
		const Rect& ra = m_rect[a], & rb = m_rect[b];
		if (m_style[i].splitDir == SplitDir::Horizontal) return { ra.right(), ra.y, rb.x - ra.right(), ra.h };
		return { ra.x, ra.bottom(), ra.w, rb.y - ra.bottom() };
	}
	SplitDrag LayoutTree::splitDragBegin(NodeId split, u32 g) {
		SplitDrag d;
		u32 i = idx(split);
		if (i == kNone || g >= gutterCount(split)) return d;
		const bool horiz = m_style[i].splitDir == SplitDir::Horizontal;
		// Normalise weights to actual pixel sizes so that "what you see" and "what the solver holds" agree
		// (a pane pinned at its minimum would otherwise jump when its neighbour moves).
		for (u32 c = m_first[i]; c != kNone; c = m_next[c])
			if (m_style[c].position != Position::Absolute) m_style[c].splitWeight = std::max(1e-3f, horiz ? m_rect[c].w : m_rect[c].h);
		u32 a = flowKid(i, g), b = flowKid(i, g + 1);
		d.split = split; d.left = g; d.right = g + 1; d.active = true;
		d.startA = horiz ? m_rect[a].w : m_rect[a].h; d.startB = horiz ? m_rect[b].w : m_rect[b].h;
		return d;
	}
	void LayoutTree::splitDragUpdate(const SplitDrag& d, f32 delta) {
		u32 i = idx(d.split);
		if (!d.active || i == kNone) return;
		u32 a = flowKid(i, d.left), b = flowKid(i, d.right);
		if (a == kNone || b == kNone) return;
		const bool horiz = m_style[i].splitDir == SplitDir::Horizontal;
		auto minOf = [&](u32 c) { return std::max(m_style[c].splitMin, horiz ? m_style[c].minW : m_style[c].minH); };
		f32 minA = minOf(a), minB = minOf(b), total = d.startA + d.startB, A;
		if (total <= minA + minB) A = (minA + minB) > 0 ? total * minA / (minA + minB) : total * 0.5f;
		else A = std::min(std::max(d.startA + delta, minA), total - minB);
		m_style[a].splitWeight = std::max(1e-3f, A);
		m_style[b].splitWeight = std::max(1e-3f, total - A);
		markArrangeOnly(i);   // pane sizes changed; the split's own outer size and measure caches did not
	}
}
