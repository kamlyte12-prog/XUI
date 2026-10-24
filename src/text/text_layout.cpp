#include <xui/text/text_layout.hpp>
#include <algorithm>
#include <cmath>

namespace xui {

	FontHandle TextSystem::loadFont(std::string_view ttfData, std::string* error) {
		auto f = std::make_unique<Font>();
		if (!f->load(ttfData, error)) return FontHandle{};
		m_fonts.push_back(std::move(f));
		return FontHandle{ u32(m_fonts.size() - 1), 1 };
	}

	const Font* TextSystem::font(FontHandle h) const {
		return h.index < m_fonts.size() ? m_fonts[h.index].get() : nullptr;
	}

	FontHandle TextSystem::fontAtWeight(FontHandle base, f32 weight, std::string* error) {
		const Font* source = font(base);
		if (!source || !source->hasWeightAxis()) return base;
		const auto [minimum, maximum] = source->weightAxisRange();
		const f32 effectiveWeight = std::clamp(weight, minimum, maximum);
		const i32 coordinate = static_cast<i32>(std::llround(static_cast<double>(effectiveWeight) * 65536.0));
		for (const FontVariant& variant : m_variants)
			if (variant.base == base.index && variant.coordinate == coordinate) return variant.handle;

		auto instance = std::make_unique<Font>();
		if (!instance->load(source->m_data, error) || !instance->setWeightAxis(effectiveWeight, error)) return FontHandle{};
		m_fonts.push_back(std::move(instance));
		const FontHandle handle{ static_cast<u32>(m_fonts.size() - 1), 1 };
		m_variants.push_back({ base.index, coordinate, handle });
		return handle;
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
		struct TempGlyph { u32 g; f32 adv, xOffset, yOffset; };
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

        // Pass 1: shape each uninterrupted text run with HarfBuzz. Whitespace remains explicit so
        // the existing greedy line-break policy is preserved while GPOS sees every adjacent pair.
        std::vector<TempGlyph> tg;
        std::vector<Word> words;
        tg.reserve(text.size());
        words.reserve(16);
        std::size_t i = 0;
        while (i < text.size()) {
            const std::size_t cpStart = i;
            const u32 cp = decodeUtf8(text, i);
            if (cp == '\n') { words.push_back({ u32(tg.size()), 0, 0, 0 }); words.back().width = -1.f; continue; }
            if (isSpaceChar(cp)) {
                // A literal tab advances to a four-space stop. Other whitespace uses its own
                // font advance; leading whitespace retains the prior behavior and is ignored.
                const f32 adv = cp == '\t' ? f->advance(f->glyphIndex(' ')) * scale * 4.f
                    : f->advance(f->glyphIndex(cp)) * scale;
                if (!words.empty() && words.back().width != -1.f) {
                    words.back().spaceAfter += adv;
                    words.back().width += adv;
                }
                continue;
            }
            // Gather one run up to a breakable space or newline. Shape the segment as a
            // whole so pair positioning, including variable GPOS deltas, is applied.
            const std::size_t runStart = cpStart;
            while (i < text.size()) {
                const std::size_t next = i;
                const u32 nextCp = decodeUtf8(text, i);
                if (nextCp == '\n' || isSpaceChar(nextCp)) { i = next; break; }
            }
            Word w;
            w.glyphFirst = u32(tg.size());
            std::vector<Font::ShapedGlyph> shaped;
            const std::string_view run = text.substr(runStart, i - runStart);
            if (f->shapeText(run, shaped)) {
                for (const Font::ShapedGlyph& glyph : shaped) {
                    const f32 advance = glyph.xAdvance * scale;
                    tg.push_back({ glyph.glyph, advance, glyph.xOffset * scale, glyph.yOffset * scale });
                    w.width += advance;
                    ++w.glyphCount;
                }
            }
            else {
                // HarfBuzz allocation failure is rare; keep readable text with the built-in
                // cmap/advance path rather than dropping the run.
                std::size_t fallback = runStart;
                while (fallback < i) {
                    const u32 fallbackCp = decodeUtf8(text, fallback);
                    const u32 glyph = f->glyphIndex(fallbackCp);
                    const f32 advance = f->advance(glyph) * scale;
                    tg.push_back({ glyph, advance, 0.f, 0.f });
                    w.width += advance;
                    ++w.glyphCount;
                }
            }
            words.push_back(w);
        }
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
						const TempGlyph& glyph = tg[start + k];
						out.glyphs.push_back({ glyph.g, pen + glyph.xOffset, lineY - glyph.yOffset, glyph.adv });
						pen += glyph.adv;
					}
					remain -= fit;
					if (remain) breakLine();
				}
				continue;
			}
			for (u32 k = 0; k < w.glyphCount; ++k) {
				const TempGlyph& t = tg[w.glyphFirst + k];
				out.glyphs.push_back({ t.g, pen + t.xOffset, lineY - t.yOffset, t.adv });
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
