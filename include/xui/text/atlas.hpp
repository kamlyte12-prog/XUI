#pragma once
/// @file atlas.hpp  Glyph atlas: a growable A8 texture with shelf packing, LRU eviction, and
/// zero-per-frame allocation. One atlas per (font raster mode); fonts of any family share it.
///
/// Packing: skyline shelves. A glyph that does not fit triggers eviction of the least-recently-used
/// glyphs (cheap: each entry's lastFrame stamp is compared against a ring cursor) and, only if the
/// atlas is genuinely full, a rebuild at the next power-of-two size up to kMaxSize.
///
/// The atlas owns one TextureHandle on the Renderer; updates are sub-regions uploaded once per new
/// glyph — no per-frame uploads in steady state.
#include <xui/core/handle.hpp>
#include <xui/core/types.hpp>
#include <xui/render/drawlist.hpp>
#include <xui/render/renderer.hpp>
#include <xui/text/font.hpp>
#include <vector>

namespace xui {

	struct GlyphKey {
		u32 font = 0;        ///< font instance identity, including variable-axis instances
		u32 glyph = 0;        ///< font glyph index
		u16 sizePx = 0;       ///< pixel size (rounded)
		u16 pad = 0;
		friend constexpr bool operator==(GlyphKey, GlyphKey) = default;
	};
	struct GlyphKeyHash {
		std::size_t operator()(GlyphKey k) const noexcept {
			return std::size_t(k.font) * 0xD6E8FEB86659FD93ull ^
				std::size_t(k.glyph) * 0x9E3779B97F4A7C15ull ^ (std::size_t(k.sizePx) << 32);
		}
	};

	struct AtlasGlyph {
		Rect uv;              ///< normalised texture coords of the coverage bitmap
		f32 advance = 0;      ///< px
		Vec2 offset;          ///< bitmap top-left relative to the pen (px; y down, negative = above baseline)
		f32 width = 0, height = 0;
		u32 lastFrame = 0;    ///< LRU stamp
	};

	/// Rasterises glyphs from a Font into an A8 texture. Coverage comes from the analytic scanline
	/// fill in rasterizer.hpp — no third-party code, deterministic across platforms.
	class GlyphAtlas {
	public:
		static constexpr u32 kMinSize = 256, kMaxSize = 4096;
		struct FrameStats {
			u32 rasterizations = 0;
			u32 uploads = 0;
			u32 rebuilds = 0;
		};

		GlyphAtlas() = default;
		GlyphAtlas(const GlyphAtlas&) = delete;
		GlyphAtlas& operator=(const GlyphAtlas&) = delete;
		~GlyphAtlas() { shutdown(); }

		/// Creates the backing texture. @p initialSize is a hint; the atlas grows as needed.
		bool init(Renderer* r, u32 initialSize = kMinSize);
		void shutdown();
		bool ready() const { return m_renderer != nullptr; }

		/// Returns the glyph, rasterising + uploading it on first use. Never fails for a valid font:
		/// unmapped glyphs fall back to .notdef; a full atlas evicts and, if still full, grows.
		const AtlasGlyph& glyph(const Font& font, u32 glyphIndex, u16 sizePx);

		TextureHandle texture() const { return m_tex; }
		u32 size() const { return m_size; }
		u32 glyphCount() const { return u32(m_entries.size()); }
		u64 rasterCount() const { return m_rasters; }   ///< test/telemetry: how many actual rasterisations
		void beginFrame();
		const FrameStats& frameStats() const { return m_frameStats; }

	private:
		static constexpr u32 kNone = 0xFFFFFFFFu;
		struct Shelf { u32 y = 0, height = 0, x = 0; };   // skyline shelf with an allocation cursor
		struct Entry { GlyphKey key; AtlasGlyph g; const Font* font = nullptr; };

		const AtlasGlyph& rasterise(const Font& font, GlyphKey key);
		u32 findSlot(const GlyphKey& key) const;
		void reindex();
		void indexEntry(u32 entryIndex);
		bool place(u32 w, u32 h, u32& outX, u32& outY);
		void rebuild(u32 newSize);
		void evictAll();

		Renderer* m_renderer = nullptr;
		TextureHandle m_tex;
		u32 m_size = 0;
		std::vector<Shelf> m_shelves;                  // skyline shelves, bottom-up
		std::vector<Entry> m_entries;
		std::vector<u32> m_index;                      // open addressing: entry slot + 1, 0 = empty
		std::vector<TextureHandle> m_retiredTextures;  // kept alive until prior-frame draw commands are gone
		u32 m_used = 0;
		u64 m_frame = 1, m_rasters = 0;
		FrameStats m_frameStats;
		std::vector<u8> m_bitmap;                      // rasterisation scratch
	};
} // namespace xui
