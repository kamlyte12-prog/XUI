#include <xui/lang/xui.hpp>
#include <xui/style/xss.hpp>
#include <xui/text/text_layout.hpp>
#include <xui/ui/runtime.hpp>
#include <xui/render/painter.hpp>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace xui;

namespace {

	// Alternating Left/Right/Top/Bottom drops around one center panel produce a wide, shallow tree.
	std::string wideSource(u32 panels) {
		std::string source = "Window(size: 1600x900) {\n"
			"  DockHost[id: \"host\", width: 100%, height: 100%, gutter: 7px, headerHeight: 30px] {\n"
			"    DockPanel(title: \"Root\")[id: \"p0\", region: \"Center\", minSize: 120x80] { Text(\"Root\") }\n";
		static const char* zones[4] = { "Left", "Right", "Top", "Bottom" };
		for (u32 i = 1; i < panels; ++i)
			source += "    DockPanel(title: \"Panel\")[id: \"p" + std::to_string(i) + "\", region: \"" +
				zones[i % 4] + "\", target: \"p" + std::to_string(i - 1) + "\", minSize: 60x40] { Text(\"P" + std::to_string(i) + "\") }\n";
		source += "  }\n}\n";
		return source;
	}

	// Every panel docks Left onto the previous one, producing one deep split chain.
	std::string deepSource(u32 panels) {
		std::string source = "Window(size: 1600x900) {\n"
			"  DockHost[id: \"host\", width: 100%, height: 100%, gutter: 7px, headerHeight: 30px] {\n"
			"    DockPanel(title: \"Root\")[id: \"p0\", region: \"Center\", minSize: 40x40] { Text(\"Root\") }\n";
		for (u32 i = 1; i < panels; ++i)
			source += "    DockPanel(title: \"Panel\")[id: \"p" + std::to_string(i) +
				"\", region: \"Left\", target: \"p" + std::to_string(i - 1) + ", minSize: 8x8] { Text(\"P" +
				std::to_string(i) + "\") }\n";
		source += "  }\n}\n";
		return source;
	}

	UiRuntime buildFor(const std::string& source, StringInterner& strings, TextSystem& text) {
		auto document = parseXui(source, "bench-docking-scale.xui", strings);
		auto styles = compileXss("", "bench-docking-scale.xss", strings);
		UiRuntime ui;
		std::string error;
		if (!document || !styles || !ui.build(*document, *styles, strings, text, {}, error)) {
			std::fprintf(stderr, "fixture build failed: %s\n", error.c_str());
			std::exit(1);
		}
		return ui;
	}

	template <typename Fn>
	f64 micros(Fn&& fn, u32 iterations) {
		const auto begin = std::chrono::steady_clock::now();
		for (u32 i = 0; i < iterations; ++i) fn(i);
		const auto end = std::chrono::steady_clock::now();
		return std::chrono::duration<f64, std::micro>(end - begin).count() / iterations;
	}

	void runScenario(const char* name, const std::string& source, u32 panels, u32 iterations) {
		StringInterner strings;
		TextSystem text;
		UiRuntime ui = buildFor(source, strings, text);
		ui.layout(1600.f, 900.f);

		const f64 idle = micros([&](u32) { ui.layout(1600.f, 900.f); }, iterations);

		Painter painter;
		painter.begin(1600, 900, 1.f);
		ui.paint(painter, 1.f);
		painter.finish();
		painter.begin(1600, 900, 1.f);
		const f64 paint = micros([&](u32) {
			painter.begin(1600, 900, 1.f);
			ui.paint(painter, 1.f);
			painter.finish();
		}, iterations / 4);

		std::vector<char> buffer;
		// Drag preview: hold the last panel's tab header and sweep over the root panel's area.
		const auto panelValue = [&](u32 index) {
			const std::string id = "p" + std::to_string(index);
			const std::optional<UiValue> value = ui.value(id);
			return std::get<DockPanelValue>(*value);
		};
		const DockPanelValue root = panelValue(0);
		const DockPanelValue grab = panelValue(panels - 1);
		Vec2 grabPoint{ grab.bounds.x + 20.f, grab.bounds.y + 12.f };
		ui.pointerDown(grabPoint);
		const f64 drag = micros([&](u32 i) {
			const f32 fraction = (i & 1u) ? 0.35f : 0.65f;
			ui.pointerMove({ root.bounds.x + root.bounds.w * fraction, root.bounds.y + 60.f });
		}, iterations);
		ui.keyDown(Key::Escape);

		// Splitter resize: hold the gutter between the root and the panel docked into it.
		const DockPanelValue first = panelValue(1);
		const bool horizontalSplit = first.bounds.w < root.bounds.w - 1.f; // side-by-side children
		Vec2 gutterPoint = horizontalSplit
			? Vec2{ first.bounds.right() + 3.5f, first.bounds.y + 60.f }
			: Vec2{ first.bounds.x + 60.f, first.bounds.bottom() + 3.5f };
		if (!ui.pointerDown(gutterPoint)) { std::fprintf(stderr, "%s: splitter hit test failed\n", name); return; }
		const f64 split = micros([&](u32 i) {
			const f32 delta = (i & 1u) ? 6.f : -6.f;
			ui.pointerMove({ gutterPoint.x + (horizontalSplit ? delta : 0.f),
				gutterPoint.y + (horizontalSplit ? 0.f : delta) });
		}, iterations);
		ui.pointerUp(gutterPoint);

		std::printf("%-14s panels=%-3u idle layout %7.3f us/frame | drag preview %7.3f us/move | splitter %7.3f us/move | paint %8.3f us/frame\n",
			name, panels, idle, drag, split, paint);
	}

} // namespace

int main() {
	constexpr u32 iterations = 20000;
	runScenario("wide", wideSource(8), 8, iterations);
	runScenario("wide", wideSource(16), 16, iterations);
	runScenario("wide", wideSource(32), 32, iterations);
	runScenario("deep", deepSource(8), 8, iterations);
	runScenario("deep", deepSource(16), 16, iterations);
	runScenario("deep", deepSource(24), 24, iterations);
	return 0;
}
