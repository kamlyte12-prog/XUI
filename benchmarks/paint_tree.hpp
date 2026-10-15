#pragma once
// Paints a LayoutTree the way a Blender-like UI would: every leaf is a widget (rounded, bordered rect + a text run),
// panels get a shadow, scroll areas clip, splits draw gutters. Not part of the library (the scene graph arrives in M7).
#include "blender_ui.hpp"
#include <xui/render/painter.hpp>
namespace blender {
	struct PaintCtx { Painter* p; const LayoutTree* t; TextureHandle atlas; u32 widgets = 0; };

	inline void paintNode(PaintCtx& c, NodeId n, Vec2 origin, int depth) {
		const LayoutTree& t = *c.t; Painter& p = *c.p;
		const Rect r = t.rect(n), a{ origin.x + r.x, origin.y + r.y, r.w, r.h };
		const LayoutKind k = t.kind(n);
		if (k == LayoutKind::Leaf) {
			if (r.w < 1.f || r.h < 1.f) return;
			++c.widgets;
			static const Color fills[4] = { {0.27f, 0.27f, 0.29f, 1}, {0.33f, 0.33f, 0.35f, 1}, {0.23f, 0.35f, 0.55f, 1}, {0.30f, 0.30f, 0.32f, 1} };
			RectStyle s; s.fill = fills[(n.index * 2654435761u >> 28) & 3]; s.border = { 0, 0, 0, 0.4f }; s.borderWidth = 1; s.radius = CornerRadii::all(3);
			p.roundRect(a, s);
			// Text is fitted to its widget (a real layout elides): glyphs that would overflow the box are dropped, so a label never
			// paints over its neighbour. (An earlier version let 48px of text overflow 46px-wide widgets, which forced one draw call
			// per widget in tight rows - the batcher was right to refuse to reorder across that overlap.)
			const int fit = std::min(6, std::max(0, int((a.w - 8.f) / 8.f)));
			GlyphQuad q[6];
			for (int i = 0; i < fit; ++i) { const f32 u = f32((n.index + u32(i) * 5) & 15) / 16.f; q[i] = { {f32(i) * 8.f, 0, 7, 11}, {u, 0, 1.f / 16.f, 1} }; }
			if (fit > 0) p.glyphRun(c.atlas, q, u32(fit), { a.x + 4, a.y + (a.h - 11) * 0.5f }, { 0.92f, 0.92f, 0.92f, 1 });
			return;
		}
		if (k == LayoutKind::Column && depth >= 4) {                     // a collapsible properties panel
			p.shadow(a, { {0, 0, 0, 0.35f}, {0, 1}, 6, 0, CornerRadii::all(4) });
			p.fillRect(a, { 0.24f, 0.24f, 0.25f, 1 }, CornerRadii::all(4));
		}
		else if (depth <= 2 || k == LayoutKind::Split || k == LayoutKind::Scroll || k == LayoutKind::Stack) {
			p.fillRect(a, { 0.16f + 0.01f * f32(depth), 0.16f + 0.01f * f32(depth), 0.17f + 0.01f * f32(depth), 1 });
		}
		const Vec2 so = t.scrollOffset(n), childOrigin{ a.x - so.x, a.y - so.y };
		const bool clip = k == LayoutKind::Scroll;
		if (clip) p.pushClip(a);
		if (k == LayoutKind::Split)
			for (u32 g = 0; g < t.gutterCount(n); ++g) { const Rect gr = t.gutterRect(n, g); p.fillRect({ a.x + gr.x, a.y + gr.y, gr.w, gr.h }, { 0.06f, 0.06f, 0.07f, 1 }); }
		for (NodeId ch = t.firstChild(n); ch.valid(); ch = t.nextSibling(ch)) paintNode(c, ch, childOrigin, depth + 1);
		if (clip) p.popClip();
	}
	/// 16 pseudo-glyphs (8x12 each) in a 128x12 A8 atlas: enough texture variety to make text runs look like text.
	inline std::vector<u8> makeAtlasPixels() {
		std::vector<u8> a(128 * 12, 0);
		for (int g = 0; g < 16; ++g) for (int y = 1; y < 11; ++y) for (int x = 1; x < 7; ++x) {
			const u32 h = u32(g * 131 + y * 17 + x * 7) * 2654435761u;
			const bool on = ((h >> 20) & 3) != 0 && (x == 1 || x == 6 || y == 1 || y == 10 || ((h >> 24) & 1));
			a[std::size_t(y) * 128 + std::size_t(g * 8 + x)] = on ? 235 : 0;
		}
		return a;
	}
}
