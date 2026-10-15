#include <xui/text/atlas.hpp>
#include <xui/text/rasterizer.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace xui {

	bool GlyphAtlas::init(Renderer* r, u32 initialSize) {
		shutdown();
		m_renderer = r;
		u32 size = std::clamp(initialSize, kMinSize, kMaxSize);
		TextureDesc d;
		d.width = size; d.height = size;
		d.format = TextureFormat::A8;
		d.linearFilter = true;
		m_tex = r->createTexture(d);                 // starts zeroed
		if (!m_tex.valid()) { m_renderer = nullptr; return false; }
		m_size = size;
		m_shelves.clear();
		m_entries.clear();
		m_index.assign(256, 0);
		m_used = 0;
		return true;
	}

	void GlyphAtlas::shutdown() {
		if (m_renderer && m_tex.valid()) m_renderer->destroyTexture(m_tex);
		if (m_renderer) for (TextureHandle texture : m_retiredTextures) m_renderer->destroyTexture(texture);
		m_retiredTextures.clear();
		m_tex = TextureHandle{};
		m_renderer = nullptr;
		m_size = 0;
	}

	void GlyphAtlas::beginFrame() {
		++m_frame;
		// A rebuild can happen while a draw list is being assembled. Retired atlas handles remain
		// valid through that submit and are safe to release at the start of the following frame.
		if (m_renderer) for (TextureHandle texture : m_retiredTextures) m_renderer->destroyTexture(texture);
		m_retiredTextures.clear();
	}

	u32 GlyphAtlas::findSlot(const GlyphKey& key) const {
		if (m_index.empty()) return kNone;
		u32 mask = u32(m_index.size() - 1);
		u32 slot = u32(GlyphKeyHash{}(key)) & mask;
		for (u32 probes = 0; probes < m_index.size() && m_index[slot]; ++probes) {
			if (m_entries[m_index[slot] - 1].key == key) return m_index[slot] - 1;
			slot = (slot + 1) & mask;
		}
		return kNone;
	}

	void GlyphAtlas::reindex() {
		// Linear probing uses a bit mask, so capacity must remain a power of two.
		// Keep load below 50% to make lookups short even for colliding glyph keys.
		u32 capacity = 256;
		const u64 required = std::max<u64>(256, u64(m_entries.size()) * 2);
		while (capacity < required && capacity <= 0x7FFFFFFFu) capacity <<= 1;
		m_index.assign(capacity, 0);
		u32 mask = u32(m_index.size() - 1);
		for (u32 i = 0; i < m_entries.size(); ++i) {
			u32 slot = u32(GlyphKeyHash{}(m_entries[i].key)) & mask;
			u32 probes = 0;
			while (m_index[slot] && probes++ < m_index.size()) slot = (slot + 1) & mask;
			if (probes >= m_index.size()) return; // Defensive: do not spin on a corrupt/full index.
			m_index[slot] = i + 1;
		}
		m_used = u32(m_entries.size());
	}

	void GlyphAtlas::indexEntry(u32 entryIndex) {
		if (m_index.empty() || u64(m_entries.size()) * 2 >= m_index.size()) {
			reindex();
			return;
		}
		const u32 mask = u32(m_index.size() - 1);
		u32 slot = u32(GlyphKeyHash{}(m_entries[entryIndex].key)) & mask;
		for (u32 probes = 0; probes < m_index.size(); ++probes) {
			if (!m_index[slot]) {
				m_index[slot] = entryIndex + 1;
				++m_used;
				return;
			}
			slot = (slot + 1) & mask;
		}
		reindex();
	}

	/// Larger atlas: recreate the texture, re-pack and re-rasterise every cached glyph.
	void GlyphAtlas::rebuild(u32 newSize) {
		std::vector<Entry> old;
		old.swap(m_entries);
		TextureHandle oldTex = m_tex;
		u32 oldSize = m_size;

		TextureDesc d;
		d.width = newSize; d.height = newSize;
		d.format = TextureFormat::A8;
		d.linearFilter = true;
		m_tex = m_renderer->createTexture(d);
		m_size = newSize;
		m_shelves.clear();
		m_index.clear();
		m_used = 0;
		m_entries.reserve(old.size() + 8);

		for (const Entry& e : old)
			if (e.font) rasterise(*e.font, e.key);   // re-rasterises and re-uploads at new positions
		if (oldTex.valid()) m_retiredTextures.push_back(oldTex);
		(void)oldSize;
	}

	void GlyphAtlas::evictAll() {
		// Full reset (bitmap positions are meaningless afterwards). Only reached at kMaxSize with a
		// working set larger than 4096x4096 coverage px — never in a normal UI.
		m_entries.clear();
		m_shelves.clear();
		m_index.assign(256, 0);
		m_used = 0;
	}

	bool GlyphAtlas::place(u32 w, u32 h, u32& outX, u32& outY) {
		for (Shelf& s : m_shelves) {
			if (h <= s.height && s.x + w <= m_size) {
				outX = s.x; outY = s.y;
				s.x += w;
				return true;
			}
		}
		u32 y = 0;
		for (const Shelf& s : m_shelves) y = std::max(y, s.y + s.height);
		if (y + h <= m_size) {
			m_shelves.push_back({ y, h, w });
			outX = 0; outY = y;
			return true;
		}
		return false;
	}

	const AtlasGlyph& GlyphAtlas::glyph(const Font& font, u32 glyphIndex, u16 sizePx) {
		if (!sizePx) sizePx = 1;
		GlyphKey key{ glyphIndex, sizePx, 0 };
		u32 slot = findSlot(key);
		if (slot != kNone) {
			Entry& e = m_entries[slot];
			e.g.lastFrame = u32(m_frame);
			return e.g;
		}
		return rasterise(font, key);
	}

	const AtlasGlyph& GlyphAtlas::rasterise(const Font& font, GlyphKey key) {
		++m_rasters;
		f32 s = font.scaleFor(f32(key.sizePx));
		Rect b = font.bounds(key.glyph);             // font units, y up
		// pixel-space ink box, padded 1px for AA bleed; y measured UP from the baseline
		f32 inkL = b.x * s, inkR = b.right() * s, inkB = b.y * s, inkT = b.bottom() * s;
		i32 x0 = 0, y0 = 0, x1 = 1, y1 = 1;
		u32 w = 1, h = 1;
		if (!b.empty()) {
			x0 = i32(std::floor(inkL)) - 1;
			y0 = i32(std::floor(inkB)) - 1;
			x1 = i32(std::ceil(inkR)) + 1;
			y1 = i32(std::ceil(inkT)) + 1;
			w = u32(std::max(0, x1 - x0));
			h = u32(std::max(0, y1 - y0));
		}                                               // whitespace: 1x1 empty cell

		u32 px, py;
		bool ok = place(w, h, px, py);
		if (!ok) {
			if (m_size < kMaxSize) {
				rebuild(m_size * 2);
				ok = place(w, h, px, py);
			}
			if (!ok) {
				evictAll();
				ok = place(w, h, px, py);
			}
			if (!ok) {                                // atlas at max size and still cannot fit: degrade
				Entry dead{};
				dead.key = key; dead.font = &font; dead.g.lastFrame = u32(m_frame);
				m_entries.push_back(dead);
				indexEntry(u32(m_entries.size() - 1));
				return m_entries.back().g;
			}
		}

		Entry e{};
		e.key = key;
		e.font = &font;
		e.g.lastFrame = u32(m_frame);
		e.g.uv = Rect{ f32(px) / m_size, f32(py) / m_size, f32(w) / m_size, f32(h) / m_size };
		// bitmap (0,0) sits pen-relative at (x0, -y1): x0 <= ink left, y1 >= ink top (y-down space)
		e.g.offset = Vec2{ f32(x0), f32(-f32(y1)) };
		e.g.advance = font.advance(key.glyph) * s;
		e.g.width = f32(w); e.g.height = f32(h);

		// Rasterize into the glyph-local bitmap; updateTexture applies the atlas offset later.
		// Including px/py here moves every non-first glyph outside its small w*h raster target.
		OutlineBuffer buf;
		font.outline(key.glyph, { &buf, [](void* c, Vec2 p) { static_cast<OutlineBuffer*>(c)->moveTo(p); },
									  [](void* c, Vec2 p) { static_cast<OutlineBuffer*>(c)->lineTo(p); } });
		if (!buf.pts.empty()) {
			std::vector<Vec2> pts;
			pts.reserve(buf.pts.size());
			for (Vec2 p : buf.pts) pts.push_back({ p.x * s - f32(x0), f32(y1) - p.y * s });
			std::vector<f32> scratch;
			m_bitmap.resize(std::size_t(w) * h);
			rasterizeContours(pts.data(), u32(pts.size()), buf.starts.data(), u32(buf.starts.size()),
				m_bitmap.data(), w, h, scratch);
			Recti region{ i32(px), i32(py), i32(w), i32(h) };
			m_renderer->updateTexture(m_tex, m_bitmap.data(), region);
		}
		m_entries.push_back(std::move(e));
		indexEntry(u32(m_entries.size() - 1));
		return m_entries.back().g;
	}

} // namespace xui
