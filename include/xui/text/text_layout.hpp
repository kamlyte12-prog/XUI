#pragma once
/// @file text_layout.hpp  Text measurement + shaping, cached per (text, font, size, width constraint).
///
/// Scope (honest): LTR Latin with kerning and per-cluster advances. Arabic/Indic complex shaping is
/// NOT implemented — those scripts render with isolated forms (documented trade-off). Word wrap uses
/// greedy first-fit with space / zero-width-space / hyphen break points, cached per width bucket.
///
/// All steady-state APIs are allocation-free once caches have warmed. The cache holds at most
/// kMaxCache entries; when full it is cleared wholesale (a miss storm is cheaper than LRU bookkeeping
/// for UI text, where the working set is small and stable).
#include <xui/core/handle.hpp>
#include <xui/core/math.hpp>
#include <xui/text/atlas.hpp>
#include <xui/text/font.hpp>
#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <vector>

namespace xui {

	using FontHandle = Handle<struct FontTag>;

	/// One shaped glyph positioned in run space (px; origin = run origin, y down, baseline at 0).
	struct ShapedGlyph {
		u32 glyph = 0;        ///< font glyph index
		f32 x = 0, y = 0;     ///< pen position when drawn (bitmap offset applied by the painter)
		f32 advance = 0;
	};

	/// A shaped, wrapped text run. `lineStarts[i]` indexes glyphs[] for the start of line i.
	struct TextLayout {
		std::vector<ShapedGlyph> glyphs;
		std::vector<u32> lineStarts;
		std::vector<f32> lineWidths;       ///< pen advance per line, including trailing whitespace
		f32 width = 0, height = 0;         ///< visual bounds (including glyph ink) and height (px)
		f32 advanceWidth = 0;              ///< widest line's pen advance; use for caret positions
		f32 inkLeft = 0;                    ///< visual left edge relative to the pen origin (px; can be negative)
		f32 lineHeight = 0;             ///< line height used
		u32 sizePx = 0;
	};

	/// Font registry + shaping/wrap cache. Owns the fonts and one shared glyph atlas.
	class TextSystem {
	public:
		static constexpr std::size_t kMaxCache = 4096;

		/// Load a font from a TTF/OTF buffer; returns an invalid handle on parse failure.
		FontHandle loadFont(std::string_view ttfData, std::string* error = nullptr);
		const Font* font(FontHandle h) const;
		/// Return a cached variable-font instance at the requested CSS weight, or the original face
		/// when it has no `wght` axis. Static-face fallback remains the caller's responsibility.
		FontHandle fontAtWeight(FontHandle base, f32 weight, std::string* error = nullptr);
		bool initAtlas(Renderer* r, u32 atlasSize = GlyphAtlas::kMinSize);
		GlyphAtlas* atlas() { return &m_atlas; }
		void beginFrame() { m_atlas.beginFrame(); }

		/// Measure + shape. @p wrapWidth <= 0 means a single line (no wrapping). Cached.
		const TextLayout& layout(FontHandle font, std::string_view text, f32 sizePx, f32 wrapWidth = 0);

		/// Cache + atlas + font stats for tests/telemetry.
		std::size_t cacheCount() const { return m_used; }
		u64 rasterCount() const { return m_atlas.rasterCount(); }

	private:
		struct CacheKey {
			u32 font = 0;               ///< FontHandle::index
			u32 hash = 0;               ///< FNV-1a of the text
			u16 sizePx = 0;
			i32 wrap = 0;               ///< wrapWidth bucketed to whole px (INT_MAX = no wrap)
			friend bool operator==(const CacheKey&, const CacheKey&) = default;
		};
		struct CacheEntry { CacheKey key; std::string text; TextLayout layout; };

		static u32 hashText(std::string_view s) {
			u32 h = 2166136261u;
			for (char c : s) { h ^= u8(c); h *= 16777619u; }
			return h ? h : 1;           // keep 0 out of the "empty slot" domain
		}

		const TextLayout& shape(FontHandle fh, std::string_view text, const CacheKey& key);

		struct FontVariant { u32 base = 0; i32 coordinate = 0; FontHandle handle; };
		std::vector<std::unique_ptr<Font>> m_fonts;
		std::vector<FontVariant> m_variants;
		GlyphAtlas m_atlas;
		std::vector<CacheEntry> m_cache;
		std::vector<u32> m_index;       // open addressing over m_cache: entry index + 1, 0 = empty
		u32 m_used = 0;
	};

} // namespace xui
