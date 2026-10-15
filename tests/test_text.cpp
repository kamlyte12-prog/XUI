// Text layer tests: TTF parsing, shaping, wrapping, atlas, and the cache.
// The font-dependent tests auto-skip when no TrueType font is available (XUI_TEST_FONT env var
// overrides the platform default), keeping the suite deterministic on headless CI.
#include "xtest.hpp"
#include <xui/platform/null_platform.hpp>
#include <xui/render/null_renderer.hpp>
#include <xui/text/atlas.hpp>
#include <xui/text/font.hpp>
#include <xui/text/rasterizer.hpp>
#include <xui/text/text_layout.hpp>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace xui;

namespace {
	/// Finds a loadable TTF or returns an empty path (tests then skip).
	std::string findFont() {
		if (const char* env = std::getenv("XUI_TEST_FONT")) return env;
#ifdef _WIN32
		namespace fs = std::filesystem;
		const char* candidates[] = { "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
									"C:/Windows/Fonts/times.ttf", "C:/Windows/Fonts/cour.ttf" };
		for (const char* c : candidates)
			if (fs::exists(c)) return c;
#endif
		return {};
	}
	struct FontFixture {
		std::string bytes;
		Font font;
		bool ok = false;
		FontFixture() {
			std::string path = findFont();
			if (path.empty()) return;
			FILE* f = std::fopen(path.c_str(), "rb");
			if (!f) return;
			char buf[16 << 10];
			std::size_t n;
			while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) bytes.append(buf, n);
			std::fclose(f);
			ok = font.load(bytes);
		}
		bool have() const { return ok; }
	};
} // namespace

TEST(font_parses_system_ttf) {
	FontFixture fx;
	if (!fx.have()) return;   // skip: no font on this machine
	const FontMetrics& m = fx.font.metrics();
	CHECK(m.unitsPerEm >= 16 && m.unitsPerEm <= 16384);
	CHECK(m.glyphCount > 0);
	CHECK(m.ascender > 0 && m.descender < 0);
	CHECK(fx.font.glyphIndex('A') != 0);
	CHECK(fx.font.glyphIndex(0x1F600) == 0 || fx.font.glyphIndex(0x1F600) != 0);   // must not crash
	// line height for 16px is sane
	f32 lh = fx.font.lineHeight(16);
	CHECK(lh > 10 && lh < 40);
}

TEST(font_outline_flattens_segments) {
	FontFixture fx;
	if (!fx.have()) return;
	OutlineBuffer buf;
	CHECK(fx.font.outline(fx.font.glyphIndex('A'), { &buf, [](void* c, Vec2 p) { static_cast<OutlineBuffer*>(c)->moveTo(p); },
														[](void* c, Vec2 p) { static_cast<OutlineBuffer*>(c)->lineTo(p); } }));
	CHECK(buf.starts.size() >= 1);      // 'A' has at least one contour
	CHECK(buf.pts.size() >= 3);         // and at least a triangle's worth of points
	// capital letters sit on/above the baseline (y up: all points >= -small epsilon)
	f32 minY = 1e9f;
	for (Vec2 p : buf.pts) minY = std::min(minY, p.y);
	CHECK(minY > -f32(fx.font.metrics().unitsPerEm) / 4);
}

TEST(rasterizer_produces_coverage) {
	FontFixture fx;
	if (!fx.have()) return;
	u32 g = fx.font.glyphIndex('I');
	OutlineBuffer buf;
	fx.font.outline(g, { &buf, [](void* c, Vec2 p) { static_cast<OutlineBuffer*>(c)->moveTo(p); },
							 [](void* c, Vec2 p) { static_cast<OutlineBuffer*>(c)->lineTo(p); } });
	// scale to 32px and rasterise
	f32 s = fx.font.scaleFor(32);
	Rect b = fx.font.bounds(g);
	u32 w = u32(b.w * s) + 4, h = u32(b.h * s) + 4;
	std::vector<Vec2> pts;
	for (Vec2 p : buf.pts) pts.push_back({ 2 + (p.x - b.x) * s, 2 + (b.bottom() - p.y) * s });
	std::vector<u8> bits(std::size_t(w) * h);
	std::vector<f32> scratch;
	rasterizeContours(pts.data(), u32(pts.size()), buf.starts.data(), u32(buf.starts.size()), bits.data(), w, h, scratch);
	u32 covered = 0;
	for (u8 v : bits) covered += v > 127;
	CHECK(covered > 10);                 // a capital 'I' covers a visible fraction
	CHECK(covered < w * h / 2);          // ... but nowhere near everything
	// determinism: rasterise again, bytes identical
	std::vector<u8> bits2(std::size_t(w) * h);
	rasterizeContours(pts.data(), u32(pts.size()), buf.starts.data(), u32(buf.starts.size()), bits2.data(), w, h, scratch);
	CHECK(bits == bits2);
}

TEST(atlas_rasterises_and_caches_through_null_renderer) {
	FontFixture fx;
	if (!fx.have()) return;
	NullRenderer nr;
	CHECK(nr.init({ 512, 512, 1, {}, 0 }));
	GlyphAtlas atlas;
	CHECK(atlas.init(&nr, 256));
	u32 gA = fx.font.glyphIndex('A');
	const AtlasGlyph& a1 = atlas.glyph(fx.font, gA, 16);
	CHECK(a1.width > 0 && a1.height > 0);
	CHECK(a1.advance > 0);
	u64 rasters = atlas.rasterCount();
	const AtlasGlyph& a2 = atlas.glyph(fx.font, gA, 16);   // cached
	CHECK(&a1 == &a2);
	CHECK_EQ(atlas.rasterCount(), rasters);
	const AtlasGlyph& a3 = atlas.glyph(fx.font, gA, 24);   // different size -> new entry
	CHECK(a3.width >= a1.width);
	CHECK(atlas.rasterCount() == rasters + 1);
	// whitespace glyph: 1x1 placeholder with advance
	u32 gSpace = fx.font.glyphIndex(' ');
	const AtlasGlyph& sp = atlas.glyph(fx.font, gSpace, 16);
	CHECK(sp.advance > 0);
	CHECK(sp.width <= 1 && sp.height <= 1);
	nr.shutdown();
}

TEST(atlas_index_growth_keeps_large_glyph_sets_probeable) {
	FontFixture fx;
	if (!fx.have()) return;
	NullRenderer nr;
	CHECK(nr.init({ 512, 512, 1, {}, 0 }));
	GlyphAtlas atlas;
	CHECK(atlas.init(&nr, 256));
	// Distinct glyphs force the open-addressed index past its first table growth without
	// exhausting the texture atlas itself.
	for (u32 glyph = 1; glyph <= 180; ++glyph) {
		const AtlasGlyph& entry = atlas.glyph(fx.font, glyph, 12);
		CHECK(entry.width > 0 && entry.height > 0);
	}
	CHECK_EQ(atlas.glyphCount(), 180u);
	const u64 rasters = atlas.rasterCount();
	CHECK(atlas.glyph(fx.font, 145, 12).advance > 0);
	CHECK_EQ(atlas.rasterCount(), rasters);
	// A single large glyph forces an atlas resize while an earlier draw list may still refer
	// to the original texture.
	(void)atlas.glyph(fx.font, fx.font.glyphIndex('W'), 512);
	CHECK(nr.liveTextures() > 1); // old atlas texture remains valid through this frame's submit
	atlas.beginFrame();
	CHECK_EQ(nr.liveTextures(), 1u);
	atlas.shutdown();
	nr.shutdown();
}

TEST(text_shaping_matches_pen_advance_sum) {
	FontFixture fx;
	if (!fx.have()) return;
	NullRenderer nr;
	nr.init({ 64, 64, 1, {}, 0 });
	TextSystem ts;
	ts.initAtlas(&nr, 256);
	FontHandle fh = ts.loadFont(fx.bytes);
	CHECK(fh.valid());
	const TextLayout& l = ts.layout(fh, "Hello", 16);
	CHECK(l.glyphs.size() == 5);
	CHECK_EQ(l.lineStarts.size(), 1u);
	CHECK(l.width > 10 && l.width < 200);
	// advances accumulate (with kerning) to the total width
	f32 sum = 0;
	for (const ShapedGlyph& sg : l.glyphs) sum += sg.advance;
	CHECK(std::abs(sum - l.width) < 2.f);
	// cache hit: same call returns the same object
	const TextLayout& l2 = ts.layout(fh, "Hello", 16);
	CHECK(&l == &l2);
	CHECK_EQ(ts.cacheCount(), 1u);
	CHECK_EQ(ts.rasterCount(), 0u);      // shaping is independent from glyph rasterisation
	nr.shutdown();
}

TEST(text_wrap_produces_expected_lines) {
	FontFixture fx;
	if (!fx.have()) return;
	NullRenderer nr;
	nr.init({ 64, 64, 1, {}, 0 });
	TextSystem ts;
	ts.initAtlas(&nr, 256);
	FontHandle fh = ts.loadFont(fx.bytes);
	std::string longText = "the quick brown fox jumps over the lazy dog";
	f32 oneLine = ts.layout(fh, longText, 14).width;
	// wrap at ~60% of the natural width forces at least 2 lines
	const TextLayout& wrapped = ts.layout(fh, longText, 14, oneLine * 0.6f);
	CHECK(wrapped.lineStarts.size() >= 2);
	CHECK(wrapped.glyphs[wrapped.lineStarts[1]].y >= wrapped.lineHeight);
	CHECK(wrapped.width <= oneLine * 0.6f + 30.f);   // no line wildly exceeds the wrap (tolerance for long words)
	// every glyph of the wrapped layout exists exactly once (rewind logic must not duplicate)
	CHECK_EQ(wrapped.glyphs.size(), ts.layout(fh, longText, 14).glyphs.size());
	// explicit newlines are respected
	const TextLayout& nl = ts.layout(fh, "a\nb", 14);
	CHECK(nl.lineStarts.size() == 2);
	CHECK_EQ(nl.lineWidths.size(), 2u);
	CHECK(nl.lineWidths[0] > 0.f && nl.lineWidths[1] > 0.f);
	const TextLayout& trailingNewline = ts.layout(fh, "a\n", 14);
	CHECK_EQ(trailingNewline.lineWidths.size(), 2u);
	CHECK_NEAR(trailingNewline.lineWidths[1], 0.f, 0.01f);
	CHECK_NEAR(ts.layout(fh, "a\tb", 14).width, ts.layout(fh, "a    b", 14).width, 0.1f);
	nr.shutdown();
}

TEST(platform_null_roundtrip) {
	NullPlatform p;
	PlatformDesc d; d.appName = "test";
	CHECK(p.init(d));
	WindowDesc wd; wd.title = "w"; wd.width = 640; wd.height = 480;
	WindowHandle w = p.createWindow(wd);
	CHECK(w.valid());
	Vec2 sz = p.windowSizeLogical(w);
	CHECK_EQ(sz.x, 640.f); CHECK_EQ(sz.y, 480.f);
	Vec2i px = p.windowSizePixels(w);
	CHECK_EQ(px.x, 640); CHECK_EQ(px.y, 480);
	Event e;
	CHECK(!p.pumpEvents(e));                       // empty
	p.click(MouseButton::Left, 10, 20);
	CHECK(p.pumpEvents(e) && e.type == EventType::MouseDown && e.x == 10 && e.y == 20);
	CHECK(p.pumpEvents(e) && e.type == EventType::MouseUp);
	CHECK(!p.pumpEvents(e));
	p.requestRedraw(w);
	CHECK(p.pumpEvents(e) && e.type == EventType::Tick);   // coalesced tick
	p.requestRedraw(w);
	p.requestRedraw(w);                            // coalesced: still one
	CHECK(p.pumpEvents(e) && e.type == EventType::Tick);
	CHECK(!p.pumpEvents(e));
	p.typeText("hi");                              // UTF-8 -> codepoints
	CHECK(p.pumpEvents(e) && e.type == EventType::TextInput && e.codepoint == 'h');
	CHECK(p.pumpEvents(e) && e.type == EventType::TextInput && e.codepoint == 'i');
	p.setClipboardText("xui");
	CHECK(p.clipboardText() == "xui");
	CHECK(p.clipboardWriteCount() == 1);
	p.keyTap(Key::A, u8(Mod::Ctrl));
	CHECK(p.pumpEvents(e) && e.type == EventType::KeyDown && e.key == Key::A && e.mods == u8(Mod::Ctrl));
	CHECK(p.pumpEvents(e) && e.type == EventType::KeyUp && e.key == Key::A && e.mods == u8(Mod::Ctrl));
	p.simulateResize(w, 800, 600, 2.f);
	CHECK(p.pumpEvents(e) && e.type == EventType::Resize && e.w == 800 && e.h == 600);
	CHECK(p.pumpEvents(e) && e.type == EventType::DpiChange && e.scale == 2.f);
	CHECK_EQ(p.dpiScale(w), 2.f);
	CHECK_EQ(p.windowSizePixels(w).x, 1600);       // 800 * 2
	p.destroyWindow(w);
	CHECK(!p.dpiScale(w));                         // stale handle -> 0
	p.shutdown();
}
