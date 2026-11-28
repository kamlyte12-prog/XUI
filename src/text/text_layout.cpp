#include <xui/text/text_layout.hpp>
#include <algorithm>
#include <chrono>
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

	bool TextSystem::addFallbackFont(FontHandle family, FontHandle fallback) {
		if (!font(family) || !font(fallback) || family == fallback)
			return false;
		auto found = std::find_if(m_fallbacks.begin(), m_fallbacks.end(),
								  [&](const FontFallback& chain) { return chain.family == family; });
		if (found == m_fallbacks.end()) {
			m_fallbacks.push_back({family, {fallback}});
		} else {
			if (std::find(found->faces.begin(), found->faces.end(), fallback) != found->faces.end())
				return true;
			found->faces.push_back(fallback);
		}
		++m_fallbackGeneration;
		// Keep existing TextLayout references alive through the current frame. The generation
		// prevents stale cache hits; beginFrame() reclaims the old entries safely.
		m_cacheClearPending = true;
		return true;
	}

	FontHandle TextSystem::fontForCodepoint(FontHandle primary, u32 codepoint) const {
		const Font* base = font(primary);
		if (base && base->glyphIndex(codepoint))
			return primary;
		for (const FontFallback& chain : m_fallbacks) {
			bool applies = chain.family == primary;
			if (!applies)
				for (const FontVariant& variant : m_variants)
					if (variant.handle == primary && variant.base == chain.family.index) {
						applies = true;
						break;
					}
			if (!applies)
				continue;
			for (FontHandle candidate : chain.faces) {
				const Font* face = font(candidate);
				if (face && face->glyphIndex(codepoint))
					return candidate;
			}
			break;
		}
		return primary;
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
		u32 decodeCodepointAt(std::string_view s, std::size_t offset) {
			if (offset >= s.size())
				return 0xFFFDu;
			return decodeUtf8(s, offset);
		}
		std::size_t nextUtf8Boundary(std::string_view s, std::size_t i) {
			if (i >= s.size())
				return s.size();
			const u8 c = u8(s[i]);
			const std::size_t count = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
			return std::min(s.size(), i + count);
		}
		bool isSpaceChar(u32 cp) { return cp == ' ' || cp == '\t' || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x3000; }
		bool isRtlCodepoint(u32 cp) {
			return (cp >= 0x0590u && cp <= 0x08FFu) || (cp >= 0xFB1Du && cp <= 0xFDFFu) ||
				   (cp >= 0xFE70u && cp <= 0xFEFFu) || (cp >= 0x10800u && cp <= 0x10FFFu);
		}

		/// A shaped word: glyphs are consecutive in the word list; spaces are attached to the preceding word.
		struct Word {
			bool rtl = false;
			u32 glyphFirst = 0, glyphCount = 0;   // into the temp glyph array
			f32 width = 0;                        // ink advance including the trailing space, if any
			f32 spaceAfter = 0;                   // width of the trailing space (dropped at line ends)
		};
		struct TempGlyph {
			u32 g;
			f32 adv, xOffset, yOffset;
			u32 clusterStart, clusterEnd;
			FontHandle face;
			std::array<f32, 8> ligatureCarets{};
			u8 ligatureCaretCount = 0;
			bool rtl = false;
		};
	} // namespace

	const TextLayout& TextSystem::layout(FontHandle fh, std::string_view text, f32 sizePx, f32 wrapWidth) {
		++m_frameStats.layoutRequests;
		// Match GlyphAtlas::glyph(), which rounds its raster size. Shaping and raster bounds must
		// use the same pixel size or tight Row children can be measured narrower than their glyphs.
		u16 size = u16(std::clamp(std::lround(sizePx), 1l, 65535l));
		const u32 textHash = hashText(text);
		CacheKey key{fh.index, textHash ^ (m_fallbackGeneration * 0x9e3779b9u), m_fallbackGeneration, size,
					 wrapWidth > 0 ? i32(std::ceil(wrapWidth)) : 0x7FFFFFFF};

		if (m_index.size() < 64) m_index.assign(64, 0);
		u32 mask = u32(m_index.size() - 1);
		u32 slot = key.hash & mask;
		while (m_index[slot]) {
			CacheEntry& e = m_cache[m_index[slot] - 1];
			if (e.key == key && std::string_view(e.text) == text) {
				++m_frameStats.layoutCacheHits;
				return e.layout;
			}
			slot = (slot + 1) & mask;
		}
		++m_frameStats.layoutCacheMisses;
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
			if (cp == '\n') {
				Word marker;
				marker.glyphFirst = static_cast<u32>(tg.size());
				marker.width = -1.f;
				words.push_back(marker);
				continue;
			}
			if (isSpaceChar(cp)) {
                // A literal tab advances to a four-space stop. Other whitespace uses its own
                // font advance; leading whitespace retains the prior behavior and is ignored.
				const FontHandle spaceFace = fontForCodepoint(fh, cp == '\t' ? ' ' : cp);
				const Font* spaceFont = font(spaceFace);
				const f32 spaceScale = spaceFont ? spaceFont->scaleFor(f32(key.sizePx)) : scale;
				const f32 adv = cp == '\t' ? spaceFont->advance(spaceFont->glyphIndex(' ')) * spaceScale * 4.f
										   : spaceFont->advance(spaceFont->glyphIndex(cp)) * spaceScale;
				if (!words.empty() && words.back().width != -1.f) {
                    words.back().spaceAfter += adv;
                    words.back().width += adv;
                }
                continue;
            }
            // Gather one run up to a breakable space or newline. Shape the segment as a
            // whole so pair positioning, including variable GPOS deltas, is applied.
            const std::size_t runStart = cpStart;
			const bool runDirectionRtl = isRtlCodepoint(cp);
            while (i < text.size()) {
                const std::size_t next = i;
                const u32 nextCp = decodeUtf8(text, i);
				if (nextCp == '\n' || isSpaceChar(nextCp) || isRtlCodepoint(nextCp) != runDirectionRtl) {
					i = next;
					break;
				}
			}
            Word w;
            w.glyphFirst = u32(tg.size());
			w.rtl = runDirectionRtl;
			std::vector<Font::ShapedGlyph> shaped;
			std::size_t segmentStart = runStart;
			while (segmentStart < i) {
				std::size_t cpEnd = segmentStart;
				(void)decodeUtf8(text, cpEnd);
				const FontHandle faceHandle = fontForCodepoint(fh, decodeCodepointAt(text, segmentStart));
				while (cpEnd < i) {
					const std::size_t next = cpEnd;
					const u32 nextCodepoint = decodeCodepointAt(text, next);
					std::size_t after = next;
					(void)decodeUtf8(text, after);
					if (fontForCodepoint(fh, nextCodepoint) != faceHandle)
						break;
					cpEnd = after;
				}
				const Font* face = font(faceHandle);
				if (!face) {
					segmentStart = cpEnd;
					continue;
				}
				const f32 faceScale = face->scaleFor(f32(key.sizePx));
				shaped.clear();
				const auto shapingStart = std::chrono::steady_clock::now();
				const bool shapingSucceeded = face->shapeText(text.substr(segmentStart, cpEnd - segmentStart), shaped);
				const auto shapingEnd = std::chrono::steady_clock::now();
				const auto shapingNanoseconds =
					std::chrono::duration_cast<std::chrono::nanoseconds>(shapingEnd - shapingStart).count();
				m_frameStats.shapingTimeNanoseconds += static_cast<u64>(std::max<std::int64_t>(0, shapingNanoseconds));
				++m_frameStats.shapingRuns;
				if (shapingSucceeded) {
					m_frameStats.shapedGlyphs += static_cast<u32>(shaped.size());
					const bool runRtl = !shaped.empty() && shaped.front().rtl;
					std::vector<u32> clusters;
					clusters.reserve(shaped.size());
					for (const auto& glyph : shaped)
						clusters.push_back(glyph.cluster);
					std::sort(clusters.begin(), clusters.end());
					clusters.erase(std::unique(clusters.begin(), clusters.end()), clusters.end());
					for (const Font::ShapedGlyph& glyph : shaped) {
						const f32 advance = std::abs(glyph.xAdvance * faceScale);
						const std::size_t clusterStart = segmentStart + glyph.cluster;
						const auto nextCluster = std::upper_bound(clusters.begin(), clusters.end(), glyph.cluster);
						const std::size_t localEnd =
							nextCluster == clusters.end() ? cpEnd - segmentStart : *nextCluster;
						TempGlyph temp{glyph.glyph,
									   advance,
									   glyph.xOffset * faceScale,
									   glyph.yOffset * faceScale,
									   static_cast<u32>(clusterStart),
									   static_cast<u32>(segmentStart + localEnd),
									   faceHandle,
									   {},
									   0,
									   runRtl};
						temp.ligatureCaretCount = glyph.ligatureCaretCount;
						for (u8 caret = 0; caret < temp.ligatureCaretCount; ++caret)
							temp.ligatureCarets[caret] = std::abs(glyph.ligatureCarets[caret] * faceScale);
						tg.push_back(temp);
						w.width += advance;
						++w.glyphCount;
					}
				} else {
					std::size_t fallback = segmentStart;
					while (fallback < cpEnd) {
						const std::size_t clusterStart = fallback;
						const u32 fallbackCp = decodeUtf8(text, fallback);
						const u32 glyph = face->glyphIndex(fallbackCp);
						const f32 advance = face->advance(glyph) * faceScale;
						tg.push_back({glyph, advance, 0.f, 0.f, static_cast<u32>(clusterStart),
									  static_cast<u32>(fallback), faceHandle});
						++m_frameStats.shapedGlyphs;
						w.width += advance;
						++w.glyphCount;
					}
				}
				segmentStart = cpEnd;
			}
            words.push_back(w);
        }
		// Basic paragraph bidi: strong Arabic/Hebrew runs were split above and HarfBuzz
		// shaped each run in its script direction. Reverse run order in RTL-base paragraphs,
		// retaining each run's already-shaped visual glyph order and its inter-run spacing.
		for (std::size_t begin = 0; begin < words.size();) {
			std::size_t end = begin;
			while (end < words.size() && words[end].width != -1.f)
				++end;
			const std::size_t count = end - begin;
			if (count && words[begin].rtl) {
				std::vector<f32> separators;
				separators.reserve(count);
				for (std::size_t k = begin; k < end; ++k)
					separators.push_back(words[k].spaceAfter);
				std::reverse(words.begin() + static_cast<std::ptrdiff_t>(begin),
							 words.begin() + static_cast<std::ptrdiff_t>(end));
				for (std::size_t k = 0; k < count; ++k) {
					const std::size_t separatorIndex = k + 1 < count ? count - 2 - k : count;
					Word& word = words[begin + k];
					word.spaceAfter = separatorIndex < separators.size() ? separators[separatorIndex] : 0.f;
					word.width = word.spaceAfter;
					for (u32 glyph = 0; glyph < word.glyphCount; ++glyph)
						word.width += tg[word.glyphFirst + glyph].adv;
				}
			}
			begin = end < words.size() ? end + 1 : end;
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
		auto appendGlyph = [&](const TempGlyph& glyph) {
			const u32 line = static_cast<u32>(out.lineStarts.size() - 1);
			ShapedGlyph placed{};
			placed.glyph = glyph.g;
			placed.x = pen + glyph.xOffset;
			placed.y = lineY - glyph.yOffset;
			placed.advance = glyph.adv;
			placed.font = glyph.face;
			placed.clusterStart = glyph.clusterStart;
			placed.clusterEnd = glyph.clusterEnd;
			placed.line = line;
			placed.ligatureCarets = glyph.ligatureCarets;
			placed.ligatureCaretCount = glyph.ligatureCaretCount;
			placed.rtl = glyph.rtl;
			out.glyphs.push_back(placed);
			std::vector<u32> interior;
			for (std::size_t byte = glyph.clusterStart; byte < glyph.clusterEnd;) {
				byte = nextUtf8Boundary(text, byte);
				if (byte < glyph.clusterEnd)
					interior.push_back(static_cast<u32>(byte));
			}
			for (std::size_t caret = 0; caret < interior.size(); ++caret) {
				const f32 position = caret < glyph.ligatureCaretCount ? glyph.ligatureCarets[caret]
																	  : glyph.adv * static_cast<f32>(caret + 1) /
																			static_cast<f32>(interior.size() + 1);
				const f32 x = glyph.rtl ? pen + glyph.adv - position : pen + position;
				out.caretStops.push_back({interior[caret], line, x});
			}
			pen += glyph.adv;
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
						appendGlyph(glyph);
					}
					remain -= fit;
					if (remain) breakLine();
				}
				continue;
			}
			for (u32 k = 0; k < w.glyphCount; ++k) {
				const TempGlyph& t = tg[w.glyphFirst + k];
				appendGlyph(t);
			}
			if (w.spaceAfter) pen += w.spaceAfter;   // room for the next word on this line (dropped on wrap)
			lineHasContent = true;
		}
		widest = std::max(widest, pen);
		out.lineWidths.back() = pen;
		out.advanceWidth = widest;
		// Build logical caret stops once alongside shaping. Editors can then map text positions
		// to pixels (and pointer positions back to text) without allocating/shaping prefixes.
		for (const ShapedGlyph& glyph : out.glyphs) {
			const f32 clusterStartX = glyph.rtl ? glyph.x + glyph.advance : glyph.x;
			const f32 clusterEndX = glyph.rtl ? glyph.x : glyph.x + glyph.advance;
			if (out.caretStops.empty() || out.caretStops.back().byteOffset != glyph.clusterStart)
				out.caretStops.push_back({glyph.clusterStart, glyph.line, clusterStartX});
			if (!out.caretStops.empty() && out.caretStops.back().byteOffset == glyph.clusterStart &&
				out.caretStops.back().line == glyph.line)
				out.caretStops.back().x = clusterStartX;
			out.caretStops.push_back({glyph.clusterEnd, glyph.line, clusterEndX});
		}
		std::sort(out.caretStops.begin(), out.caretStops.end(), [](const TextCaretStop& a, const TextCaretStop& b) {
			return a.byteOffset < b.byteOffset || (a.byteOffset == b.byteOffset && a.line < b.line);
		});
		out.caretStops.erase(std::unique(out.caretStops.begin(), out.caretStops.end(),
										 [](const TextCaretStop& a, const TextCaretStop& b) {
											 return a.byteOffset == b.byteOffset && a.line == b.line;
										 }),
							 out.caretStops.end());
		if (out.caretStops.empty() || out.caretStops.front().byteOffset != 0)
			out.caretStops.insert(out.caretStops.begin(), {0, 0, 0.f});
		if (out.caretStops.back().byteOffset != text.size())
			out.caretStops.push_back(
				{static_cast<u32>(text.size()), static_cast<u32>(out.lineWidths.size() - 1), out.lineWidths.back()});
		// Spaces and explicit newlines do not emit glyphs. Fill their character boundaries
		// between adjacent shaped clusters so hit-testing still has a stable target for each.
		std::vector<TextCaretStop> completeStops;
		completeStops.reserve(text.size() + 1);
		for (std::size_t offset = 0; offset <= text.size();) {
			auto next = std::lower_bound(out.caretStops.begin(), out.caretStops.end(), static_cast<u32>(offset),
										 [](const TextCaretStop& stop, u32 value) { return stop.byteOffset < value; });
			if (next != out.caretStops.end() && next->byteOffset == offset) {
				const auto afterSameOffset =
					std::upper_bound(next, out.caretStops.end(), static_cast<u32>(offset),
									 [](u32 value, const TextCaretStop& stop) { return value < stop.byteOffset; });
				completeStops.push_back(*(afterSameOffset - 1)); // wrap boundaries prefer the next visual line
			} else {
				const TextCaretStop* before = next == out.caretStops.begin() ? nullptr : &*(next - 1);
				const TextCaretStop* after = next == out.caretStops.end() ? nullptr : &*next;
				if (!before)
					completeStops.push_back(after ? *after : TextCaretStop{});
				else if (!after || before->line != after->line)
					completeStops.push_back(*before);
				else {
					const f32 t = static_cast<f32>(offset - before->byteOffset) /
								  static_cast<f32>(std::max<u32>(1, after->byteOffset - before->byteOffset));
					completeStops.push_back(
						{static_cast<u32>(offset), before->line, before->x + (after->x - before->x) * t});
				}
			}
			if (offset == text.size())
				break;
			offset = nextUtf8Boundary(text, offset);
		}
		out.caretStops = std::move(completeStops);
		out.visualCaretStops = out.caretStops;
		std::sort(out.visualCaretStops.begin(), out.visualCaretStops.end(),
				  [](const TextCaretStop& a, const TextCaretStop& b) {
					  return a.line < b.line ||
							 (a.line == b.line && (a.x < b.x || (a.x == b.x && a.byteOffset < b.byteOffset)));
				  });
		// Intrinsic width must contain the rasterized glyphs, not just their pen advances.
		// Column children are usually stretched, which hid this; auto-sized Row children can
		// clip glyph ink and the atlas's one-pixel antialiasing border at exactly this width.
		f32 inkLeft = 0.f, inkRight = widest;
		for (const ShapedGlyph& glyph : out.glyphs) {
			const Font* glyphFont = font(glyph.font.valid() ? glyph.font : fh);
			if (!glyphFont)
				continue;
			const f32 inkScale = glyphFont->scaleFor(f32(key.sizePx));
			const Rect bounds = glyphFont->bounds(glyph.glyph);
			if (bounds.empty()) continue;
			const f32 rasterLeft = std::floor(bounds.x * inkScale) - 1.f;
			const f32 rasterRight = std::ceil(bounds.right() * inkScale) + 1.f;
			inkLeft = std::min(inkLeft, glyph.x + rasterLeft);
			inkRight = std::max(inkRight, glyph.x + rasterRight);
		}
		out.inkLeft = inkLeft;
		out.width = inkRight - inkLeft;
		out.height = out.lineHeight * f32(out.lineStarts.size());

		// Do not clear the cache here: callers may still hold references returned by an earlier
		// layout() call in this same paint/layout pass. Reclaim at beginFrame() instead.
		if (m_used >= kMaxCache)
			m_cacheClearPending = true;
		if (m_used * 2 >= m_index.size()) {
			m_index.assign(m_index.size() * 2, 0);
			const u32 newMask = u32(m_index.size() - 1);
			for (u32 entryIndex = 0; entryIndex < m_cache.size(); ++entryIndex) {
				u32 rehashSlot = m_cache[entryIndex].key.hash & newMask;
				while (m_index[rehashSlot]) rehashSlot = (rehashSlot + 1) & newMask;
				m_index[rehashSlot] = entryIndex + 1;
			}
			++m_frameStats.layoutCacheRehashes;
		}
		u32 mask = u32(m_index.size() - 1);
		u32 slot = key.hash & mask;
		while (m_index[slot]) slot = (slot + 1) & mask;
		m_cache.push_back(CacheEntry{ key, std::string(text), std::move(out) });
		m_index[slot] = u32(m_cache.size());
		++m_used;
		return m_cache.back().layout;
	}

	TextCaretStop TextLayout::caretAt(u32 byteOffset) const {
		if (caretStops.empty())
			return {};
		// Use indices rather than vector iterators here. Editor lookups run while layouts are
		// frequently appended to the TextSystem cache; bounded indices make the boundary cases
		// (empty text, wrapped duplicate stops, and end-of-buffer) explicit and safe.
		std::size_t first = 0, last = caretStops.size();
		while (first < last) {
			const std::size_t middle = first + (last - first) / 2;
			if (caretStops[middle].byteOffset < byteOffset)
				first = middle + 1;
			else
				last = middle;
		}
		if (first == 0)
			return caretStops.front();
		if (first >= caretStops.size())
			return caretStops.back();
		if (caretStops[first].byteOffset == byteOffset) {
			std::size_t after = first;
			while (after < caretStops.size() && caretStops[after].byteOffset == byteOffset)
				++after;
			return caretStops[after - 1];
		}
		const TextCaretStop& before = caretStops[first - 1];
		const TextCaretStop& after = caretStops[first];
		return byteOffset - before.byteOffset <= after.byteOffset - byteOffset ? before : after;
	}

} // namespace xui
