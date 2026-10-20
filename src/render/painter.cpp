#include <xui/render/painter.hpp>
#include <algorithm>
#include <cmath>
namespace xui {

	namespace {
		u16 q4(f32 px) { return static_cast<u16>(std::min(65535.f, std::max(0.f, std::round(px * 4.f)))); }
	}

	void Painter::begin(u32 w, u32 h, f32 dpi) {
		m_dpi = dpi > 0 ? dpi : 1.f; m_w = w; m_h = h; m_layer = 0; m_nextCustom = 1;
		m_cellsX = (w + (1u << kCellShift) - 1) >> kCellShift; m_cellsY = (h + (1u << kCellShift) - 1) >> kCellShift;
		m_clips.clear(); m_clips.push_back({ 0, 0, static_cast<i32>(w), static_cast<i32>(h) });
		m_transforms.clear(); m_transforms.push_back({});
		m_opacity.clear(); m_verts.clear(); m_prims.clear(); m_indices.clear(); m_cmds.clear();
		for (Layer& l : m_layers) {
			l.batches.clear(); l.entries.clear(); l.used = false; l.cellHead.assign(std::size_t(m_cellsX) * m_cellsY, kNil);
		}
		m_stats = {};
	}
	void Painter::setLayer(u32 l) { m_layer = std::min(l, kMaxLayers - 1); }
	void Painter::pushClip(Rect r) {
		const Mat3& transform = m_transforms.back();
		const Vec2 corners[] = { transform.apply({r.x, r.y}), transform.apply({r.right(), r.y}),
			transform.apply({r.right(), r.bottom()}), transform.apply({r.x, r.bottom()}) };
		f32 x0 = corners[0].x, x1 = corners[0].x, y0 = corners[0].y, y1 = corners[0].y;
		for (const Vec2 p : corners) { x0 = std::min(x0, p.x); x1 = std::max(x1, p.x); y0 = std::min(y0, p.y); y1 = std::max(y1, p.y); }
		ClipI c{ static_cast<i32>(std::floor(x0 * m_dpi)), static_cast<i32>(std::floor(y0 * m_dpi)),
				static_cast<i32>(std::ceil(x1 * m_dpi)), static_cast<i32>(std::ceil(y1 * m_dpi)) };
		const ClipI& t = m_clips.back();
		c = { std::max(c.x0, t.x0), std::max(c.y0, t.y0), std::min(c.x1, t.x1), std::min(c.y1, t.y1) };
		if (c.empty()) c = { t.x0, t.y0, t.x0, t.y0 };          // canonical empty clip: everything inside is culled
		m_clips.push_back(c);
	}
	void Painter::popClip() { if (m_clips.size() > 1) m_clips.pop_back(); }
	void Painter::pushTransform(Mat3 logical) { m_transforms.push_back(m_transforms.back() * logical); }
	void Painter::popTransform() { if (m_transforms.size() > 1) m_transforms.pop_back(); }
	void Painter::pushOpacity(f32 a) { m_opacity.push_back((m_opacity.empty() ? 1.f : m_opacity.back()) * std::min(std::max(a, 0.f), 1.f)); }
	void Painter::popOpacity() { if (!m_opacity.empty()) m_opacity.pop_back(); }

	u32 Painter::packAlpha(Color c) const { if (!m_opacity.empty()) c.a *= m_opacity.back(); return c.toRGBA8(); }

	Vertex Painter::makeVertex(f32 x, f32 y) const { Vertex v; v.x = x; v.y = y; return v; }

	bool Painter::admit(f32 x0, f32 y0, f32 x1, f32 y1, ClipI& b) const {
		const Mat3& transform = m_transforms.back();
		const Vec2 corners[] = { transform.apply({x0 / m_dpi, y0 / m_dpi}), transform.apply({x1 / m_dpi, y0 / m_dpi}),
			transform.apply({x1 / m_dpi, y1 / m_dpi}), transform.apply({x0 / m_dpi, y1 / m_dpi}) };
		x0 = x1 = corners[0].x * m_dpi; y0 = y1 = corners[0].y * m_dpi;
		for (const Vec2 p : corners) { x0 = std::min(x0, p.x * m_dpi); x1 = std::max(x1, p.x * m_dpi); y0 = std::min(y0, p.y * m_dpi); y1 = std::max(y1, p.y * m_dpi); }
		const ClipI& c = m_clips.back();
		b = { std::max(c.x0, static_cast<i32>(std::floor(x0))), std::max(c.y0, static_cast<i32>(std::floor(y0))),
			 std::min(c.x1, static_cast<i32>(std::ceil(x1))), std::min(c.y1, static_cast<i32>(std::ceil(y1))) };
		return !b.empty();
	}

	// Place a primitive (already written to m_verts at firstVert) into a batch.
	void Painter::commit(const Key& key, const ClipI& b, u32 firstVert, u32 quads) {
		Layer& L = m_layers[m_layer];
		L.used = true;
		const u32 pi = static_cast<u32>(m_prims.size());
		m_prims.push_back({ firstVert, quads, kNil });
		const i32 cx0 = b.x0 >> kCellShift, cy0 = b.y0 >> kCellShift, cx1 = (b.x1 - 1) >> kCellShift, cy1 = (b.y1 - 1) >> kCellShift;
		u32 lowest = 0;               // newest batch (+1) containing a primitive that really overlaps this one: we must land at or after it
		for (i32 cy = cy0; cy <= cy1; ++cy)
			for (i32 cx = cx0; cx <= cx1; ++cx)
				for (u32 e = L.cellHead[std::size_t(cy) * m_cellsX + std::size_t(cx)]; e != kNil; e = L.entries[e].next) {
					const Entry& en = L.entries[e];
					if (en.x0 < b.x1 && b.x0 < en.x1 && en.y0 < b.y1 && b.y0 < en.y1) lowest = std::max(lowest, en.batch + 1);
				}
		const u32 n = static_cast<u32>(L.batches.size());
		u32 k = kNil;
		if (m_opt.reorder) { for (u32 j = lowest ? lowest - 1 : 0; j < n; ++j) if (L.batches[j].key == key) { k = j; break; } }
		else if (n && L.batches[n - 1].key == key) k = n - 1;
		if (k == kNil) { L.batches.push_back({ key, kNil, kNil }); k = n; }
		Batch& B = L.batches[k];
		if (B.tail == kNil) B.head = pi; else m_prims[B.tail].next = pi;
		B.tail = pi;
		for (i32 cy = cy0; cy <= cy1; ++cy)
			for (i32 cx = cx0; cx <= cx1; ++cx) {
				u32& head = L.cellHead[std::size_t(cy) * m_cellsX + std::size_t(cx)];
				L.entries.push_back({ b.x0, b.y0, b.x1, b.y1, k, head });
				head = static_cast<u32>(L.entries.size() - 1);
			}
	}

	void Painter::quad(const Vertex& t, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1) {
		Vertex v = t;
		const Mat3& transform = m_transforms.back();
		auto write = [&](f32 x, f32 y, f32 u, f32 vv) {
			const Vec2 p = transform.apply({ x / m_dpi, y / m_dpi });
			v.x = p.x * m_dpi; v.y = p.y * m_dpi; v.u = u; v.v = vv; m_verts.push_back(v);
		};
		write(x0, y0, u0, v0); // TL
		write(x1, y0, u1, v0); // TR
		write(x1, y1, u1, v1); // BR
		write(x0, y1, u0, v1); // BL
	}

	// ---- primitives -----------------------------------------------------------------------------------------
	void Painter::fillRect(Rect r, Color c, CornerRadii radius) { RectStyle s; s.fill = c; s.radius = radius; roundRect(r, s); }

	void Painter::roundRect(Rect r, const RectStyle& s) {
		const bool gradient = s.gradient || s.gradientStopCount >= 2;
		if (gradient && s.borderWidth > 0 && s.border.a > 0) {        // gradient + border = gradient prim, then a ring prim on top
			RectStyle g = s; g.borderWidth = 0; rectPrim(r, g, false);
			RectStyle ring = s; ring.gradient = false; ring.gradientStopCount = 0; ring.fill = Color{ 0, 0, 0, 0 }; rectPrim(r, ring, true);
			return;
		}
		rectPrim(r, s, false);
	}

	void Painter::rectPrim(Rect r, const RectStyle& s, bool) {
		++m_stats.prims;
		const bool gradient = s.gradient || s.gradientStopCount >= 2;
		const bool hasBorder = s.borderWidth > 0 && s.border.a > 0 && !gradient;
		if (!gradient && s.fill.a <= 0 && !hasBorder) { ++m_stats.culled; return; }
		const f32 x0 = snapv(r.x * m_dpi), y0 = snapv(r.y * m_dpi), x1 = snapv((r.x + r.w) * m_dpi), y1 = snapv((r.y + r.h) * m_dpi);
		if (x1 <= x0 || y1 <= y0) { ++m_stats.culled; return; }
		ClipI b;
		const f32 bm = m_opt.snap ? 0.f : 1.f;     // snapped edges sit on pixel boundaries: the 1px quad margin has zero coverage
		if (!admit(x0 - bm, y0 - bm, x1 + bm, y1 + bm, b)) { ++m_stats.culled; return; }
		const f32 hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f, cx = x0 + hx, cy = y0 + hy, maxR = std::min(hx, hy);
		Vertex t;
		t.prim = static_cast<u8>(::xui::Prim::RoundRect); t.hx = hx; t.hy = hy;
		t.c0 = packAlpha(gradient && s.gradientStopCount >= 2 ? s.gradientStops[0] : s.fill);
		t.c1 = packAlpha(gradient && s.gradientStopCount >= 2 ? s.gradientStops[1] : (s.gradient ? s.gradientEnd : s.border));
		t.c2 = packAlpha(s.gradientStopCount >= 3 ? s.gradientStops[2] : (s.gradientStopCount >= 2 ? s.gradientStops[1] : Color{}));
		t.c3 = packAlpha(s.gradientStopCount >= 4 ? s.gradientStops[3] : (s.gradientStopCount >= 2 ? s.gradientStops[s.gradientStopCount - 1] : Color{}));
		t.rad[0] = q4(std::min(s.radius.tl * m_dpi, maxR)); t.rad[1] = q4(std::min(s.radius.tr * m_dpi, maxR));
		t.rad[2] = q4(std::min(s.radius.br * m_dpi, maxR)); t.rad[3] = q4(std::min(s.radius.bl * m_dpi, maxR));
		if (gradient) {
			t.flags |= FlagGradient;
			if (s.gradientStopCount >= 2) {
				if (s.gradientKind == 1) t.flags |= FlagGradientRadial;
				else if (s.gradientKind == 2) t.flags |= FlagGradientConic;
				if (s.gradientStopCount == 3) t.flags |= FlagGradientThreeStops;
				else if (s.gradientStopCount >= 4) t.flags |= FlagGradientFourStops;
			}
			if (s.gradientStopCount >= 2 && s.gradientKind == 2) t.aux0 = s.gradientAngle;
			else { t.aux0 = std::cos(s.gradientAngle); t.aux1 = std::sin(s.gradientAngle); }
		}
		else if (hasBorder) {
			f32 bw = s.borderWidth * m_dpi; if (m_opt.snap) bw = std::max(1.f, std::round(bw));
			t.border = q4(std::min(bw, maxR));
			if (s.dashed) { t.flags |= FlagDashed; t.aux0 = std::max(2.f, s.dashLength * m_dpi); }
		}
		const u32 first = static_cast<u32>(m_verts.size());
		quad(t, x0 - bm, y0 - bm, x1 + bm, y1 + bm, x0 - bm - cx, y0 - bm - cy, x1 + bm - cx, y1 + bm - cy);   // geometry == declared bounds
		commit(Key{ TextureHandle{}, {m_clips.back()}, 0 }, b, first, 1);
	}

	void Painter::shadow(Rect r, const ShadowStyle& s) {
		++m_stats.prims;
		if (s.color.a <= 0) { ++m_stats.culled; return; }
		f32 sx = r.x, sy = r.y, sw = r.w, sh = r.h;
		if (!s.inner) { sx += s.offset.x - s.spread; sy += s.offset.y - s.spread; sw += 2 * s.spread; sh += 2 * s.spread; }
		const f32 x0 = snapv(sx * m_dpi), y0 = snapv(sy * m_dpi), x1 = snapv((sx + sw) * m_dpi), y1 = snapv((sy + sh) * m_dpi);
		if (x1 <= x0 || y1 <= y0) { ++m_stats.culled; return; }
		const f32 blur = std::max(0.f, s.blur * m_dpi), margin = s.inner ? 1.f : blur * 0.5f + 1.f;
		ClipI b;
		if (!admit(x0 - margin, y0 - margin, x1 + margin, y1 + margin, b)) { ++m_stats.culled; return; }
		const f32 hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f, cx = x0 + hx, cy = y0 + hy, maxR = std::min(hx, hy);
		const f32 grow = s.inner ? 0.f : s.spread;
		Vertex t;
		t.prim = static_cast<u8>(::xui::Prim::Shadow); t.hx = hx; t.hy = hy; t.c0 = packAlpha(s.color); t.border = q4(blur);
		t.rad[0] = q4(std::min((s.radius.tl + grow) * m_dpi, maxR)); t.rad[1] = q4(std::min((s.radius.tr + grow) * m_dpi, maxR));
		t.rad[2] = q4(std::min((s.radius.br + grow) * m_dpi, maxR)); t.rad[3] = q4(std::min((s.radius.bl + grow) * m_dpi, maxR));
		if (s.inner) { t.flags |= FlagInner; t.aux0 = s.offset.x * m_dpi; t.aux1 = s.offset.y * m_dpi; }
		const u32 first = static_cast<u32>(m_verts.size());
		quad(t, x0 - margin, y0 - margin, x1 + margin, y1 + margin, x0 - margin - cx, y0 - margin - cy, x1 + margin - cx, y1 + margin - cy);
		commit(Key{ TextureHandle{}, {m_clips.back()}, 0 }, b, first, 1);
	}

	void Painter::glyphRun(TextureHandle atlas, const GlyphQuad* q, u32 n, Vec2 origin, Color c) {
		++m_stats.prims;
		if (n == 0 || c.a <= 0) { ++m_stats.culled; return; }
		f32 bx0 = 1e30f, by0 = 1e30f, bx1 = -1e30f, by1 = -1e30f;
		const u32 first = static_cast<u32>(m_verts.size());
		Vertex t; t.prim = static_cast<u8>(::xui::Prim::Glyph); t.c0 = packAlpha(c);
		for (u32 i = 0; i < n; ++i) {
			const f32 x0 = snapv((origin.x + q[i].dst.x) * m_dpi), y0 = snapv((origin.y + q[i].dst.y) * m_dpi);
			const f32 x1 = x0 + std::max(1.f, std::round(q[i].dst.w * m_dpi)), y1 = y0 + std::max(1.f, std::round(q[i].dst.h * m_dpi));
			quad(t, x0, y0, x1, y1, q[i].uv.x, q[i].uv.y, q[i].uv.x + q[i].uv.w, q[i].uv.y + q[i].uv.h);
			bx0 = std::min(bx0, x0); by0 = std::min(by0, y0); bx1 = std::max(bx1, x1); by1 = std::max(by1, y1);
		}
		ClipI b;
		if (!admit(bx0, by0, bx1, by1, b)) { m_verts.resize(first); ++m_stats.culled; return; }
		commit(Key{ atlas, {m_clips.back()}, 0 }, b, first, n);
	}

	void Painter::image(TextureHandle tex, Rect dst, Rect uv, Color tint, CornerRadii radius) {
		++m_stats.prims;
		const f32 x0 = snapv(dst.x * m_dpi), y0 = snapv(dst.y * m_dpi), x1 = snapv((dst.x + dst.w) * m_dpi), y1 = snapv((dst.y + dst.h) * m_dpi);
		if (x1 <= x0 || y1 <= y0 || tint.a <= 0) { ++m_stats.culled; return; }
		ClipI b;
		const f32 bmi = m_opt.snap ? 0.f : 1.f;
		if (!admit(x0 - bmi, y0 - bmi, x1 + bmi, y1 + bmi, b)) { ++m_stats.culled; return; }
		const f32 hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f, maxR = std::min(hx, hy);
		Vertex t; t.prim = static_cast<u8>(::xui::Prim::Image); t.c0 = packAlpha(tint); t.hx = hx; t.hy = hy; t.aux0 = x0 + hx; t.aux1 = y0 + hy;
		t.rad[0] = q4(std::min(radius.tl * m_dpi, maxR)); t.rad[1] = q4(std::min(radius.tr * m_dpi, maxR));
		t.rad[2] = q4(std::min(radius.br * m_dpi, maxR)); t.rad[3] = q4(std::min(radius.bl * m_dpi, maxR));
		const f32 sx = (x1 - x0) > 0 ? uv.w / (x1 - x0) : 0.f, sy = (y1 - y0) > 0 ? uv.h / (y1 - y0) : 0.f;   // texcoords for the 1px margin
		const u32 first = static_cast<u32>(m_verts.size());
		quad(t, x0 - bmi, y0 - bmi, x1 + bmi, y1 + bmi, uv.x - sx * bmi, uv.y - sy * bmi, uv.x + uv.w + sx * bmi, uv.y + uv.h + sy * bmi);
		commit(Key{ tex, {m_clips.back()}, 0 }, b, first, 1);
	}

	void Painter::capsule(Vec2 a, Vec2 b2, f32 width, Color c, Color c2, bool grad) {
		++m_stats.prims;
		if (c.a <= 0 && !(grad && c2.a > 0)) { ++m_stats.culled; return; }
		const f32 ax = a.x * m_dpi, ay = a.y * m_dpi, bx = b2.x * m_dpi, by = b2.y * m_dpi, r = std::max(0.5f, width * m_dpi * 0.5f);
		const f32 x0 = std::min(ax, bx) - r - 1, y0 = std::min(ay, by) - r - 1, x1 = std::max(ax, bx) + r + 1, y1 = std::max(ay, by) + r + 1;
		ClipI b;
		if (!admit(x0, y0, x1, y1, b)) { ++m_stats.culled; return; }
		const f32 mx = (ax + bx) * 0.5f, my = (ay + by) * 0.5f;
		Vertex t; t.prim = static_cast<u8>(::xui::Prim::Capsule); t.c0 = packAlpha(c); t.c1 = packAlpha(c2);
		t.hx = (bx - ax) * 0.5f; t.hy = (by - ay) * 0.5f; t.rad[0] = q4(r); if (grad) t.flags |= FlagGradient;
		const u32 first = static_cast<u32>(m_verts.size());
		quad(t, x0, y0, x1, y1, x0 - mx, y0 - my, x1 - mx, y1 - my);
		commit(Key{ TextureHandle{}, {m_clips.back()}, 0 }, b, first, 1);
	}
	void Painter::line(Vec2 a, Vec2 b, f32 w, Color c) { capsule(a, b, w, c, c, false); }
	void Painter::line(Vec2 a, Vec2 b, f32 w, Color c, Color c2) { capsule(a, b, w, c, c2, true); }
	void Painter::polyline(const Vec2* p, u32 n, f32 w, Color c) { for (u32 i = 0; i + 1 < n; ++i) capsule(p[i], p[i + 1], w, c, c, false); }
	void Painter::bezier(Vec2 p0, Vec2 c0, Vec2 c1, Vec2 p1, f32 w, Color c) {
		auto len = [](Vec2 a, Vec2 b) { return std::hypot(b.x - a.x, b.y - a.y); };
		const f32 approx = len(p0, c0) + len(c0, c1) + len(c1, p1);
		const u32 segs = static_cast<u32>(std::min(64.f, std::max(8.f, approx / 10.f)));
		Vec2 prev = p0;
		for (u32 i = 1; i <= segs; ++i) {
			const f32 t = f32(i) / f32(segs), u = 1 - t;
			const Vec2 p{ u * u * u * p0.x + 3 * u * u * t * c0.x + 3 * u * t * t * c1.x + t * t * t * p1.x,
						 u * u * u * p0.y + 3 * u * u * t * c0.y + 3 * u * t * t * c1.y + t * t * t * p1.y };
			capsule(prev, p, w, c, c, false); prev = p;
		}
	}

	void Painter::custom(Rect r, u32 id) {
		++m_stats.prims;
		const f32 x0 = std::floor(r.x * m_dpi), y0 = std::floor(r.y * m_dpi), x1 = std::ceil((r.x + r.w) * m_dpi), y1 = std::ceil((r.y + r.h) * m_dpi);
		ClipI b;
		if (!admit(x0, y0, x1, y1, b)) { ++m_stats.culled; return; }
		ClipI clip = b;                                   // the command's scissor is the visible part of the reserved rect
		commit(Key{ TextureHandle{}, clip, (m_nextCustom++ << 16) | (id & 0xFFFFu) }, b, static_cast<u32>(m_verts.size()), 0);
	}

	// ---- finish -----------------------------------------------------------------------------------------------
	DrawList Painter::finish() {
		m_stats.batches = 0;
		for (u32 li = 0; li < kMaxLayers; ++li) {
			const Layer& L = m_layers[li];
			for (const Batch& B : L.batches) {
				++m_stats.batches;
				DrawCmd cmd; cmd.tex = B.key.tex; cmd.idxOffset = static_cast<u32>(m_indices.size());
				cmd.clip = { B.key.clip.x0, B.key.clip.y0, B.key.clip.x1 - B.key.clip.x0, B.key.clip.y1 - B.key.clip.y0 };
				if (B.key.custom) { cmd.kind = DrawCmd::Custom; cmd.customId = B.key.custom & 0xFFFFu; }
				for (u32 p = B.head; p != kNil; p = m_prims[p].next)
					for (u32 q = 0; q < m_prims[p].quads; ++q) {
						const u32 v = m_prims[p].firstVert + q * 4;
						m_indices.push_back(v); m_indices.push_back(v + 1); m_indices.push_back(v + 2);
						m_indices.push_back(v); m_indices.push_back(v + 2); m_indices.push_back(v + 3);
					}
				cmd.idxCount = static_cast<u32>(m_indices.size()) - cmd.idxOffset;
				m_cmds.push_back(cmd);
			}
		}
		m_stats.drawCalls = static_cast<u32>(m_cmds.size());
		m_stats.vertices = static_cast<u32>(m_verts.size()); m_stats.indices = static_cast<u32>(m_indices.size());
		DrawList dl;
		dl.verts = m_verts.data(); dl.nVerts = m_stats.vertices; dl.idx = m_indices.data(); dl.nIdx = m_stats.indices;
		dl.cmds = m_cmds.data(); dl.nCmds = m_stats.drawCalls;
		return dl;
	}
}
