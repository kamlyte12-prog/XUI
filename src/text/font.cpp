#include <xui/text/font.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <unordered_set>
#include <utility>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H
#include <hb.h>
#include <hb-ot.h>

namespace xui {
	namespace { std::atomic<u32> s_nextFontCacheId{ 1 }; }

	struct Font::VariationState {
		FT_Library library = nullptr;
		FT_Face face = nullptr;
		FT_MM_Var* axes = nullptr;
		FT_UInt weightAxis = 0;
		FT_Fixed minimum = 0, maximum = 0;
		~VariationState() {
			if (axes && library) FT_Done_MM_Var(library, axes);
			if (face) FT_Done_Face(face);
			if (library) FT_Done_FreeType(library);
		}
	};

	struct Font::ShapingState {
		hb_blob_t* blob = nullptr;
		hb_face_t* face = nullptr;
		hb_font_t* font = nullptr;
		~ShapingState() {
			if (font) hb_font_destroy(font);
			if (face) hb_face_destroy(face);
			if (blob) hb_blob_destroy(blob);
		}
	};

	namespace {
		template<class Variation>
		void traceWeightAtGlyphLoad(const Variation& variation, u32 glyph) {
			static const bool enabled = std::getenv("XUI_TRACE_FONT_WEIGHT") != nullptr;
			if (!enabled) return;
			static std::unordered_set<u64> seen;
			const u64 key = static_cast<u64>(reinterpret_cast<std::uintptr_t>(variation.face)) ^ (u64(glyph) << 1);
			if (!seen.insert(key).second) return;
			std::vector<FT_Fixed> coordinates(variation.axes->num_axis);
			if (FT_Get_Var_Design_Coordinates(variation.face, variation.axes->num_axis, coordinates.data())) {
				std::fprintf(stderr, "font-weight glyph-load: glyph=%u FT_Get_Var_Design_Coordinates=failed\n", glyph);
				return;
			}
			const FT_Fixed coordinate = coordinates[variation.weightAxis];
			std::fprintf(stderr, "font-weight glyph-load: glyph=%u axis=wght fixed=%ld applied=%.3f\n",
				glyph, static_cast<long>(coordinate), static_cast<double>(coordinate) / 65536.0);
		}
	}

	Font::Font() = default;
	Font::~Font() = default;

	bool Font::hasWeightAxis() const { return m_variation != nullptr; }

	std::pair<f32, f32> Font::weightAxisRange() const {
		if (!m_variation) return { 400.f, 400.f };
		return { static_cast<f32>(m_variation->minimum) / 65536.f,
			static_cast<f32>(m_variation->maximum) / 65536.f };
	}

	bool Font::setWeightAxis(f32 weight, std::string* error) {
		if (!m_variation) return false;
		std::vector<FT_Fixed> coords(m_variation->axes->num_axis);
		if (FT_Get_Var_Design_Coordinates(m_variation->face, m_variation->axes->num_axis, coords.data())) {
			for (FT_UInt i = 0; i < m_variation->axes->num_axis; ++i) coords[i] = m_variation->axes->axis[i].def;
		}
		const FT_Fixed requested = static_cast<FT_Fixed>(std::llround(static_cast<double>(weight) * 65536.0));
		coords[m_variation->weightAxis] = std::clamp(requested, m_variation->minimum, m_variation->maximum);
		if (FT_Set_Var_Design_Coordinates(m_variation->face, m_variation->axes->num_axis, coords.data())) {
			if (error) *error = "FreeType could not apply the requested wght axis coordinate";
			return false;
		}
		m_appliedWeight = static_cast<f32>(coords[m_variation->weightAxis]) / 65536.f;
		if (m_shaping) {
			const hb_variation_t variation{ HB_TAG('w', 'g', 'h', 't'), m_appliedWeight };
			hb_font_set_variations(m_shaping->font, &variation, 1);
		}
		if (std::getenv("XUI_TRACE_FONT_WEIGHT")) {
			std::fprintf(stderr, "font-weight FT_Set_Var_Design_Coordinates: wght requested=%.3f fixed=%ld applied=%.3f HarfBuzz-wght=%.3f range=%.3f..%.3f default=%.3f result=success\n",
				static_cast<double>(weight), static_cast<long>(coords[m_variation->weightAxis]),
				static_cast<double>(m_appliedWeight), static_cast<double>(m_shaping ? m_appliedWeight : -1.f),
				static_cast<double>(weightAxisRange().first),
				static_cast<double>(weightAxisRange().second), static_cast<double>(m_weightAxisDefault));
		}
		m_metrics.ascender = static_cast<f32>(m_variation->face->ascender);
		m_metrics.descender = static_cast<f32>(m_variation->face->descender);
		m_metrics.lineGap = static_cast<f32>(m_variation->face->height) - m_metrics.ascender + m_metrics.descender;
		return true;
	}

	namespace {
		struct OutlineContext { OutlineBuffer outline; };
		int ftMoveTo(const FT_Vector* p, void* user) {
			static_cast<OutlineContext*>(user)->outline.moveTo({ static_cast<f32>(p->x), static_cast<f32>(p->y) });
			return 0;
		}
		int ftLineTo(const FT_Vector* p, void* user) {
			static_cast<OutlineContext*>(user)->outline.lineTo({ static_cast<f32>(p->x), static_cast<f32>(p->y) });
			return 0;
		}
		int ftConicTo(const FT_Vector* control, const FT_Vector* to, void* user) {
			auto& outline = static_cast<OutlineContext*>(user)->outline;
			if (outline.pts.empty()) return 1;
			const Vec2 from = outline.pts.back();
			const Vec2 c{ static_cast<f32>(control->x), static_cast<f32>(control->y) };
			const Vec2 end{ static_cast<f32>(to->x), static_cast<f32>(to->y) };
			constexpr int steps = 8;
			for (int i = 1; i <= steps; ++i) {
				const f32 t = static_cast<f32>(i) / steps, u = 1.f - t;
				outline.lineTo({ u * u * from.x + 2.f * u * t * c.x + t * t * end.x,
					u * u * from.y + 2.f * u * t * c.y + t * t * end.y });
			}
			return 0;
		}
		int ftCubicTo(const FT_Vector*, const FT_Vector*, const FT_Vector*, void*) { return 1; }
	}

	// ---- table directory --------------------------------------------------------------------------

	u32 Font::tableOffset(std::string_view tag) const {
		u16 n = u16At(4);
		for (u16 i = 0; i < n; ++i) {
			u32 rec = 12 + u32(i) * 16;
			if (m_data.compare(rec, 4, tag) == 0) return u32At(rec + 8);
		}
		return 0;
	}

	bool Font::load(std::string_view data, std::string* error) {
		m_variation.reset();
		m_shaping.reset();
		auto fail = [&](const char* why) {
			if (error) *error = why;
			m_variation.reset();
			m_shaping.reset();
			m_data.clear();
			return false;
			};
		if (data.size() < 12) return fail("file too small for an sfnt directory");
		m_data.assign(data);
		u32 magic = u32At(0);
		if (magic != 0x00010000u && magic != 0x74727565u /*'true'*/ && magic != 0x4F54544Fu /*'OTTO'*/)
			return fail("not a TrueType/OpenType font (bad sfnt version)");
		m_cff = (magic == 0x4F54544Fu);

		if (!(m_head = tableOffset("head"))) return fail("missing head table");
		if (!(m_maxp = tableOffset("maxp"))) return fail("missing maxp table");
		if (!(m_hhea = tableOffset("hhea"))) return fail("missing hhea table");
		if (!(m_hmtx = tableOffset("hmtx"))) return fail("missing hmtx table");
		if (!(m_loca = tableOffset("loca"))) return fail("missing loca table");
		if (!(m_glyf = tableOffset("glyf"))) return fail("missing glyf table");

		m_metrics.unitsPerEm = u16At(m_head + 18);
		if (m_metrics.unitsPerEm < 16 || m_metrics.unitsPerEm > 16384) return fail("bad unitsPerEm in head");
		m_longLoca = i16At(m_head + 50) != 0;
		m_numGlyphs = u16At(m_maxp + 4);
		m_metrics.glyphCount = m_numGlyphs;

		m_metrics.ascender = i16At(m_hhea + 4);
		m_metrics.descender = i16At(m_hhea + 6);
		m_metrics.lineGap = i16At(m_hhea + 8);
		m_metrics.underlinePosition = i16At(m_hhea + 10);
		m_metrics.underlineThickness = std::max<i16>(1, i16At(m_hhea + 12));
		m_numHMetrics = u16At(m_hhea + 34);
		if (m_numHMetrics == 0 || m_numHMetrics > m_numGlyphs) return fail("bad numberOfHMetrics");

		u32 cmap = tableOffset("cmap");
		if (!cmap || !parseCmap(cmap)) return fail("no usable cmap subtable (need format 4 or 12)");

		if (u32 os2 = tableOffset("OS/2")) {
			m_metrics.typoAscender = i16At(os2 + 68);
			m_metrics.typoDescender = i16At(os2 + 70);
		}
		if (u32 name = tableOffset("name")) parseName(name);
		if ((m_kern = tableOffset("kern"))) parseKern(m_kern);
		m_metrics.hasKernTable = !m_kernPairs.empty();

		// Keep the font bytes owned by Font; HarfBuzz uses them for OpenType shaping and
		// resolves GPOS variation data independently for each cached Font weight instance.
		m_shaping = std::make_unique<ShapingState>();
		m_shaping->blob = hb_blob_create(m_data.data(), static_cast<unsigned>(m_data.size()),
			HB_MEMORY_MODE_READONLY, nullptr, nullptr);
		if (!m_shaping->blob || hb_blob_get_length(m_shaping->blob) == 0)
			return fail("HarfBuzz could not create a font blob");
		m_shaping->face = hb_face_create(m_shaping->blob, 0);
		if (!m_shaping->face || hb_face_get_glyph_count(m_shaping->face) == 0)
			return fail("HarfBuzz could not read the font face");
		m_shaping->font = hb_font_create(m_shaping->face);
		if (!m_shaping->font) return fail("HarfBuzz could not create a shaping font");
		hb_ot_font_set_funcs(m_shaping->font);
		const int shapingScale = static_cast<int>(m_metrics.unitsPerEm);
		hb_font_set_scale(m_shaping->font, shapingScale, shapingScale);

		// Keep the existing zero-dependency parser for static faces. Variable `glyf` fonts need
		// FreeType to evaluate fvar/gvar and HVAR data at a concrete design coordinate.
		if (tableOffset("fvar")) {
			auto variation = std::make_unique<VariationState>();
			if (FT_Init_FreeType(&variation->library) ||
				FT_New_Memory_Face(variation->library, reinterpret_cast<const FT_Byte*>(m_data.data()),
					static_cast<FT_Long>(m_data.size()), 0, &variation->face) ||
				FT_Get_MM_Var(variation->face, &variation->axes)) {
				return fail("FreeType could not initialize the variable font");
			}
			bool foundWeight = false;
			for (FT_UInt i = 0; i < variation->axes->num_axis; ++i) {
				const FT_Var_Axis& axis = variation->axes->axis[i];
				if (axis.tag == FT_MAKE_TAG('w', 'g', 'h', 't')) {
					variation->weightAxis = i;
					variation->minimum = axis.minimum;
					variation->maximum = axis.maximum;
					m_weightAxisDefault = static_cast<f32>(axis.def) / 65536.f;
					foundWeight = true;
					break;
				}
			}
			if (foundWeight) {
				m_variation = std::move(variation);
				if (!setWeightAxis(400, error)) { m_variation.reset(); m_data.clear(); return false; }
			}
		}
		m_cacheId = s_nextFontCacheId.fetch_add(1, std::memory_order_relaxed);
		return true;
	}

	bool Font::shapeText(std::string_view utf8, std::vector<ShapedGlyph>& glyphs) const {
		if (!m_shaping || !m_shaping->font || utf8.empty() ||
			utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
		hb_buffer_t* buffer = hb_buffer_create();
		if (!buffer) return false;
		hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
		hb_buffer_add_utf8(buffer, utf8.data(), static_cast<int>(utf8.size()), 0,
			static_cast<int>(utf8.size()));
		hb_buffer_guess_segment_properties(buffer);
		// The current layout/caret model is left-to-right. Disable ligature substitutions so
		// editable text retains a stable relationship between input characters and glyphs.
		hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
		const hb_feature_t features[]{
			{ HB_TAG('l', 'i', 'g', 'a'), 0, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
			{ HB_TAG('c', 'l', 'i', 'g'), 0, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END }
		};
		hb_shape(m_shaping->font, buffer, features, 2);
		unsigned count = 0;
		const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &count);
		const hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, &count);
		if (!hb_buffer_allocation_successful(buffer) || (!infos && count) || (!positions && count)) {
			hb_buffer_destroy(buffer);
			return false;
		}
		glyphs.clear();
		glyphs.reserve(count);
		for (unsigned i = 0; i < count; ++i) {
			glyphs.push_back({ infos[i].codepoint, static_cast<f32>(positions[i].x_advance),
				static_cast<f32>(positions[i].x_offset), static_cast<f32>(positions[i].y_offset), infos[i].cluster });
		}
		hb_buffer_destroy(buffer);
		return true;
	}

	// ---- character map ----------------------------------------------------------------------------

	bool Font::parseCmap(u32 off) {
		u16 n = u16At(off + 2);
		for (u16 i = 0; i < n; ++i) {
			u32 rec = off + 4 + u32(i) * 8;
			u16 plat = u16At(rec), enc = u16At(rec + 2);
			u32 sub = off + u32At(rec + 4);
			if (sub + 2 > m_data.size()) continue;
			u16 fmt = u16At(sub);
			if ((plat == 3 && (enc == 1 || enc == 10)) || (plat == 0 && (enc == 4 || enc == 6))) {
				if (fmt == 4 && !m_cmap4) m_cmap4 = sub;
				if (fmt == 12 && !m_cmap12) m_cmap12 = sub;
			}
			else if (plat == 3 && enc == 0 && fmt == 4 && !m_cmap4) {
				m_cmap4 = sub;   // symbol fonts: chars map through 0xF000; the plain lookup still works
			}
		}
		return m_cmap4 || m_cmap12;
	}

	u32 Font::glyphIndex(u32 cp) const {
		if (m_cmap12) {   // full unicode: binary search the groups
			u32 n = u32At(m_cmap12 + 12);
			u32 lo = 0, hi = n;
			while (lo < hi) {
				u32 mid = (lo + hi) / 2, g = m_cmap12 + 16 + mid * 12;
				u32 start = u32At(g), end = u32At(g + 4);
				if (cp < start) hi = mid;
				else if (cp > end) lo = mid + 1;
				else return u32At(g + 8) + (cp - start);
			}
		}
		if (m_cmap4 && cp <= 0xFFFF) {
			u32 t = m_cmap4;
			u16 segCount = u16At(t + 6) / 2;
			const u32 endBase = t + 14, startBase = endBase + 2 * segCount + 2,
				deltaBase = startBase + 2 * segCount, rangeBase = deltaBase + 2 * segCount;
			u32 lo = 0, hi = segCount;
			while (lo < hi) {                       // first segment whose end code >= cp
				u32 mid = (lo + hi) / 2;
				if (u16At(endBase + mid * 2) < cp) lo = mid + 1;
				else hi = mid;
			}
			if (lo >= segCount) return 0;
			u32 seg = lo;
			if (cp < u16At(startBase + seg * 2)) return 0;
			u16 delta = u16At(deltaBase + seg * 2), rangeOff = u16At(rangeBase + seg * 2);
			if (!rangeOff) return (cp + delta) & 0xFFFFu;
			u32 idAddr = rangeBase + seg * 2 + rangeOff + (cp - u16At(startBase + seg * 2)) * 2;
			u16 g = u16At(idAddr);
			return g ? (g + delta) & 0xFFFFu : 0;
		}
		return 0;
	}

	// ---- metrics ----------------------------------------------------------------------------------

	f32 Font::advance(u32 g) const {
		if (g >= m_numGlyphs) return 0;
		if (m_variation) traceWeightAtGlyphLoad(*m_variation, g);
		if (m_variation && !FT_Load_Glyph(m_variation->face, g,
			FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM))
			return static_cast<f32>(m_variation->face->glyph->metrics.horiAdvance);
		u32 entry = g < m_numHMetrics ? g : m_numHMetrics - 1;
		return u16At(m_hmtx + entry * 4);
	}

	bool Font::kerning(u32 left, u32 right, i16& outX) const {
		u64 key = (u64(left) << 32) | right;
		auto it = std::lower_bound(m_kernPairs.begin(), m_kernPairs.end(), key,
			[](const auto& p, u64 k) { return p.first < k; });
		if (it == m_kernPairs.end() || it->first != key) return false;
		outX = i16(it->second);
		return true;
	}

	// ---- kern / name ------------------------------------------------------------------------------

	bool Font::parseKern(u32 off) {
		u16 nSub = u16At(off + 2);
		u32 p = off + 4;
		for (u16 i = 0; i < nSub; ++i) {
			if (p + 6 > m_data.size()) break;
			u16 length = u16At(p + 2);
			u16 coverage = u16At(p + 4);
			u16 format = coverage >> 8;
			bool horizontal = !(coverage & 1);
			if (format == 0 && horizontal && p + 14 <= m_data.size()) {
				u16 nPairs = u16At(p + 6);
				u32 q = p + 14;
				m_kernPairs.reserve(m_kernPairs.size() + nPairs);
				for (u16 k = 0; k < nPairs && q + 6 <= m_data.size(); ++k, q += 6)
					m_kernPairs.emplace_back((u64(u16At(q)) << 32) | u16At(q + 2), u32(i16At(q + 4)));
			}
			if (!length) break;
			p += length;
		}
		std::sort(m_kernPairs.begin(), m_kernPairs.end(),
			[](const auto& a, const auto& b) { return a.first < b.first; });
		return true;
	}

	void Font::parseName(u32 off) {
		u16 count = u16At(off + 2);
		u16 strOff = u16(off + u16At(off + 4));
		for (u16 i = 0; i < count; ++i) {
			u32 rec = off + 6 + u32(i) * 12;
			if (u16At(rec + 6) != 1) continue;   // family name
			u16 plat = u16At(rec), len = u16At(rec + 8), str = u16At(rec + 10);
			u32 base = strOff + str;
			if (base + len > m_data.size()) continue;
			std::string out;
			if (plat == 0 || plat == 3) {                 // UTF-16BE -> UTF-8
				for (u16 j = 0; j + 1 < len; j += 2) {
					u32 cp = u16At(base + j);
					if (cp >= 0xD800 && cp < 0xDC00 && j + 3 < len) {
						cp = 0x10000 + ((cp - 0xD800) << 10) + (u16At(base + j + 2) - 0xDC00);
						j += 2;
					}
					if (cp < 0x80) out += char(cp);
					else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 63)); }
					else if (cp < 0x10000) { out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
					else { out += char(0xF0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 63)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
				}
			}
			else
				for (u16 j = 0; j < len; ++j) out += m_data[base + j];
			if (!out.empty()) { m_metrics.family = std::move(out); break; }
		}
	}

	// ---- outlines ---------------------------------------------------------------------------------
	// Points are collected into one flat OutlineBuffer, then replayed into the caller's sink. Composite
	// transforms are composed as Xform 2x3 matrices during the descent, so nested composites need no
	// callback re-wrapping.

	bool Font::glyphData(u32 g, u32& off, u32& len) const {
		if (g >= m_numGlyphs) return false;
		u32 a, b;
		if (m_longLoca) { a = u32At(m_loca + g * 4); b = u32At(m_loca + g * 4 + 4); }
		else { a = u32(u16At(m_loca + g * 2)) * 2; b = u32(u16At(m_loca + g * 2 + 2)) * 2; }
		if (b <= a) { off = 0; len = 0; return true; }       // empty glyph (space)
		off = m_glyf + a;
		len = b - a;
		return off + len <= m_data.size();
	}

	void Font::emitSimple(u32 off, u32 len, const Xform& xf, OutlineBuffer& b) const {
		if (len < 10) return;
		i16 nContours = i16At(off);
		if (nContours < 0 || u32(nContours) * 2 + 10 > len) return;
		std::vector<u16> endPts(nContours);
		for (i16 i = 0; i < nContours; ++i) endPts[i] = u16At(off + 10 + u32(i) * 2);
		u32 nPts = nContours ? endPts[nContours - 1] + 1 : 0;
		u32 p = off + 10 + u32(nContours) * 2;
		u16 instrLen = u16At(p);
		p += 2 + instrLen;
		std::vector<u8> flags(nPts);
		for (u32 i = 0; i < nPts;) {
			if (p >= m_data.size()) return;
			u8 f = u8(m_data[p++]);
			flags[i++] = f;
			if (f & 0x08) {                               // REPEAT_FLAG: next byte is a repetition count
				if (p >= m_data.size()) return;
				for (u8 rep = u8(m_data[p++]); rep-- && i < nPts;) flags[i++] = f;
			}
		}
		f32 x = 0, y = 0;
		std::vector<Vec2> raw(nPts);
		for (u32 i = 0; i < nPts; ++i) {                  // x deltas
			if (flags[i] & 0x02) { u8 d = u8(m_data[p++]); x += (flags[i] & 0x10) ? f32(d) : -f32(d); }
			else if (!(flags[i] & 0x10)) { x += i16At(p); p += 2; }
			raw[i].x = x;
		}
		for (u32 i = 0; i < nPts; ++i) {                  // y deltas
			if (flags[i] & 0x04) { u8 d = u8(m_data[p++]); y += (flags[i] & 0x20) ? f32(d) : -f32(d); }
			else if (!(flags[i] & 0x20)) { y += i16At(p); p += 2; }
			raw[i].y = y;
		}
		u32 first = 0;
		for (i16 ci = 0; ci < nContours; ++ci) {
			u32 last = endPts[ci];
			u32 n = last - first + 1;
			auto onCurve = [&](u32 k) { return (flags[first + k] & 0x01) != 0; };
			auto pt = [&](u32 k) { return raw[first + k]; };
			u32 start = 0;
			while (start < n && !onCurve(start)) ++start;
			bool allOff = start == n;
			Vec2 sp = allOff ? (pt(n - 1) + pt(0)) * 0.5f : pt(start);
			b.moveTo(xf.apply(sp));
			Vec2 prev = sp;
			u32 k = allOff ? 0 : (start + 1) % n;
			for (u32 step = allOff ? 0 : 1; step < (allOff ? n : n + 1); ++step, k = (k + 1) % n) {
				Vec2 cp = pt(k);
				if (onCurve(k)) { b.lineTo(xf.apply(cp)); prev = cp; }
				else {                                     // off-curve: quadratic through (prev, cp, next)
					u32 nk = (k + 1) % n;
					Vec2 next = onCurve(nk) ? pt(nk) : (cp + pt(nk)) * 0.5f;
					const int STEPS = 6;                   // < 0.05 font-unit error at UI sizes
					for (int s = 1; s <= STEPS; ++s) {
						f32 t = f32(s) / STEPS, u = 1 - t;
						b.lineTo(xf.apply({ u * u * prev.x + 2 * u * t * cp.x + t * t * next.x,
										   u * u * prev.y + 2 * u * t * cp.y + t * t * next.y }));
					}
					prev = next;
				}
			}
			first = last + 1;
		}
	}

	void Font::emitGlyph(u32 g, const Xform& xf, OutlineBuffer& b, int depth) const {
		if (depth > 8) return;                            // malformed fonts can loop; spec uses <= 2 levels
		u32 off, len;
		if (!glyphData(g, off, len) || !len) return;
		if (i16At(off) >= 0) { emitSimple(off, len, xf, b); return; }
		u32 p = off + 10;                                 // composite header done
		for (;;) {
			if (p + 4 > m_data.size()) return;
			u16 flags = u16At(p), glyph = u16At(p + 2);
			p += 4;
			f32 dx = 0, dy = 0;
			if (flags & 0x0001) { dx = i16At(p); dy = i16At(p + 2); p += 4; }
			else { dx = f32(static_cast<signed char>(m_data[p])); dy = f32(static_cast<signed char>(m_data[p + 1])); p += 2; }
			Xform cx;
			if (flags & 0x0008) { f32 s = i16At(p) / 65536.f; cx = { s, 0, 0, s, dx, dy }; p += 2; }
			else if (flags & 0x0040)
				cx = { i16At(p) / 65536.f, i16At(p + 2) / 65536.f, i16At(p + 4) / 65536.f,
					  i16At(p + 6) / 65536.f, dx, dy }, p += 8;
			else cx = { 1, 0, 0, 1, dx, dy };
			emitGlyph(glyph, xf.then(cx), b, depth + 1);
			if (!(flags & 0x0020)) break;                 // MORE_COMPONENTS
		}
	}

	bool Font::outline(u32 g, const OutlineSink& sink) const {
		OutlineBuffer b;
		if (m_variation) {
			traceWeightAtGlyphLoad(*m_variation, g);
			if (FT_Load_Glyph(m_variation->face, g,
				FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM) ||
				m_variation->face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) return false;
			OutlineContext context;
			const FT_Outline_Funcs funcs{ ftMoveTo, ftLineTo, ftConicTo, ftCubicTo, 0, 0 };
			if (FT_Outline_Decompose(&m_variation->face->glyph->outline, &funcs, &context)) return false;
			b = std::move(context.outline);
		}
		else {
		emitGlyph(g, Xform{}, b, 0);
		}
		if (b.starts.empty()) return false;
		for (u32 ci = 0; ci < b.starts.size(); ++ci) {
			u32 s = b.starts[ci];
			u32 e = ci + 1 < b.starts.size() ? b.starts[ci + 1] : u32(b.pts.size());
			if (e > s) {
				sink.moveTo(sink.ctx, b.pts[s]);
				for (u32 j = s + 1; j < e; ++j) sink.lineTo(sink.ctx, b.pts[j]);
			}
		}
		return true;
	}

	Rect Font::bounds(u32 g) const {
		if (m_variation) {
			traceWeightAtGlyphLoad(*m_variation, g);
			if (FT_Load_Glyph(m_variation->face, g,
				FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM) ||
				m_variation->face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) return {};
			FT_BBox box{};
			FT_Outline_Get_CBox(&m_variation->face->glyph->outline, &box);
			return Rect{ static_cast<f32>(box.xMin), static_cast<f32>(box.yMin),
				static_cast<f32>(box.xMax - box.xMin), static_cast<f32>(box.yMax - box.yMin) };
		}
		u32 off, len;
		if (!glyphData(g, off, len) || !len) return {};
		return Rect{ f32(i16At(off + 2)), f32(i16At(off + 4)), f32(i16At(off + 6) - i16At(off + 2)),
					f32(i16At(off + 8) - i16At(off + 4)) };   // xMin, yMin, w, h in font units (y up)
	}

} // namespace xui
