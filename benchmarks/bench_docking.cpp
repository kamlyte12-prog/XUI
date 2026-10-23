#include <xui/lang/xui.hpp>
#include <xui/style/xss.hpp>
#include <xui/text/text_layout.hpp>
#include <xui/ui/runtime.hpp>
#include <chrono>
#include <cstdio>
#include <string>

using namespace xui;

int main() {
	constexpr std::string_view source = R"(
Window(size: 1200x800) {
  DockHost[id: "host", width: 100%, height: 100%, gutter: 7px, headerHeight: 30px] {
    DockPanel(title: "Root")[id: "root", region: "Center", minSize: 160x120] { Text("Root") }
    DockPanel(title: "Left")[id: "left", region: "Left", target: "root", minSize: 120x120] { Text("Left") }
    DockPanel(title: "Right")[id: "right", region: "Right", target: "root", minSize: 120x120] { Text("Right") }
    DockPanel(title: "Tab")[id: "tab", region: "Center", target: "root", minSize: 120x120] { Text("Tab") }
  }
}
)";
	StringInterner strings;
	auto document = parseXui(source, "bench-docking.xui", strings);
	// Keep the pointer fixture's host origin at (0, 0). The built-in Fluent baseline
	// intentionally pads DockHost, so this benchmark overrides that visual default to
	// keep its hand-calculated splitter probe deterministic.
	auto styles = compileXss("DockHost { resting: { pad: 0px } }", "bench-docking.xss", strings);
	if (!document || !styles) { std::fprintf(stderr, "fixture parse failed\n"); return 1; }
	TextSystem text;
	UiRuntime ui;
	std::string error;
	if (!ui.build(*document, *styles, strings, text, {}, error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
	ui.layout(1200.f, 800.f);
	constexpr u32 iterations = 200000;

	const Rect left = ui.bounds({ 4, 1 });
	const Rect root = ui.bounds({ 2, 1 });
	const Vec2 dragStart{ left.x + 24.f, left.y + 14.f };
	ui.pointerDown(dragStart);
	const auto dragBegin = std::chrono::steady_clock::now();
	for (u32 i = 0; i < iterations; ++i) {
		const f32 fraction = (i & 1u) ? 0.04f : 0.5f;
		ui.pointerMove({ root.x + root.w * fraction, root.y + 80.f });
	}
	const auto dragEnd = std::chrono::steady_clock::now();
	ui.keyDown(Key::Escape);
	const f64 dragNs = std::chrono::duration<f64, std::nano>(dragEnd - dragBegin).count() / iterations;

	const Rect resizedLeft = ui.bounds({ 4, 1 });
	const Vec2 gutter{ resizedLeft.right() + 3.5f, resizedLeft.y + 100.f };
	if (!ui.pointerDown(gutter)) {
		std::fprintf(stderr, "splitter hit test failed: left=(%.1f, %.1f, %.1f, %.1f), root=(%.1f, %.1f, %.1f, %.1f), probe=(%.1f, %.1f)\n",
			resizedLeft.x, resizedLeft.y, resizedLeft.w, resizedLeft.h, root.x, root.y, root.w, root.h, gutter.x, gutter.y);
		return 2;
	}
	const auto splitBegin = std::chrono::steady_clock::now();
	for (u32 i = 0; i < iterations; ++i) {
		const f32 delta = (i & 1u) ? 8.f : -8.f;
		ui.pointerMove({ gutter.x + delta, gutter.y });
	}
	const auto splitEnd = std::chrono::steady_clock::now();
	ui.pointerUp(gutter);
	const f64 splitNs = std::chrono::duration<f64, std::nano>(splitEnd - splitBegin).count() / iterations;

	std::printf("Dock pointer path (%u iterations): drag preview %.2f us/move, splitter resize %.2f us/move\n",
		iterations, dragNs / 1000.0, splitNs / 1000.0);
	return 0;
}
