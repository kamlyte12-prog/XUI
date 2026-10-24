#pragma once
/// @file font.hpp  Compact sfnt parser for cmap, metrics, names, legacy kern, and static `glyf` outlines.
/// HarfBuzz shapes OpenType runs; FreeType evaluates variable `glyf` outlines and metrics at a selected
/// axis instance. CFF/CFF2 outlines are outside the current font pipeline.
#include <xui/core/math.hpp>
#include <xui/core/types.hpp>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <memory>
#include <utility>
#include <vector>

namespace xui {

	/// Receives the flattened outline of one glyph: quadratic control points are delivered as on-curve
	/// midpoints so the callback only sees line segments. Callers rasterize from this.
	struct OutlineSink {
		void* ctx = nullptr;
		void (*moveTo)(void* ctx, Vec2 p) = nullptr;
		void (*lineTo)(void* ctx, Vec2 p) = nullptr;
	};

	/// 2x3 affine [a c e; b d f] used when descending into composite glyph components.
	struct Xform {
		f32 a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
		Vec2 apply(Vec2 p) const { return { a * p.x + c * p.y + e, b * p.x + d * p.y + f }; }
		Xform then(const Xform& o) const {
			return { o.a * a + o.b * c, o.c * a + o.d * c, o.a * b + o.b * d, o.c * b + o.d * d,
					o.a * e + o.b * f + o.e, o.c * e + o.d * f + o.f };
		}
	};

	/// Flat outline storage: points of all contours plus each contour's start index.
	struct OutlineBuffer {
		std::vector<Vec2> pts;
		std::vector<u32> starts;
		void moveTo(Vec2 p) { starts.push_back(u32(pts.size())); pts.push_back(p); }
		void lineTo(Vec2 p) { pts.push_back(p); }
	};

	/// Metrics in font units (divide by unitsPerEm). All sizes are em-relative floats after scaling.
	struct FontMetrics {
		f32 unitsPerEm = 1000;
		f32 ascender = 800, descender = -200, lineGap = 0;   ///< hhea values
		f32 typoAscender = 0, typoDescender = 0;             ///< OS/2 sTypo* (0 = table absent)
		i16 underlinePosition = 0; i16 underlineThickness = 1;
		u32 glyphCount = 0;
		bool hasKernTable = false;
		std::string family;                                   ///< name table, name id 1
	};

	/// A scaled glyph's layout data: everything needed to draw and advance it, in pixels.
	struct GlyphMetrics {
		u32 glyphIndex = 0;
		f32 advance = 0;                       ///< pen advance
		Rect bounds;                           ///< ink box, relative to the glyph origin (y down; above baseline = negative y)
		f32 scale = 1;                         ///< font-unit -> px factor used
	};

	class Font {
	public:
		Font();
		~Font();
		Font(const Font&) = delete;
		Font& operator=(const Font&) = delete;
		Font(Font&&) = delete;
		Font& operator=(Font&&) = delete;
		/// Parse a TTF/OTF buffer (copied internally; the caller may free it after load).
		bool load(std::string_view data, std::string* error = nullptr);
		bool loaded() const { return !m_data.empty(); }

		const FontMetrics& metrics() const { return m_metrics; }
		/// Em scale factor for a requested pixel size (px = fontUnit * scale(size)).
		f32 scaleFor(f32 px) const { return px / m_metrics.unitsPerEm; }
		/// Line height in px for a given size (ascender - descender + lineGap).
		f32 lineHeight(f32 px) const { return (m_metrics.ascender - m_metrics.descender + m_metrics.lineGap) * scaleFor(px); }
		f32 baselineAt(f32 px) const { return m_metrics.ascender * scaleFor(px); }

		/// 0 = not mapped (.notdef). Supports BMP (cmap 4) and supplementary (cmap 12) codepoints.
		u32 glyphIndex(u32 codepoint) const;
		f32 advance(u32 glyphIndex) const;                       ///< font units
		bool kerning(u32 left, u32 right, i16& outX) const;      ///< font units, kern format 0

		/// Flatten a glyph's contours into @p sink (font units, y up). Returns false for missing/empty glyphs.
		bool outline(u32 glyphIndex, const OutlineSink& sink) const;

		/// Bounding box of a glyph's ink in font units (empty if no outline).
		Rect bounds(u32 glyphIndex) const;

		const std::string& family() const { return m_metrics.family; }
		bool hasWeightAxis() const;
		std::pair<f32, f32> weightAxisRange() const;
		/// Default coordinate of the `wght` axis in CSS weight units.
		f32 weightAxisDefault() const { return m_weightAxisDefault; }
		/// Axis coordinate last accepted by FreeType, in user-facing CSS weight units.
		f32 appliedWeight() const { return m_appliedWeight; }
		u32 cacheId() const { return m_cacheId; }

	private:
		friend class TextSystem;
		struct ShapingState;
		bool setWeightAxis(f32 weight, std::string* error = nullptr);
		struct ShapedGlyph {
			u32 glyph = 0;
			f32 xAdvance = 0, xOffset = 0, yOffset = 0; // font units; y offset is positive upward
			u32 cluster = 0;                              // UTF-8 byte index in the shaped input
		};
		bool shapeText(std::string_view utf8, std::vector<ShapedGlyph>& glyphs) const;
		u32 tableOffset(std::string_view tag) const;
		u16 u16At(u32 off) const { u16 v; std::memcpy(&v, m_data.data() + off, 2); return (u16)(v << 8 | v >> 8); }
		i16 i16At(u32 off) const { return (i16)u16At(off); }
		u32 u32At(u32 off) const {
			u32 v; std::memcpy(&v, m_data.data() + off, 4);
			return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | (v >> 8 & 0xFF00) | (v >> 24);
		}
		bool parseCmap(u32 off);
		bool parseKern(u32 off);
		void parseName(u32 off);
		bool glyphData(u32 glyphIndex, u32& off, u32& len) const;   // into m_data
		void emitSimple(u32 off, u32 len, const struct Xform& xf, struct OutlineBuffer& b) const;
		void emitGlyph(u32 glyphIndex, const struct Xform& xf, struct OutlineBuffer& b, int depth) const;

		std::string m_data;                     // the whole font file, big-endian
		FontMetrics m_metrics;
		u32 m_cmap4 = 0, m_cmap12 = 0;          // subtable offsets (0 = absent)
		u32 m_head = 0, m_hhea = 0, m_hmtx = 0, m_maxp = 0, m_loca = 0, m_glyf = 0, m_kern = 0;
		u32 m_numGlyphs = 0, m_numHMetrics = 0;
		bool m_longLoca = false, m_cff = false;
		std::vector<std::pair<u64, u32>> m_kernPairs;   // key = (left<<32)|right, sorted; kern format 0
		struct VariationState;
		std::unique_ptr<VariationState> m_variation;
		std::unique_ptr<ShapingState> m_shaping;
		f32 m_weightAxisDefault = 400.f;
		f32 m_appliedWeight = 400.f;
		u32 m_cacheId = 0;
	};

} // namespace xui
