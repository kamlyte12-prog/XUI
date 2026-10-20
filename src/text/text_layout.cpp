#include <xui/text/text_layout.hpp>
#include <algorithm>
#include <cmath>

namespace xui {

	FontHandle TextSystem::loadFont(std::string_view ttfData, std::string* error) {
		Font f;
		if (!f.load(ttfData, error)) return FontHandle{};
		m_fonts.push_back(std::move(f));
		return FontHandle{ u32(m_fonts.size() - 1), 1 };
	}

	const Font* TextSystem::font(FontHandle h) const {
		return h.index < m_fonts.size() ? &m_fonts[h.index] : nullptr;
	}

	bool TextSystem::initAtlas(Renderer* r, u32 atlasSize) { return m_atlas.init(r, atlasSize); }

	namespace {
		/// UTF-8 decode one codepoint; advances i. Invalid bytes decode as U+FFFD.
		u32 decodeUtf8(std::string_view s, std::size_t& i) {
			unsigned char c = u8(s[i]);
			std::size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
			if (i + n > s.size()) { ++i; return 0xFFFD; }
			u32 cp = 0;
			if (n == 1) cp = c;
			else {
				cp = c & (0xFFu >> (n + 1));
				for (std::size_t j = i + 1; j < i + n; ++j) {
					if ((u8(s[j]) & 0xC0) != 0x80) { ++i; return 0xFFFD; }
					cp = (cp << 6) | (s[j] & 0x3F);
				}
			}
			i += n;
			return cp;
		}
		bool isSpaceChar(u32 cp) { return cp == ' ' || cp == '\t' || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x3000; }

		/// A shaped word: glyphs are consecutive in the word list; spaces are attached to the preceding word.
		struct Word {
			u32 glyphFirst = 0, glyphCount = 0;   // into the temp glyph array
			f32 width = 0;                        // ink advance including the trailing space, if any
			f32 spaceAfter = 0;                   // width of the trailing space (dropped at line ends)
		};
		struct TempGlyph { u32 g; f32 adv; };
	} // namespace

	const TextLayout& TextSystem::layout(FontHandle fh, std::string_view text, f32 sizePx, f32 wrapWidth) {
		// Match GlyphAtlas::glyph(), which rounds its raster size. Shaping and raster bounds must
		// use the same pixel size or tight Row children can be measured narrower than their glyphs.
		u16 size = u16(std::clamp(std::lround(sizePx), 1l, 65535l));
		CacheKey key{ fh.index, hashText(text), size,
					 wrapWidth > 0 ? i32(std::floor(wrapWidth)) : 0x7FFFFFFF };

		if (m_index.size() < 64) m_index.assign(64, 0);
		u32 mask = u32(m_index.size() - 1);
		u32 slot = key.hash & mask;
		while (m_index[slot]) {
			CacheEntry& e = m_cache[m_index[slot] - 1];
			if (e.key == key && std::string_view(e.text) == text) return e.layout;
			slot = (slot + 1) & mask;
		}
		return shape(fh, text, key);
	}

	const TextLayout& TextSystem::shape(FontHandle fh, std::string_view text, const CacheKey& key) {
		const Font* f = font(fh);
		if (!f) { static const TextLayout empty; return empty; }

		TextLayout out;
		out.sizePx = key.sizePx;
		out.lineHeight = f->lineHeight(f32(key.sizePx));
		f32 scale = f->scaleFor(f32(key.sizePx));
		f32 wrap = key.wrap == 0x7FFFFFFF ? 0.f : f32(key.wrap);

		// Pass 1: shape into words (kerned advances). Positions come in pass 2.
		std::vector<TempGlyph> tg;
		std::vector<Word> words;
		tg.reserve(text.size());
		words.reserve(16);
		u32 prevG = 0;
		std::size_t i = 0;
		bool pendingSpace = false;
		while (i < text.size()) {
			u32 cp = decodeUtf8(text, i);
			if (cp == '\n') { words.push_back({ u32(tg.size()), 0, 0, 0 }); words.back().width = -1.f; prevG = 0; pendingSpace = false; continue; }
			u32 g = f->glyphIndex(cp);
			f32 adv = f->advance(g) * scale;
			// A literal tab is non-rendering whitespace with a predictable four-space indent.
			// Measure it from the actual space glyph (some fonts give control glyphs a nonzero advance).
			// TextArea key input expands tabs to the configured number of spaces before shaping.
			if (cp == '\t') adv = f->advance(f->glyphIndex(' ')) * scale * 4.f;
			if (isSpaceChar(cp)) {
				pendingSpace = true;                 // widths resolve when the next word starts
				if (!words.empty()) words.back().spaceAfter += adv;
				prevG = 0;
				continue;
			}
			Word w;
			w.glyphFirst = u32(tg.size());
			if (pendingSpace && !words.empty()) {
				words.back().width += words.back().spaceAfter;   // the space belongs to the flow
			}
			pendingSpace = false;
			if (prevG) {
				i16 kx = 0;
				if (f->kerning(prevG, g, kx)) adv += kx * scale;
			}
			tg.push_back({ g, adv });
			w.glyphCount = 1;
			w.width = adv;
			words.push_back(w);
			prevG = g;
		}
		// merge single-glyph words split only by kerning into fewer word records is unnecessary: each
		// decoded char became one Word; merge runs of non-space chars (same word) by folding widths.
		// (Decoding above creates one Word per char; fold consecutive non-space words into one.)
		std::vector<Word> folded;
		folded.reserve(words.size());
		for (const Word& w : words) {
			if (w.width == -1.f) { folded.push_back(w); continue; }          // newline marker
			if (!folded.empty() && folded.back().width != -1.f && folded.back().spaceAfter == 0.f) {
				folded.back().glyphCount += w.glyphCount;
				folded.back().width += w.width;
				folded.back().spaceAfter = w.spaceAfter;
			}
			else
				folded.push_back(w);
		}
		words.swap(folded);

		// Pass 2: greedy word wrap; assign pen positions. A line starts when lineStarts grows; the last
		// line stays open until the loop ends (no phantom empty line for text not ending in '\n').
		out.glyphs.reserve(tg.size());
		out.lineStarts.push_back(0);
		out.lineWidths.push_back(0.f);
		f32 pen = 0, widest = 0, lineY = 0;
		bool lineHasContent = false;
		auto breakLine = [&] {
			widest = std::max(widest, pen);
			out.lineWidths.back() = pen;
			pen = 0;
			lineY += out.lineHeight;
			lineHasContent = false;
			out.lineStarts.push_back(u32(out.glyphs.size()));
			out.lineWidths.push_back(0.f);
			};
		for (u32 wi = 0; wi < words.size(); ++wi) {
			const Word& w = words[wi];
			if (w.width == -1.f) { breakLine(); continue; }                  // explicit newline
			f32 wordW = w.width - w.spaceAfter;                              // trailing space hangs, never wraps with it
			if (wrap > 0 && lineHasContent && pen + wordW > wrap) breakLine();
			// Hard-break a single word wider than the whole line (rare: long paths / URLs).
			if (wrap > 0 && !lineHasContent && wordW > wrap && w.glyphCount > 1) {
				u32 remain = w.glyphCount;
				while (remain) {
					u32 start = w.glyphFirst + (w.glyphCount - remain);
					u32 fit = 0;
					f32 wp = 0;
					while (fit < remain && wp + tg[start + fit].adv <= wrap) { wp += tg[start + fit].adv; ++fit; }
					if (!fit) fit = 1;                                       // one oversized char per line
					for (u32 k = 0; k < fit; ++k) {
						out.glyphs.push_back({ tg[start + k].g, pen, lineY, tg[start + k].adv });
						pen += tg[start + k].adv;
					}
					remain -= fit;
					if (remain) breakLine();
				}
				continue;
			}
			for (u32 k = 0; k < w.glyphCount; ++k) {
				const TempGlyph& t = tg[w.glyphFirst + k];
				out.glyphs.push_back({ t.g, pen, lineY, t.adv });
				pen += t.adv;
			}
			if (w.spaceAfter) pen += w.spaceAfter;   // room for the next word on this line (dropped on wrap)
			lineHasContent = true;
		}
		widest = std::max(widest, pen);
		out.lineWidths.back() = pen;
		out.advanceWidth = widest;
		// Intrinsic width must contain the rasterized glyphs, not just their pen advances.
		// Column children are usually stretched, which hid this; auto-sized Row children can
		// clip glyph ink and the atlas's one-pixel antialiasing border at exactly this width.
		const f32 inkScale = f->scaleFor(f32(key.sizePx));
		f32 inkLeft = 0.f, inkRight = widest;
		for (const ShapedGlyph& glyph : out.glyphs) {
			const Rect bounds = f->bounds(glyph.glyph);
			if (bounds.empty()) continue;
			const f32 rasterLeft = std::floor(bounds.x * inkScale) - 1.f;
			const f32 rasterRight = std::ceil(bounds.right() * inkScale) + 1.f;
			inkLeft = std::min(inkLeft, glyph.x + rasterLeft);
			inkRight = std::max(inkRight, glyph.x + rasterRight);
		}
		out.inkLeft = inkLeft;
		out.width = inkRight - inkLeft;
		out.height = out.lineHeight * f32(out.lineStarts.size());

		// Cache insert (clear-all when full: a miss storm is cheaper than LRU for small UI working sets).
		if (m_used >= kMaxCache) {
			m_cache.clear();
			m_index.assign(64, 0);
			m_used = 0;
		}
		if (m_used * 2 >= m_index.size()) m_index.resize(m_index.size() * 2, 0);
		u32 mask = u32(m_index.size() - 1);
		u32 slot = key.hash & mask;
		while (m_index[slot]) slot = (slot + 1) & mask;
		m_cache.push_back(CacheEntry{ key, std::string(text), std::move(out) });
		m_index[slot] = u32(m_cache.size());
		++m_used;
		return m_cache.back().layout;
	}

} // namespace xui
