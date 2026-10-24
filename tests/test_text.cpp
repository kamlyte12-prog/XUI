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
#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

TEST(static_font_weight_requests_keep_the_loaded_face_without_synthetic_bold) {
	FontFixture fx;
	if (!fx.have() || fx.font.hasWeightAxis()) return;
	TextSystem text;
	const FontHandle face = text.loadFont(fx.bytes);
	CHECK(face.valid());
	CHECK_EQ(text.fontAtWeight(face, 100), face);
	CHECK_EQ(text.fontAtWeight(face, 900), face);
	const TextLayout regular = text.layout(face, "Hamburgefontsiv To Va", 28.f);
	const TextLayout requestedBold = text.layout(text.fontAtWeight(face, 700), "Hamburgefontsiv To Va", 28.f);
	CHECK_NEAR(regular.advanceWidth, requestedBold.advanceWidth, 0.001f);
	std::cout << "static-font-smoke has-wght-axis=no requested-weight=700 uses-same-face=yes"
		<< " Hamburgefontsiv-To-Va=" << regular.advanceWidth << "px\n";
}

TEST(variable_font_weights_create_distinct_axis_instances_and_atlas_entries) {
	std::filesystem::path path;
	if (const char* env = std::getenv("XUI_VARIABLE_FONT")) path = env;
#ifdef _WIN32
	else if (std::filesystem::exists("C:/Windows/Fonts/bahnschrift.ttf")) path = "C:/Windows/Fonts/bahnschrift.ttf";
#endif
	if (path.empty() || !std::filesystem::exists(path)) {
		std::cout << "SKIP variable font smoke: set XUI_VARIABLE_FONT to a variable TrueType font\n";
		return;
	}
	std::ifstream file(path, std::ios::binary);
	const std::string bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	if (bytes.empty()) { std::cout << "SKIP variable font smoke: could not read font file\n"; return; }
	NullRenderer renderer; TextSystem text; CHECK(text.initAtlas(&renderer));
	std::string error;
	const FontHandle base = text.loadFont(bytes, &error);
	CHECK(base.valid()); if (!base.valid()) return;
	CHECK(text.font(base)->hasWeightAxis()); if (!text.font(base)->hasWeightAxis()) return;
	const auto [axisMin, axisMax] = text.font(base)->weightAxisRange();
	const u32 glyphIndex = text.font(base)->glyphIndex('g');
	const std::array<u16, 6> weights{ 100, 300, 400, 500, 700, 900 };
	std::array<FontHandle, weights.size()> instances{};
	std::array<f32, weights.size()> effectiveWeights{};
	std::array<f32, weights.size()> outlineSums{};
	for (std::size_t i = 0; i < weights.size(); ++i) {
		instances[i] = text.fontAtWeight(base, weights[i], &error);
		CHECK(instances[i].valid()); if (!instances[i].valid()) return;
		const Font* face = text.font(instances[i]);
		const f32 expected = std::clamp(static_cast<f32>(weights[i]), axisMin, axisMax);
		effectiveWeights[i] = expected;
		CHECK_NEAR(face->appliedWeight(), expected, 0.01f);
		std::cout << "variable font requested=" << weights[i] << " wght-coordinate=" << face->appliedWeight() << '\n';
		const TextLayout run = text.layout(instances[i], "The quick brown fox 0123456789", 28.f);
		CHECK_EQ(run.glyphs.size(), std::size_t{ 26 });
		const TextLayout t = text.layout(instances[i], "T", 28.f);
		const TextLayout o = text.layout(instances[i], "o", 28.f);
		const TextLayout to = text.layout(instances[i], "To", 28.f);
		const TextLayout kerningSample = text.layout(instances[i], "To Va", 28.f);
		CHECK_EQ(to.glyphs.size(), 2u);
		CHECK_EQ(kerningSample.glyphs.size(), 4u);
		const f32 toPositioning = to.advanceWidth - t.advanceWidth - o.advanceWidth;
		CHECK(std::isfinite(toPositioning));
		std::cout << "variable font wght=" << face->appliedWeight() << " To GPOS=" << toPositioning
			<< "px To-Va-run=" << kerningSample.advanceWidth << "px\n";
		const AtlasGlyph& atlasGlyph = text.atlas()->glyph(*face, glyphIndex, 28);
		CHECK(atlasGlyph.width > 0 && atlasGlyph.height > 0);
		OutlineBuffer outline;
		CHECK(face->outline(glyphIndex, { &outline, [](void* c, Vec2 p) { static_cast<OutlineBuffer*>(c)->moveTo(p); },
			[](void* c, Vec2 p) { static_cast<OutlineBuffer*>(c)->lineTo(p); } }));
		for (Vec2 p : outline.pts) outlineSums[i] += std::abs(p.x) + std::abs(p.y);
	}
	std::size_t uniqueEffectiveWeights = 0;
	for (std::size_t i = 0; i < instances.size(); ++i) {
		bool seen = false;
		for (std::size_t j = 0; j < i; ++j) {
			if (std::abs(effectiveWeights[i] - effectiveWeights[j]) < 0.01f) seen = true;
			else CHECK(instances[i].index != instances[j].index);
		}
		if (!seen) ++uniqueEffectiveWeights;
	}
	CHECK_EQ(text.atlas()->glyphCount(), static_cast<u32>(uniqueEffectiveWeights));
	std::cout << "font-weight atlas: one glyph at size 28 has " << text.atlas()->glyphCount()
		<< " cached entries for " << uniqueEffectiveWeights << " distinct effective weights\n";
	for (const FontHandle instance : instances)
		(void)text.atlas()->glyph(*text.font(instance), glyphIndex, 28);
	CHECK_EQ(text.atlas()->glyphCount(), static_cast<u32>(uniqueEffectiveWeights));
	CHECK(text.rasterCount() >= uniqueEffectiveWeights); // Weights outside this font's axis range clamp to an existing effective weight.
	CHECK(std::abs(outlineSums.front() - outlineSums.back()) > 0.01f);
	for (std::size_t i = 0; i < weights.size(); ++i)
		for (std::size_t j = i + 1; j < weights.size(); ++j)
			if (std::abs(effectiveWeights[i] - effectiveWeights[j]) < 0.01f)
				CHECK_NEAR(outlineSums[i], outlineSums[j], 0.01f);
}

TEST(variable_font_gpos_kerning_and_advances_follow_each_weight) {
#ifndef XUI_EXAMPLE_DIR
	std::cout << "SKIP Roboto GPOS regression: XUI_EXAMPLE_DIR is not configured\n";
	return;
#else
	const std::filesystem::path path = std::filesystem::path(XUI_EXAMPLE_DIR) / "layout" / "Roboto.ttf";
	std::ifstream file(path, std::ios::binary);
	if (!file) { std::cout << "SKIP Roboto GPOS regression: Roboto.ttf is missing\n"; return; }
	const std::string bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	TextSystem text;
	NullRenderer renderer;
	CHECK(renderer.init({ 64, 64, 1.f, {}, false }));
	CHECK(text.initAtlas(&renderer));
	const FontHandle base = text.loadFont(bytes);
	CHECK(base.valid()); if (!base.valid()) return;
	const Font* baseFont = text.font(base);
	CHECK(baseFont->hasWeightAxis()); if (!baseFont->hasWeightAxis()) return;
	const std::array<f32, 4> weights{ 100.f, 400.f, 700.f, 900.f };
	constexpr std::array<char, 4> representatives{ 'H', 'i', 'm', '.' };
	std::array<f32, 4> toAdjustments{};
	std::array<f32, 4> vaAdjustments{};
	for (std::size_t i = 0; i < weights.size(); ++i) {
		const FontHandle handle = text.fontAtWeight(base, weights[i]);
		CHECK(handle.valid()); if (!handle.valid()) continue;
		const Font* font = text.font(handle);
		CHECK_NEAR(font->appliedWeight(), weights[i], 0.01f);
		for (char character : representatives) {
			const u32 glyph = font->glyphIndex(static_cast<u8>(character));
			const TextLayout single = text.layout(handle, std::string_view(&character, 1), 28.f);
			const f32 ftAdvance = font->advance(glyph);
			const f32 expected = ftAdvance * font->scaleFor(28.f);
			const AtlasGlyph& atlasGlyph = text.atlas()->glyph(*font, glyph, 28);
			CHECK_EQ(single.glyphs.size(), 1u);
			CHECK_NEAR(single.advanceWidth, expected, 0.01f);
			CHECK_NEAR(atlasGlyph.advance, expected, 0.01f);
			std::cout << "Roboto wght=" << weights[i] << " glyph=" << character << " FT_Load_Glyph advance=" << ftAdvance
				<< " shaped=" << single.advanceWidth / font->scaleFor(28.f)
				<< " atlas=" << atlasGlyph.advance / font->scaleFor(28.f) << " font-units\n";
		}

		const f32 t = text.layout(handle, "T", 28.f).advanceWidth;
		const f32 o = text.layout(handle, "o", 28.f).advanceWidth;
		const f32 v = text.layout(handle, "V", 28.f).advanceWidth;
		const f32 a = text.layout(handle, "a", 28.f).advanceWidth;
		const TextLayout to = text.layout(handle, "To", 28.f);
		const TextLayout vaPair = text.layout(handle, "Va", 28.f);
		CHECK_EQ(to.glyphs.size(), 2u);
		CHECK_EQ(vaPair.glyphs.size(), 2u);
		toAdjustments[i] = to.advanceWidth - t - o;
		vaAdjustments[i] = vaPair.advanceWidth - v - a;
		CHECK(toAdjustments[i] < -0.01f);
		CHECK(vaAdjustments[i] < -0.01f);
		const TextLayout sample = text.layout(handle, "Hamburgefontsiv", 28.f);
		const TextLayout pairs = text.layout(handle, "To Va", 28.f);
		CHECK_EQ(sample.glyphs.size(), 15u);
		CHECK_EQ(pairs.glyphs.size(), 4u);
		std::cout << "Roboto wght=" << weights[i] << " To GPOS=" << toAdjustments[i] << "px Va GPOS=" << vaAdjustments[i]
			<< "px Hamburgefontsiv=" << sample.advanceWidth << "px To Va=" << pairs.advanceWidth << "px\n";
	}
	CHECK(text.atlas()->glyphCount() >= weights.size());
	CHECK(std::abs(toAdjustments[1] - toAdjustments[2]) > 0.001f);
	text.atlas()->shutdown();
	renderer.shutdown();
#endif
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

TEST(rasterizer_uses_nonzero_winding_for_overlapping_and_nested_contours) {
	constexpr u32 width = 12, height = 12;
	auto rasterize = [](const std::vector<Vec2>& points, const std::vector<u32>& starts) {
		std::vector<u8> pixels(std::size_t(width) * height);
		std::vector<f32> scratch;
		rasterizeContours(points.data(), static_cast<u32>(points.size()), starts.data(),
			static_cast<u32>(starts.size()), pixels.data(), width, height, scratch);
		return pixels;
	};
	// Two same-direction contours overlap. Non-zero fill treats them as one solid shape;
	// even-odd fill incorrectly punched a transparent stripe through their intersection.
	const std::vector<Vec2> overlapping{
		{1, 1}, {6, 1}, {6, 8}, {1, 8},
		{4, 1}, {9, 1}, {9, 8}, {4, 8}
	};
	const auto overlapBits = rasterize(overlapping, { 0, 4 });
	CHECK_EQ(overlapBits[std::size_t(4) * width + 4], 255);
	CHECK_EQ(overlapBits[std::size_t(4) * width + 7], 255);

	// An oppositely oriented inner contour remains a transparent counter.
	const std::vector<Vec2> counter{
		{1, 1}, {10, 1}, {10, 10}, {1, 10},
		{3, 3}, {3, 8}, {8, 8}, {8, 3}
	};
	const auto counterBits = rasterize(counter, { 0, 4 });
	CHECK_EQ(counterBits[std::size_t(2) * width + 2], 255);
	CHECK_EQ(counterBits[std::size_t(5) * width + 5], 0);
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
	// advances accumulate (with kerning) to the total pen width; visual width also includes ink.
	f32 sum = 0;
	for (const ShapedGlyph& sg : l.glyphs) sum += sg.advance;
	CHECK(std::abs(sum - l.advanceWidth) < 2.f);
	CHECK(l.width >= l.advanceWidth);
	// cache hit: same call returns the same object
	const TextLayout& l2 = ts.layout(fh, "Hello", 16);
	CHECK(&l == &l2);
	CHECK_EQ(ts.cacheCount(), 1u);
	CHECK_EQ(ts.rasterCount(), 0u);      // shaping is independent from glyph rasterisation
	nr.shutdown();
}

TEST(text_frame_stats_count_cache_shaping_and_atlas_work) {
#ifndef XUI_EXAMPLE_DIR
	std::cout << "SKIP text frame counters: XUI_EXAMPLE_DIR is not configured\n";
	return;
#else
	const std::filesystem::path path = std::filesystem::path(XUI_EXAMPLE_DIR) / "layout" / "Roboto.ttf";
	std::ifstream file(path, std::ios::binary);
	if (!file) { std::cout << "SKIP text frame counters: Roboto.ttf is missing\n"; return; }
	const std::string bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	NullRenderer renderer;
	CHECK(renderer.init({ 128, 128, 1.f, {}, false }));
	TextSystem text;
	CHECK(text.initAtlas(&renderer, 256));
	const FontHandle font = text.loadFont(bytes);
	CHECK(font.valid()); if (!font.valid()) return;

	text.beginFrame();
	const TextLayout& first = text.layout(font, "Cache", 16.f);
	const TextLayout& second = text.layout(font, "Cache", 16.f);
	CHECK(&first == &second);
	CHECK_EQ(text.frameStats().layoutRequests, 2u);
	CHECK_EQ(text.frameStats().layoutCacheMisses, 1u);
	CHECK_EQ(text.frameStats().layoutCacheHits, 1u);
	CHECK_EQ(text.frameStats().shapingRuns, 1u);
	CHECK_EQ(text.frameStats().shapedGlyphs, 5u);
	CHECK(text.frameStats().shapingTimeNanoseconds > 0u);

	const Font* loadedFont = text.font(font);
	for (const ShapedGlyph& glyph : first.glyphs)
		(void)text.atlas()->glyph(*loadedFont, glyph.glyph, static_cast<u16>(first.sizePx));
	CHECK(text.atlas()->frameStats().rasterizations > 0u);
	CHECK(text.atlas()->frameStats().uploads > 0u);
	text.beginFrame();
	CHECK_EQ(text.frameStats().layoutRequests, 0u);
	CHECK_EQ(text.atlas()->frameStats().rasterizations, 0u);
#endif
}

TEST(text_layout_cache_rehash_preserves_all_entries) {
#ifndef XUI_EXAMPLE_DIR
	std::cout << "SKIP text cache rehash: XUI_EXAMPLE_DIR is not configured\n";
	return;
#else
	const std::filesystem::path path = std::filesystem::path(XUI_EXAMPLE_DIR) / "layout" / "Roboto.ttf";
	std::ifstream file(path, std::ios::binary);
	if (!file) { std::cout << "SKIP text cache rehash: Roboto.ttf is missing\n"; return; }
	const std::string bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	TextSystem text;
	const FontHandle font = text.loadFont(bytes);
	CHECK(font.valid()); if (!font.valid()) return;
	std::vector<std::string> values;
	values.reserve(256);
	for (u32 i = 0; i < 256; ++i) values.push_back("text cache entry " + std::to_string(i));

	text.beginFrame();
	for (const std::string& value : values) (void)text.layout(font, value, 16.f);
	CHECK_EQ(text.frameStats().layoutCacheMisses, 256u);
	CHECK(text.frameStats().layoutCacheRehashes > 0u);
	text.beginFrame();
	for (const std::string& value : values) (void)text.layout(font, value, 16.f);
	CHECK_EQ(text.frameStats().layoutCacheHits, 256u);
	CHECK_EQ(text.frameStats().layoutCacheMisses, 0u);
#endif
}

TEST(text_layout_width_contains_rasterized_glyph_bounds) {
	FontFixture fx;
	if (!fx.have()) return;
	NullRenderer renderer;
	CHECK(renderer.init({ 64, 64, 1.f, {}, false }));
	TextSystem text;
	CHECK(text.initAtlas(&renderer, 256));
	const FontHandle fontHandle = text.loadFont(fx.bytes);
	CHECK(fontHandle.valid());
	const Font* font = text.font(fontHandle);
	for (std::string_view sample : { "!", "World!", "World! Extension" }) {
		const TextLayout& run = text.layout(fontHandle, sample, 16.6f);
		CHECK_EQ(run.sizePx, 17u); // same rounded pixel size as the glyph atlas
		f32 left = 0.f, right = run.advanceWidth;
		for (const ShapedGlyph& shaped : run.glyphs) {
			const AtlasGlyph& glyph = text.atlas()->glyph(*font, shaped.glyph, static_cast<u16>(run.sizePx));
			left = std::min(left, shaped.x + glyph.offset.x);
			right = std::max(right, shaped.x + glyph.offset.x + glyph.width);
		}
		CHECK(run.inkLeft <= 0.f);
		CHECK(run.width + 0.01f >= right - left);
	}
	text.atlas()->shutdown();
	renderer.shutdown();
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
