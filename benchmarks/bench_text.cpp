// Text pipeline benchmark. It measures cached layout, shaped layout misses, glyph-atlas
// lookup/raster/upload, and draw-list construction without including GPU submission.
#include <xui/render/null_renderer.hpp>
#include <xui/render/painter.hpp>
#include <xui/text/text_layout.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace xui;
using Clock = std::chrono::steady_clock;

namespace {
	struct Result {
		double microseconds = 0;
		TextSystem::FrameStats text;
		GlyphAtlas::FrameStats atlas;
		Painter::Stats painter;
	};

	std::string loadBytes(const std::filesystem::path& path) {
		std::ifstream input(path, std::ios::binary);
		if (!input) return {};
		return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	}

	void drawOne(TextSystem& text, FontHandle fontHandle, std::string_view value, f32 wrapWidth,
		Painter& painter, std::vector<GlyphQuad>& quads) {
		const TextLayout& layout = text.layout(fontHandle, value, 18.f, wrapWidth);
		const Font* font = text.font(fontHandle);
		if (!font) return;
		quads.clear();
		quads.reserve(std::max(quads.capacity(), layout.glyphs.size()));
		for (const ShapedGlyph& shaped : layout.glyphs) {
			const AtlasGlyph& glyph = text.atlas()->glyph(*font, shaped.glyph, static_cast<u16>(layout.sizePx));
			if (glyph.width <= 0.f || glyph.height <= 0.f) continue;
			quads.push_back({ Rect{ shaped.x + glyph.offset.x, shaped.y + glyph.offset.y, glyph.width, glyph.height }, glyph.uv });
		}
		painter.begin(1024, 768, 1.f);
		painter.glyphRun(text.atlas()->texture(), quads.data(), static_cast<u32>(quads.size()), {}, Color{ 1, 1, 1, 1 });
		(void)painter.finish();
	}

	Result measure(const std::filesystem::path& fontPath, std::string_view name,
		const std::vector<std::pair<std::string, f32>>& inputs, bool warmFirst) {
		const std::string fontBytes = loadBytes(fontPath);
		if (fontBytes.empty()) throw std::runtime_error("cannot read benchmark font: " + fontPath.string());
		NullRenderer renderer;
		if (!renderer.init({ 1024, 768, 1.f, {}, false })) throw std::runtime_error("NullRenderer initialization failed");
		TextSystem text;
		if (!text.initAtlas(&renderer)) throw std::runtime_error("glyph atlas initialization failed");
		const FontHandle font = text.loadFont(fontBytes);
		if (!font.valid()) throw std::runtime_error("benchmark font could not be parsed");
		Painter painter;
		std::vector<GlyphQuad> quads;
		quads.reserve(512);
		if (warmFirst && !inputs.empty()) drawOne(text, font, inputs.front().first, inputs.front().second, painter, quads);
		text.beginFrame();
		const auto start = Clock::now();
		for (const auto& [value, width] : inputs) drawOne(text, font, value, width, painter, quads);
		const auto end = Clock::now();
		const double elapsed = std::chrono::duration<double, std::micro>(end - start).count();
		const auto textStats = text.frameStats();
		const auto atlasStats = text.atlas()->frameStats();
		const auto painterStats = painter.stats();
		const double perOperation = inputs.empty() ? 0.0 : elapsed / static_cast<double>(inputs.size());
		std::cout << name << ": " << inputs.size() << " operations, " << elapsed << " us total, "
			<< perOperation << " us/op\n"
			<< "  layout requests=" << textStats.layoutRequests << " hits=" << textStats.layoutCacheHits
			<< " misses=" << textStats.layoutCacheMisses << " shape-runs=" << textStats.shapingRuns
			<< " rehashes=" << textStats.layoutCacheRehashes
			<< " shaped-glyphs=" << textStats.shapedGlyphs << " shaping=" << textStats.shapingTimeNanoseconds << " ns\n"
			<< "  atlas rasterizations=" << atlasStats.rasterizations << " uploads=" << atlasStats.uploads
			<< " rebuilds=" << atlasStats.rebuilds << " painter-batches(last op)=" << painterStats.batches
			<< " draw-calls(last op)=" << painterStats.drawCalls << "\n";
		return { elapsed, textStats, atlasStats, painterStats };
	}
}

int main(int, char** argv) {
	try {
		const std::filesystem::path executable = std::filesystem::absolute(argv[0]);
		const std::filesystem::path fontPath = executable.parent_path() / "Roboto.ttf";
		const std::string stable = "The quick brown fox jumps over the lazy dog.";
		std::vector<std::pair<std::string, f32>> staticText(10000, { stable, 0.f });
		measure(fontPath, "static label (warm layout and atlas)", staticText, true);

		std::vector<std::pair<std::string, f32>> typing;
		typing.reserve(400);
		std::string edited;
		for (u32 i = 0; i < 400; ++i) {
			edited += char('a' + (i % 26));
			if (edited.size() > 80) edited.erase(0, 1);
			typing.emplace_back("Edit " + std::to_string(i) + ": " + edited, 0.f);
		}
		measure(fontPath, "textbox typing (distinct content, 400 edits)", typing, false);

		const std::string paragraph =
			"A retained interface keeps its structure between frames. Text measurement, shaping, wrapping, glyph rasterization, "
			"atlas uploads, and draw-list construction each have different costs. This sample repeats a long wrapped paragraph "
			"to separate cached layout from the work required when content changes. ";
		const std::string wrapped( paragraph.begin(), paragraph.end());
		std::vector<std::pair<std::string, f32>> wrappedOps(3000, { wrapped, 420.f });
		measure(fontPath, "long wrapped paragraph (warm layout and atlas)", wrappedOps, true);

		std::vector<std::pair<std::string, f32>> widths;
		widths.reserve(600);
		for (u32 pass = 0; pass < 2; ++pass)
			for (u32 i = 0; i < 300; ++i)
				widths.emplace_back(paragraph, 300.f + static_cast<f32>(i));
		measure(fontPath, "continuous resize (300 wrap widths, two passes)", widths, false);
		return 0;
	}
	catch (const std::exception& error) {
		std::cerr << "bench_text: " << error.what() << '\n';
		return 1;
	}
}
