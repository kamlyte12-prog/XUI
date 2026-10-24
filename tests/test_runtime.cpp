#include "xtest.hpp"
#include <xui/lang/xui.hpp>
#include <xui/platform/null_platform.hpp>
#include <xui/render/null_renderer.hpp>
#include <xui/render/soft_renderer.hpp>
#include <xui/style/xss.hpp>
#include <xui/text/text_layout.hpp>
#include <xui/ui/runtime.hpp>
#include <xui/xui.hpp>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

using namespace xui;

namespace xui {
	struct UiRuntimeFontWeightProbe {
		static f32 appliedWeight(const UiRuntime& runtime, std::string_view id) {
			for (const UiRuntime::Node& node : runtime.m_nodes)
				if (node.id.v && runtime.m_strings->view(node.id) == id) {
					const Font* font = runtime.m_text ? runtime.m_text->font(node.font) : nullptr;
					return font ? font->appliedWeight() : -1.f;
				}
			return -1.f;
		}
		static Rect bounds(const UiRuntime& runtime, std::string_view id) {
			for (const UiRuntime::Node& node : runtime.m_nodes)
				if (node.id.v && runtime.m_strings->view(node.id) == id) return runtime.m_layout.absRect(node.layout);
			return {};
		}
	};
}

namespace {
	void recordPress(void* context, std::string_view name) {
		if (name == "onSubmit") ++*static_cast<u32*>(context);
		if (name == "onChange") ++static_cast<u32*>(context)[1];
	}
	void recordDock(void* context, std::string_view) { ++*static_cast<u32*>(context); }
	struct CheckListProbe { UiRuntime* runtime = nullptr; u32 calls = 0; CheckListValue selected; };
	void recordCheckList(void* context, std::string_view name) {
		auto& probe = *static_cast<CheckListProbe*>(context);
		if (name != "onListChange") return;
		++probe.calls;
		if (const auto value = probe.runtime->value("tasks"))
			if (const auto* selected = std::get_if<CheckListValue>(&*value)) probe.selected = *selected;
	}

	std::string readTestFont() {
		std::vector<std::filesystem::path> candidates;
		if (const char* configured = std::getenv("XUI_TEST_FONT")) candidates.emplace_back(configured);
#ifdef _WIN32
		candidates.emplace_back("C:/Windows/Fonts/segoeui.ttf");
		candidates.emplace_back("C:/Windows/Fonts/arial.ttf");
#endif
		for (const auto& path : candidates) {
			std::ifstream file(path, std::ios::binary);
			if (file) return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
		}
		return {};
	}
	std::string readVariableTestFont() {
		std::vector<std::filesystem::path> candidates;
		if (const char* configured = std::getenv("XUI_VARIABLE_FONT")) candidates.emplace_back(configured);
#ifdef _WIN32
		candidates.emplace_back("C:/Windows/Fonts/bahnschrift.ttf");
#endif
		for (const auto& path : candidates) {
			if (!std::filesystem::exists(path)) continue;
			std::ifstream file(path, std::ios::binary);
			return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
		}
		return {};
	}

	TEST(auto_sized_empty_widgets_keep_intrinsic_space_for_content) {
		constexpr std::string_view xuiSource = R"(
Window(size: 520x640) {
  Column(gap: 4) {
    TextBox[placeholder: "Name"]
    TextArea[placeholder: "Notes"]
    Slider[value: 0.5]
    ProgressBar[value: 0.5]
    ColorPicker[value: #336699]
    Dropdown[options: "One|Two", value: "One"]
    Row { TextBox[id: "growing-field", placeholder: "Short"] }
    CheckBox(label: "Enable feature")
    Switch[label: "Synchronize changes"]
    Button(label: "")
  }
}

)";
		StringInterner strings;
		auto document = parseXui(xuiSource, "intrinsic-controls.xui", strings);
		CHECK(document.has_value()); if (!document) return;
		auto styles = compileXss("", "intrinsic-controls.xss", strings);
		CHECK(styles.has_value()); if (!styles) return;
		SoftRenderer renderer;
		CHECK(renderer.init({ 520, 640, 1.f, nullptr, false }));
		TextSystem text; CHECK(text.initAtlas(&renderer)); UiRuntime ui; std::string error;
		const std::string fontBytes = readTestFont();
		const FontHandle font = fontBytes.empty() ? FontHandle{} : text.loadFont(fontBytes, &error);
		CHECK(ui.build(*document, *styles, strings, text, font, error));
		if (!error.empty()) return;
		ui.layout(520.f, 640.f);
		const Rect name = ui.bounds({ 2, 1 });
		const Rect notes = ui.bounds({ 3, 1 });
		const Rect slider = ui.bounds({ 4, 1 });
		const Rect progress = ui.bounds({ 5, 1 });
		const Rect picker = ui.bounds({ 6, 1 });
		const Rect dropdown = ui.bounds({ 7, 1 });
		const Rect column = ui.bounds({ 1, 1 });
		const Rect growingBefore = ui.bounds({ 9, 1 });
		const Rect checkbox = ui.bounds({ 10, 1 });
		const Rect switchControl = ui.bounds({ 11, 1 });
		const Rect emptyButton = ui.bounds({ 12, 1 });
		CHECK(name.w >= 160.f); CHECK(name.h >= 28.f);
		CHECK(notes.w >= 240.f); CHECK(notes.h >= 96.f);
		CHECK(slider.w >= 160.f); CHECK(slider.h >= 24.f);
		CHECK(progress.w >= 160.f); CHECK(progress.h >= 16.f);
		CHECK(picker.w >= 156.f); CHECK(picker.h >= 34.f);
		CHECK(dropdown.w >= 156.f); CHECK(dropdown.h >= 34.f);
		CHECK(checkbox.w >= 40.f); CHECK(checkbox.h >= 20.f);
		CHECK(switchControl.w >= 46.f); CHECK(switchControl.h >= 24.f);
		CHECK(emptyButton.w >= 96.f); CHECK(emptyButton.h >= 36.f);
		CHECK(dropdown.bottom() <= column.bottom() + 0.01f);
		CHECK(notes.y >= name.bottom());
		CHECK(slider.y >= notes.bottom());
		if (font.valid()) {
			CHECK(ui.setValue("growing-field", std::string("A much longer field value that must grow intrinsically")));
			ui.layout(520.f, 640.f);
			CHECK(ui.bounds({ 9, 1 }).w > growingBefore.w);
		}
	}

	TEST(ui_runtime_applies_xss_weight_to_multiple_variable_font_instances) {
		const std::string fontBytes = readVariableTestFont();
		if (fontBytes.empty()) { std::cout << "SKIP runtime variable-font smoke: set XUI_VARIABLE_FONT to a variable TrueType font\n"; return; }
		constexpr std::string_view sample = "The quick brown fox 0123456789";
		constexpr std::string_view xuiSource = R"(
Window(size: 900x360) {
  Column(gap: 8px) {
    Text("The quick brown fox 0123456789")[id: "w100", class: "w100"]
    Text("The quick brown fox 0123456789")[id: "w300", class: "w300"]
    Text("The quick brown fox 0123456789")[id: "w400", class: "w400"]
    Text("The quick brown fox 0123456789")[id: "w500", class: "w500"]
    Text("The quick brown fox 0123456789")[id: "w700", class: "w700"]
    Text("The quick brown fox 0123456789")[id: "w900", class: "w900"]
  }
})";
		constexpr std::string_view xssSource = R"(
Text { resting: { font-size: 25px, fg: #ffffffff } }
Text.w100 { resting: { font-weight: 100 } }
Text.w300 { resting: { font-weight: 300 } }
Text.w400 { resting: { font-weight: 400 } }
Text.w500 { resting: { font-weight: 500 } }
Text.w700 { resting: { font-weight: 700 } }
Text.w900 { resting: { font-weight: 900 } }
)";
		StringInterner strings; UiAssetBundle assets; std::string error;
		CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error)); if (!error.empty()) return;
		const std::array<std::pair<std::string_view, f32>, 6> requestedCases{{
			std::pair<std::string_view, f32>{ "w100", 100.f }, { "w300", 300.f }, { "w400", 400.f },
			{ "w500", 500.f }, { "w700", 700.f }, { "w900", 900.f } }};
		for (const auto& [className, requested] : requestedCases) {
			MatchInput input;
			input.type = strings.intern("Text");
			const StrId classId = strings.intern(className);
			input.classes = std::span<const StrId>(&classId, 1);
			PropTable cascaded;
			assets.styles.resolve(assets.styles.match(input), stateBit(State::Resting), 0, cascaded);
			CHECK(cascaded.has(PropId::FontWeight));
			const f32 resolved = cascaded.has(PropId::FontWeight) ? cascaded.get(PropId::FontWeight).atoms[0].num : -1.f;
			std::cout << "font-weight xss-parse/cascade: class=" << className << " requested=" << requested
				<< " resolved=" << resolved << '\n';
			CHECK_NEAR(resolved, requested, 0.01f);
		}
		SoftRenderer renderer; CHECK(renderer.init({ 900, 360, 1.f, nullptr, false }));
		TextSystem text; CHECK(text.initAtlas(&renderer));
		const FontHandle font = text.loadFont(fontBytes, &error); CHECK(font.valid()); if (!font.valid()) return;
		CHECK(text.font(font)->hasWeightAxis()); if (!text.font(font)->hasWeightAxis()) return;
		const auto [axisMin, axisMax] = text.font(font)->weightAxisRange();
		std::cout << "font-weight axis: tag=wght min=" << axisMin << " default=" << text.font(font)->weightAxisDefault()
			<< " max=" << axisMax << '\n';
		UiRuntime ui; CHECK(ui.build(assets.document, assets.styles, strings, text, font, error)); if (!error.empty()) return;
		CHECK(ui.registerFont("default", 400, font, error)); if (!error.empty()) return;
		// Reproduce the GLFW demo's regular-variable + separately registered static-bold setup.
		const std::string staticFontBytes = readTestFont();
		FontHandle staticFont;
		if (!staticFontBytes.empty()) {
			staticFont = text.loadFont(staticFontBytes, &error);
			if (staticFont.valid() && !text.font(staticFont)->hasWeightAxis()) {
				CHECK(ui.registerFont("default", 700, staticFont, error));
				std::cout << "font-weight fallback face registered: weight=700 variable=no\n";
			}
		}
		const std::array<f32, 6> expectedWeights{
			std::clamp(100.f, axisMin, axisMax), std::clamp(300.f, axisMin, axisMax),
			std::clamp(400.f, axisMin, axisMax), std::clamp(500.f, axisMin, axisMax),
			std::clamp(700.f, axisMin, axisMax), std::clamp(900.f, axisMin, axisMax)
		};
		const std::array<std::string_view, 6> nodeIds{ "w100", "w300", "w400", "w500", "w700", "w900" };
		const std::array<f32, 6> widgetRequestedWeights{ 100, 300, 400, 500, 700, 900 };
		for (std::size_t i = 0; i < nodeIds.size(); ++i) {
			const f32 actual = UiRuntimeFontWeightProbe::appliedWeight(ui, nodeIds[i]);
			std::cout << "font-weight cascade/widget/text: requested=" << widgetRequestedWeights[i]
				<< " node=" << nodeIds[i] << " applied=" << actual << '\n';
			CHECK_NEAR(actual, expectedWeights[i], 0.01f);
		}
		ui.layout(900.f, 360.f);
		Painter painter; painter.begin(900, 360, 1.f); text.beginFrame(); ui.paint(painter);
		const DrawList list = painter.finish();
		renderer.beginFrame({ 900, 360, 1.f, Color{ 0, 0, 0, 1 }, 1 });
		renderer.submit(list); renderer.endFrame();
		const Image image = renderer.snapshot();
		std::unordered_set<u32> glyphs;
		for (char ch : sample) if (ch != ' ') glyphs.insert(text.font(font)->glyphIndex(static_cast<u8>(ch)));
		std::array<f32, 6> effectiveWeights{};
		constexpr std::array<f32, 6> requestedWeights{ 100, 300, 400, 500, 700, 900 };
		std::size_t uniqueEffectiveWeights = 0;
		for (std::size_t i = 0; i < requestedWeights.size(); ++i) {
			effectiveWeights[i] = std::clamp(requestedWeights[i], axisMin, axisMax);
			bool seen = false;
			for (std::size_t j = 0; j < i; ++j)
				if (std::abs(effectiveWeights[i] - effectiveWeights[j]) < 0.01f) seen = true;
			if (!seen) ++uniqueEffectiveWeights;
		}
		CHECK(text.rasterCount() >= glyphs.size() * uniqueEffectiveWeights);
		u32 glyphCommands = 0;
		for (u32 i = 0; i < list.nCmds; ++i) if (list.cmds[i].tex.valid()) ++glyphCommands;
		CHECK(glyphCommands >= 6);
		std::array<u32, 6> brightPixels{};
		for (u32 weight = 0; weight < brightPixels.size(); ++weight) {
			const Rect row = ui.bounds({ weight + 2, 1 });
			for (u32 y = static_cast<u32>(std::max(0.f, std::floor(row.y))); y < static_cast<u32>(std::min(360.f, std::ceil(row.bottom()))); ++y)
				for (u32 x = 0; x < image.width; ++x) {
					const u8* pixel = image.px(x, y);
					if (pixel[0] > 250 && pixel[1] > 250 && pixel[2] > 250) ++brightPixels[weight];
				}
			CHECK(brightPixels[weight] > 0);
			std::cout << "font-weight rendered row: requested=" << requestedWeights[weight]
				<< " effective=" << effectiveWeights[weight] << " bright-pixels=" << brightPixels[weight] << '\n';
		}
		auto rowsDiffer = [&](u32 first, u32 second) {
			const Rect a = ui.bounds({ first + 2, 1 }), b = ui.bounds({ second + 2, 1 });
			const u32 height = static_cast<u32>(std::max(0.f, std::min(a.h, b.h)));
			for (u32 y = 0; y < height; ++y)
				for (u32 x = 0; x < image.width; ++x) {
					const u8* pa = image.px(x, static_cast<u32>(a.y) + y);
					const u8* pb = image.px(x, static_cast<u32>(b.y) + y);
					if (pa[0] != pb[0] || pa[1] != pb[1] || pa[2] != pb[2]) return true;
				}
			return false;
		};
		bool visibleWeightDifference = false;
		for (std::size_t i = 0; i < effectiveWeights.size(); ++i)
			for (std::size_t j = i + 1; j < effectiveWeights.size(); ++j) {
				const bool different = rowsDiffer(static_cast<u32>(i), static_cast<u32>(j));
				if (std::abs(effectiveWeights[i] - effectiveWeights[j]) > 0.01f) {
					CHECK(different);
					if (different) visibleWeightDifference = true;
				}
				else CHECK(!different);
				if (std::abs(effectiveWeights[i] - effectiveWeights[j]) > 0.01f && brightPixels[i] != brightPixels[j])
					visibleWeightDifference = true;
			}
		CHECK(visibleWeightDifference);
		if (std::abs(effectiveWeights[4] - effectiveWeights[5]) < 0.01f)
			CHECK_EQ(brightPixels[4], brightPixels[5]);
		CHECK(brightPixels.front() != brightPixels.back());
	}

	TEST(ui_runtime_variable_font_weight_cascades_through_text_button_and_textbox) {
		const std::string fontBytes = readVariableTestFont();
		if (fontBytes.empty()) { std::cout << "SKIP variable-font cascade: set XUI_VARIABLE_FONT to a variable TrueType font\n"; return; }
		constexpr std::string_view xuiSource = R"(
Window(size: 360x320) {
  Column[class: "parent-weight"] {
    Text("Inherited from parent")[id: "inherited-text"]
    Button(label: "Inherited button")[id: "inherited-button", width: 240px, height: 42px]
    TextBox[id: "inherited-textbox", value: "Inherited editable value", width: 280px, height: 42px]
    Button(label: "Button weight override")[id: "button-weight", class: "button-weight", width: 240px, height: 42px]
    TextBox[id: "textbox-weight", class: "textbox-weight", value: "TextBox weight override", width: 280px, height: 42px]
  }
})";
		constexpr std::string_view xssSource = R"(
Column.parent-weight { resting: { font-weight: 700, font-size: 18px } }
Button.button-weight { resting: { font-weight: 500, font-size: 16px } }
TextBox.textbox-weight { resting: { font-weight: 900, font-size: 16px } }
)";
		StringInterner strings; UiAssetBundle assets; std::string error;
		CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error)); if (!error.empty()) return;
		SoftRenderer renderer; CHECK(renderer.init({ 360, 320, 1.f, nullptr, false }));
		TextSystem text; CHECK(text.initAtlas(&renderer));
		const FontHandle font = text.loadFont(fontBytes, &error); CHECK(font.valid()); if (!font.valid()) return;
		CHECK(text.font(font)->hasWeightAxis()); if (!text.font(font)->hasWeightAxis()) return;
		const auto [weightMin, weightMax] = text.font(font)->weightAxisRange();
		UiRuntime ui; CHECK(ui.build(assets.document, assets.styles, strings, text, font, error)); if (!error.empty()) return;
		CHECK_NEAR(UiRuntimeFontWeightProbe::appliedWeight(ui, "inherited-text"), std::clamp(700.f, weightMin, weightMax), 0.01f);
		CHECK_NEAR(UiRuntimeFontWeightProbe::appliedWeight(ui, "inherited-button"), std::clamp(700.f, weightMin, weightMax), 0.01f);
		CHECK_NEAR(UiRuntimeFontWeightProbe::appliedWeight(ui, "inherited-textbox"), std::clamp(700.f, weightMin, weightMax), 0.01f);
		CHECK_NEAR(UiRuntimeFontWeightProbe::appliedWeight(ui, "button-weight"), std::clamp(500.f, weightMin, weightMax), 0.01f);
		CHECK_NEAR(UiRuntimeFontWeightProbe::appliedWeight(ui, "textbox-weight"), std::clamp(900.f, weightMin, weightMax), 0.01f);
		ui.layout(360.f, 320.f);
		Painter painter; painter.begin(360, 320, 1.f); text.beginFrame(); ui.paint(painter);
		const DrawList list = painter.finish();
		renderer.beginFrame({ 360, 320, 1.f, Color{ 0, 0, 0, 1 }, 1 }); renderer.submit(list); renderer.endFrame();
		const Image image = renderer.snapshot();
		CHECK(image.width == 360 && image.height == 320);
		u32 glyphCommands = 0;
		for (u32 i = 0; i < list.nCmds; ++i) if (list.cmds[i].tex.valid()) ++glyphCommands;
		CHECK(glyphCommands >= 3);
		for (std::string_view id : { "inherited-text", "inherited-button", "inherited-textbox", "button-weight", "textbox-weight" }) {
			const Rect nodeBounds = UiRuntimeFontWeightProbe::bounds(ui, id);
			CHECK(nodeBounds.w > 0.f && nodeBounds.h > 0.f);
		}
	}

	TEST(ui_runtime_checklist_and_splitter_are_file_driven_and_interactive) {
		constexpr std::string_view xuiSource = R"(
Window(title: "Controls", size: 600x360) {
  Split(direction: Horizontal)[ratio: 0.4, gutter: 8, grow: 1] {
    Panel[class: "left"] {
      CheckList {
        CheckItem(label: "First", checked: true)[change: onChange]
        CheckItem(label: "Second")[change: onChange]
      }
    }
    Panel[class: "right"] { Text("Details") }
  }
}

)";
		constexpr std::string_view xssSource = R"(
Panel.left { resting: { background: #202a3b, padding: 12px } }
Panel.right { resting: { bg: #131a26, pad: 12px } }
CheckList { resting: { gap: 6px } }
CheckItem { resting: { color: #f0f0f0, size: 180px 32px, border: 1px dashed #718096, radius: 4px, font-family: "Example", font-size: 14px, font-weight: 400 }
  hover: { bg: #263752 }
  focus: { border: 2px solid #4f8cff }
  checked: { fg: #82aaff }
  disabled: { opacity: 0.4 }
}
Split { resting: { fg: #52627b } }
)";
		StringInterner strings;
		auto document = parseXui(xuiSource, "controls.xui", strings);
		auto styles = compileXss(xssSource, "controls.xss", strings);
		CHECK(document.has_value()); CHECK(styles.has_value());
		if (!document || !styles) return;
		Program program; program.files.push_back(*document);
		CHECK(validate(program, XuiSchema::withCoreWidgets(), strings).has_value());
		TextSystem text; UiRuntime ui; std::string error;
		CHECK(ui.build(*document, *styles, strings, text, {}, error));
		if (!error.empty()) return;
		ui.layout(600, 360);
		CHECK_EQ(ui.nodeCount(), 8u);
		const Rect leftBefore = ui.bounds({ 2, 1 });
		const Rect itemRect = ui.bounds({ 4, 1 });
		CHECK(leftBefore.w > 0.f); CHECK(itemRect.h == 32.f);
		u32 counts[2]{}; ui.setEventHandler(recordPress, counts);
		const Vec2 itemCenter{ itemRect.x + itemRect.w * 0.5f, itemRect.y + itemRect.h * 0.5f };
		CHECK(ui.pointerDown(itemCenter)); CHECK(ui.pointerUp(itemCenter));
		CHECK_EQ(counts[1], 1u);
		ui.keyDown(Key::Space);
		// The just-clicked checklist item remains focused and is keyboard-toggleable.
		CHECK_EQ(counts[1], 2u);
		const Rect splitRect = ui.bounds({ 1, 1 });
		const f32 gutterX = leftBefore.right() + 4.f;
		const Vec2 gutterCenter{ gutterX, splitRect.y + splitRect.h * 0.5f };
		CHECK(ui.pointerDown(gutterCenter));
		CHECK(ui.pointerMove({ gutterCenter.x + 24.f, gutterCenter.y }));
		ui.layout(600, 360);
		const Rect leftAfter = ui.bounds({ 2, 1 });
		CHECK(leftAfter.w > leftBefore.w + 10.f);
		ui.pointerUp({ gutterCenter.x + 24.f, gutterCenter.y });
	}

	TEST(ui_runtime_checklist_exposes_and_sets_selected_child_ids) {
		constexpr std::string_view xuiSource = R"(
Window(size: 240x120) {
  CheckList[id: "tasks", change: onListChange] {
    CheckItem(label: "First", checked: true)[id: "first"]
    CheckItem(label: "Second")[id: "second"]
  }
}
)";
		constexpr std::string_view xssSource = R"(
CheckItem { resting: { size: 200px 36px, bg: #f3f3f3, fg: #242424 }
  focus: { outline: 2px solid #0f6cbd, outline-offset: 2px } }
)";
		StringInterner strings;
		auto document = parseXui(xuiSource, "checklist-values.xui", strings);
		auto styles = compileXss(xssSource, "checklist-values.xss", strings);
		CHECK(document.has_value()); CHECK(styles.has_value());
		if (!document || !styles) return;
		TextSystem text; UiRuntime ui; std::string error;
		CHECK(ui.build(*document, *styles, strings, text, {}, error));
		if (!error.empty()) return;
		ui.layout(240.f, 120.f);
		CheckListProbe probe{ &ui, 0, {} };
		ui.setEventHandler(recordCheckList, &probe);
		auto selected = ui.value("tasks");
		CHECK(selected.has_value());
		if (selected) {
			const auto* ids = std::get_if<CheckListValue>(&*selected);
			CHECK(ids != nullptr);
			if (ids) { CHECK_EQ(ids->size(), 1u); CHECK_EQ(ids->front(), std::string("first")); }
		}
		CHECK(ui.setValue("tasks", CheckListValue{ "second" }, true));
		CHECK_EQ(probe.calls, 1u); CHECK_EQ(probe.selected.size(), 1u);
		CHECK_EQ(probe.selected.front(), std::string("second"));
		CHECK(!ui.setValue("tasks", CheckListValue{ "missing" }));
		selected = ui.value("tasks");
		CHECK(selected && std::get<CheckListValue>(*selected) == CheckListValue{ "second" });

		const Rect first = ui.bounds({ 2, 1 });
		const Vec2 center{ first.x + first.w * 0.5f, first.y + first.h * 0.5f };
		Painter painter; painter.begin(240, 120, 1.f); ui.paint(painter); const u32 restingPrims = painter.finish().nVerts;
		CHECK(ui.pointerDown(center));
		Painter focusedPainter; focusedPainter.begin(240, 120, 1.f); ui.paint(focusedPainter); const u32 focusedPrims = focusedPainter.finish().nVerts;
		CHECK(focusedPrims > restingPrims);
		CHECK(ui.pointerUp(center));
		selected = ui.value("tasks");
		CHECK(selected && std::get<CheckListValue>(*selected) == CheckListValue({ "first", "second" }));
		CHECK_EQ(probe.calls, 2u);
		CHECK(ui.keyDown(Key::Space));
		selected = ui.value("tasks");
		CHECK(selected && std::get<CheckListValue>(*selected) == CheckListValue{ "second" });
		CHECK_EQ(probe.calls, 3u);
	}

	TEST(ui_runtime_dock_host_tabs_split_resize_float_and_restore) {
		constexpr std::string_view xuiSource = R"(
Window(title: "Dock test", size: 640x420) {
  DockHost[id: "workspace", class: "workspace", width: 100%, height: 100%, gutter: 8, headerHeight: 30] {
    DockPanel(title: "Editor")[id: "editor", region: "Center", minSize: 220x140, closable: false] {
      Text("Editor content")[width: 100%, height: 100%]
    }
    DockPanel(title: "Inspector")[id: "inspector", region: "Right", target: "editor", minSize: 130x140] {
      Text("Inspector content")
    }
    DockPanel(title: "Console")[id: "console", region: "Center", target: "editor", minSize: 140x80] {
      Text("Console content")
    }
  }
}
)";
		constexpr std::string_view xssSource = R"(
DockHost.workspace { resting: { bg: #111827, pad: 4px, min-size: 480px 320px } }
DockPanel { resting: { bg: #182438, pad: 8px, font-size: 14px, fg: #eef4ff } }
DockHeader { resting: { bg: #27364d, fg: #eef4ff, pad: 8px, radius: 4px } hover: { bg: #354a68 } }
DockTab { resting: { bg: #202c40, fg: #bac9df, pad: 6px } hover: { bg: #344760 } selected: { bg: #344760, fg: #8fc0ff } }
DockCloseButton { resting: { fg: #bac9df } hover: { bg: #9c3e52, fg: #ffffff } }
DockSplitter { resting: { bg: #465a74 } hover: { bg: #69a7ff } pressed: { bg: #91bdff } }
DockPreview { resting: { bg: #5386d866, border: 1px solid #69a7ff, radius: 4px } }
)";
		StringInterner strings;
		auto document = parseXui(xuiSource, "dock-runtime.xui", strings);
		auto styles = compileXss(xssSource, "dock-runtime.xss", strings);
		CHECK(document.has_value()); CHECK(styles.has_value());
		if (!document || !styles) return;
		TextSystem text; UiRuntime ui; std::string error;
		CHECK(ui.build(*document, *styles, strings, text, {}, error));
		if (!error.empty()) return;
		ui.layout(640.f, 420.f);
		const Rect editor = ui.bounds({ 2, 1 }), inspector = ui.bounds({ 4, 1 }), console = ui.bounds({ 6, 1 });
		CHECK(editor.w >= 220.f); CHECK(editor.h >= 140.f);
		CHECK(inspector.w >= 130.f); CHECK(console.h >= 80.f);
		CHECK(inspector.x >= editor.right());
		CHECK(!std::get<DockPanelValue>(*ui.value("editor")).active);
		CHECK(std::get<DockPanelValue>(*ui.value("console")).activeTab == "console");

		// Splitter drag updates the adjacent regions while the panel tree is nested.
		const Vec2 splitPoint{ inspector.x - 3.5f, inspector.y + 90.f };
		CHECK(ui.pointerDown(splitPoint));
		CHECK(ui.pointerMove({ splitPoint.x - 24.f, splitPoint.y }));
		ui.layout(640.f, 420.f);
		CHECK(ui.bounds({ 4, 1 }).w > inspector.w);
		ui.pointerUp({ splitPoint.x - 24.f, splitPoint.y });

		// Clicking a tab activates it; dragging another region onto the group center creates a tab.
		const Rect editorAfterSplit = ui.bounds({ 2, 1 });
		const Vec2 editorTab{ editorAfterSplit.x + editorAfterSplit.w * 0.25f, editorAfterSplit.y + 14.f };
		CHECK(ui.pointerDown(editorTab)); CHECK(ui.pointerUp(editorTab));
		CHECK(std::get<DockPanelValue>(*ui.value("editor")).active);
		const Rect inspectorBeforeDock = ui.bounds({ 4, 1 });
		const Vec2 inspectorHeader{ inspectorBeforeDock.x + 28.f, inspectorBeforeDock.y + 14.f };
		CHECK(ui.pointerDown(inspectorHeader));
		const Vec2 editorBody{ editorAfterSplit.x + editorAfterSplit.w * 0.5f, editorAfterSplit.y + editorAfterSplit.h * 0.6f };
		CHECK(ui.pointerMove(editorBody));
		ui.pointerUp(editorBody);
		CHECK(!std::get<DockPanelValue>(*ui.value("inspector")).floating);
		CHECK(std::get<DockPanelValue>(*ui.value("inspector")).activeTab == "inspector");

		// A drag released beyond the host creates an in-app floating overlay.
		const Rect consoleAfterSplit = ui.bounds({ 6, 1 });
		const Vec2 tabPoint{ consoleAfterSplit.x + consoleAfterSplit.w * 0.5f, consoleAfterSplit.y + 14.f };
		CHECK(ui.pointerDown(tabPoint));
		CHECK(ui.pointerMove({ tabPoint.x + 20.f, tabPoint.y + 14.f }));
		CHECK(ui.pointerUp({ 650.f, 430.f }));
		auto floating = std::get<DockPanelValue>(*ui.value("console"));
		CHECK(floating.floating);

		DockPanelValue restore = floating;
		restore.floating = false; restore.targetPanel = "editor"; restore.region = DockZone::Center; restore.active = true;
		CHECK(ui.setValue("console", restore));
		CHECK(!std::get<DockPanelValue>(*ui.value("console")).floating);
	}

	std::string readExample(std::string_view filename) {
		const std::filesystem::path path = std::filesystem::path(XUI_EXAMPLE_DIR) / filename;
		std::ifstream file(path, std::ios::binary);
		if (!file) return {};
		return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	}
	std::string readFixture(std::string_view filename) {
		const std::filesystem::path path = std::filesystem::path(XUI_TEST_FIXTURE_DIR) / filename;
		std::ifstream file(path, std::ios::binary);
		if (!file) return {};
		return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	}
}

TEST(ui_runtime_pointer_stacking_activates_only_the_front_widget) {
	constexpr std::string_view xuiSource = R"(
Window(size: 400x220) {
  Column {
    Dropdown[id: "behind", options: "One|Two|Three", value: "One", change: onDropdown, width: 220px, height: 120px]
    Tabs[id: "front-tabs", selected: 0, width: 220px, height: 150px, class: "overlap-tabs"] {
      Tab(title: "Front") { Button(label: "Front page action")[id: "front", press: onFront, width: 220px, height: 34px] }
      Tab(title: "Other") { Text("Other page") }
    }
  }
}

)";
	constexpr std::string_view xssSource = R"(
Dropdown { resting: { bg: #223344, fg: #ffffff } }
Tabs.overlap-tabs { resting: { margin: -50px, bg: #101820 } }
Button { resting: { bg: #446688, fg: #ffffff } }
)";
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error));
	if (!error.empty()) return;
	TextSystem text; UiRuntime ui;
	const bool built = ui.build(assets.document, assets.styles, strings, text, {}, error);
	if (!built) std::cerr << "overlap fixture: " << error << '\n';
	CHECK(built);
	if (!built) return;
	ui.layout(400, 220);
	const Rect behind = ui.bounds({ 2, 1 }), front = ui.bounds({ 5, 1 });
	const f32 overlapTop = std::max(behind.y, front.y);
	const f32 overlapBottom = std::min(behind.bottom(), front.bottom());
	CHECK(overlapBottom > overlapTop);
	if (overlapBottom <= overlapTop) return;

	std::vector<std::string> events;
	ui.setEventHandler([](void* context, std::string_view name) {
		static_cast<std::vector<std::string>*>(context)->emplace_back(name);
		}, &events);
	const f32 overlapLeft = std::max({ behind.x, front.x, 0.f });
	const Vec2 overlapPoint{ overlapLeft + 12.f, (overlapTop + overlapBottom) * 0.5f };
	CHECK(ui.pointerMove(overlapPoint));
	CHECK(ui.pointerDown(overlapPoint));
	CHECK(ui.pointerUp(overlapPoint));
	CHECK_EQ(events.size(), 1u);
	CHECK(events.back() == "onFront");

	// If the covered dropdown received the first pointer event, its popup would
	// consume this point and invoke onDropdown instead of leaving the value alone.
	const Vec2 coveredPopupPoint{ behind.x + 12.f, behind.bottom() + 8.f };
	CHECK(ui.pointerDown(coveredPopupPoint));
	CHECK(ui.pointerUp(coveredPopupPoint));
	CHECK(ui.controlText("behind") == "One");
	CHECK_EQ(events.size(), 2u);
	CHECK(events.back() == "onFront");
}

TEST(ui_runtime_button_hugs_nested_children_and_respects_min_max) {
	constexpr std::string_view xuiSource = R"(
Window(size: 420x180) {
  Row(gap: 12) { Button[id: "hug", class: "hug", press: onSubmit] { Text("Nested button content") } }
}
)";
	constexpr std::string_view xssSource = R"(
Button.hug { resting: { size: auto, pad: 12px, min-width: 90px, max-width: 240px, bg: #304860, fg: #ffffff, radius: 6px } }
)";
	StringInterner strings; auto document = parseXui(xuiSource, "button-hug.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "button-hug.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	SoftRenderer renderer; CHECK(renderer.init({ 420, 180, 1.f, nullptr, false }));
	TextSystem text; CHECK(text.initAtlas(&renderer)); UiRuntime ui; std::string error;
	const std::string fontBytes = readTestFont(); const FontHandle font = fontBytes.empty() ? FontHandle{} : text.loadFont(fontBytes, &error);
	const bool built = ui.build(*document, *styles, strings, text, font, error);
	if (!built) std::cerr << "nested button build: " << error << '\n';
	CHECK(built); if (!error.empty()) return;
	ui.layout(420.f, 180.f);
	const Rect button = ui.bounds({ 2, 1 }), child = ui.bounds({ 3, 1 });
	CHECK(button.w >= 90.f && button.w <= 240.f);
	CHECK(button.w >= child.w + 20.f);
	CHECK(button.h >= child.h + 20.f);
}

TEST(ui_runtime_xss_hover_transition_interpolates_without_idle_timer) {
	constexpr std::string_view xuiSource = R"(Window(size: 240x120) { Button[id: "animated", width: 160px, height: 48px] })";
	constexpr std::string_view xssSource = R"(
Button { resting: { bg: #000000ff } hover: { bg: #ffffffff } transition: resting -> hover 120ms linear }
)";
	StringInterner strings; auto document = parseXui(xuiSource, "animation.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "animation.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.layout(240.f, 120.f); Painter painter;
	auto paintColor = [&]() {
		painter.begin(240, 120, 1.f); ui.paint(painter, 1.f);
		const DrawList list = painter.finish(); return list.nVerts ? list.verts[list.nVerts - 1].c0 : 0u;
	};
	const u32 resting = paintColor();
	const Rect rect = ui.bounds({ 1, 1 });
	CHECK(ui.pointerMove({ rect.x + 8.f, rect.y + 8.f }));
	(void)paintColor();
	std::this_thread::sleep_for(std::chrono::milliseconds(65));
	const u32 midway = paintColor();
	CHECK(midway != resting);
	CHECK(ui.nextTimerDelay() >= 0.0);
	std::this_thread::sleep_for(std::chrono::milliseconds(90));
	const u32 complete = paintColor();
	CHECK(complete == 0xFFFFFFFFu);
}

TEST(ui_runtime_animation_timer_continues_without_pointer_input_and_after_leave) {
	constexpr std::string_view xuiSource = R"(Window(size: 240x120) { Button[id: "animated", width: 160px, height: 48px] })";
	constexpr std::string_view xssSource = R"(
Button { resting: { bg: #000000ff } hover: { bg: #ffffffff }
  transition: resting -> hover 120ms linear
  transition: hover -> resting 120ms linear }
)";
	StringInterner strings; auto document = parseXui(xuiSource, "animation-leave.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "animation-leave.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.layout(240.f, 120.f); Painter painter;
	auto paintColor = [&]() {
		painter.begin(240, 120, 1.f); ui.paint(painter, 1.f);
		const DrawList list = painter.finish(); return list.nVerts ? list.verts[list.nVerts - 1].c0 : 0u;
	};
	(void)paintColor(); // establish the resting style before triggering a state transition
	const Rect rect = ui.bounds({ 1, 1 });
	CHECK(ui.pointerMove({ rect.x + 8.f, rect.y + 8.f }));
	(void)paintColor();
	const f64 frameDelay = ui.nextTimerDelay();
	CHECK(frameDelay > 0.0 && frameDelay <= 1.0 / 60.0);
	const auto scheduledAt = std::chrono::steady_clock::now();
	CHECK(ui.updateTimers(scheduledAt));
	const f64 remainingFrameDelay = ui.nextTimerDelay(scheduledAt + std::chrono::milliseconds(5));
	CHECK(remainingFrameDelay > 0.0 && remainingFrameDelay < frameDelay);
	std::this_thread::sleep_for(std::chrono::milliseconds(65));
	CHECK(ui.updateTimers()); // no pointer events are needed to schedule the next animation frame
	const u32 hoverSample = paintColor();
	CHECK(ui.pointerLeave());
	(void)paintColor(); // resolves hover -> resting into a new active transition
	CHECK(ui.nextTimerDelay() > 0.0);
	std::this_thread::sleep_for(std::chrono::milliseconds(65));
	CHECK(ui.updateTimers()); // the mouse-leave event does not disable the timer path
	const u32 leaveSample = paintColor();
	CHECK(leaveSample != hoverSample);
	std::this_thread::sleep_for(std::chrono::milliseconds(75));
	CHECK(ui.updateTimers());
	CHECK_EQ(paintColor(), Color::fromRGBA8(0x000000FFu).toRGBA8());
	CHECK(ui.nextTimerDelay() < 0.0);
}

TEST(ui_runtime_animation_cancel_finish_completes_transition_before_new_state) {
	constexpr std::string_view xuiSource = R"(Window(size: 240x120) { Button[id: "animated", width: 160px, height: 48px] })";
	constexpr std::string_view xssSource = R"(
Button { resting: { bg: #000000ff, animation-cancel: finish } hover: { bg: #ffffffff }
  transition: resting -> hover 120ms linear
  transition: hover -> resting 120ms linear }
)";
	StringInterner strings; auto document = parseXui(xuiSource, "animation-finish.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "animation-finish.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.layout(240.f, 120.f); Painter painter;
	auto paintColor = [&]() {
		painter.begin(240, 120, 1.f); ui.paint(painter, 1.f);
		const DrawList list = painter.finish(); return list.nVerts ? list.verts[list.nVerts - 1].c0 : 0u;
	};
	(void)paintColor();
	const Rect rect = ui.bounds({ 1, 1 });
	CHECK(ui.pointerMove({ rect.x + 8.f, rect.y + 8.f }));
	(void)paintColor();
	std::this_thread::sleep_for(std::chrono::milliseconds(65));
	(void)paintColor();
	CHECK(ui.pointerLeave());
	(void)paintColor(); // finish the hover transition before applying the resting target
	std::this_thread::sleep_for(std::chrono::milliseconds(75));
	CHECK(ui.updateTimers());
	CHECK_EQ(paintColor(), Color::fromRGBA8(0xFFFFFFFFu).toRGBA8());
	(void)paintColor();
	CHECK(ui.nextTimerDelay() > 0.0); // the resting transition starts after the hover target completes
	std::this_thread::sleep_for(std::chrono::milliseconds(140));
	CHECK(ui.updateTimers());
	CHECK_EQ(paintColor(), Color::fromRGBA8(0x000000FFu).toRGBA8());
	CHECK(ui.nextTimerDelay() < 0.0);
}

TEST(ui_runtime_animation_clock_pauses_and_resumes_for_hidden_window) {
	constexpr std::string_view xuiSource = R"(Window(size: 240x120) { Button[id: "animated", width: 160px, height: 48px] })";
	constexpr std::string_view xssSource = R"(
Button { resting: { bg: #000000ff } hover: { bg: #ffffffff } transition: resting -> hover 180ms linear }
)";
	StringInterner strings; auto document = parseXui(xuiSource, "animation-hidden.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "animation-hidden.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.layout(240.f, 120.f); Painter painter;
	auto paint = [&]() { painter.begin(240, 120, 1.f); ui.paint(painter, 1.f); (void)painter.finish(); };
	paint(); const Rect rect = ui.bounds({ 1, 1 });
	ui.pointerMove({ rect.x + 8.f, rect.y + 8.f }); paint();
	const auto pausedAt = std::chrono::steady_clock::now();
	ui.setAnimationsPaused(true, pausedAt);
	std::this_thread::sleep_for(std::chrono::milliseconds(220));
	CHECK(!ui.updateTimers()); CHECK(ui.nextTimerDelay() < 0.0);
	ui.setAnimationsPaused(false);
	CHECK(ui.nextTimerDelay() > 0.0 && ui.nextTimerDelay() <= 1.0 / 60.0);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	CHECK(ui.updateTimers()); paint(); // animation retains its pre-hide elapsed time
	CHECK(ui.nextTimerDelay() > 0.0);
}

TEST(ui_runtime_animation_cancel_finish_completes_current_keyframe_iteration) {
	constexpr std::string_view xuiSource = R"(Window(size: 240x120) { Panel[id: "animated", width: 160px, height: 48px] })";
	constexpr std::string_view xssSource = R"(
Panel { resting: { bg: #000000ff } hover: { animation: pulse 180ms infinite linear cancel(finish) }
  transition: resting -> hover 0ms linear }
keyframes pulse { from { bg: #000000ff } to { bg: #ffffffff } }
)";
	StringInterner strings; auto document = parseXui(xuiSource, "animation-keyframe-finish.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "animation-keyframe-finish.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.layout(240.f, 120.f); Painter painter;
	auto paintColor = [&]() {
		painter.begin(240, 120, 1.f); ui.paint(painter, 1.f);
		const DrawList list = painter.finish(); return list.nVerts ? list.verts[list.nVerts - 1].c0 : 0u;
	};
	(void)paintColor();
	const Rect rect = ui.bounds({ 1, 1 });
	ui.pointerMove({ rect.x + 8.f, rect.y + 8.f });
	(void)paintColor(); // enter the hover keyframe animation
	std::this_thread::sleep_for(std::chrono::milliseconds(45));
	(void)paintColor();
	ui.pointerLeave();
	(void)paintColor(); // finish the current infinite iteration, then leave the hover state
	std::this_thread::sleep_for(std::chrono::milliseconds(160));
	CHECK(ui.updateTimers());
	CHECK_EQ(paintColor(), Color::fromRGBA8(0xFFFFFFFFu).toRGBA8());
	CHECK_EQ(paintColor(), Color::fromRGBA8(0x000000FFu).toRGBA8());
	CHECK(ui.nextTimerDelay() < 0.0);
}

TEST(ui_runtime_f4_draw_batch_counter_is_independent_of_fps_sampling) {
	UiRuntime ui;
	CHECK(!ui.drawBatchCounterEnabled());
	CHECK(ui.keyDown(Key::F4));
	CHECK(ui.drawBatchCounterEnabled());
	CHECK(!ui.fpsCounterEnabled()); // the draw counter must not enable continuous FPS rendering
	CHECK(ui.keyDown(Key::F4));
	CHECK(!ui.drawBatchCounterEnabled());
}

TEST(ui_runtime_xss_keyframes_tick_only_while_visible_animation_runs) {
	constexpr std::string_view xuiSource = R"(Window(size: 240x120) { Panel[id: "animated", class: "animated", width: 160px, height: 48px] })";
	constexpr std::string_view xssSource = R"(
Panel.animated { resting: { bg: #000000ff, animation: fade-in 120ms 1 linear } }
keyframes fade-in { from { opacity: 0, bg: #000000ff } to { opacity: 1, bg: #ffffffff } }
)";
	StringInterner strings; auto document = parseXui(xuiSource, "keyframes.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "keyframes.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.layout(240.f, 120.f); Painter painter;
	auto paintColor = [&]() {
		painter.begin(240, 120, 1.f); ui.paint(painter, 1.f);
		const DrawList list = painter.finish(); return list.nVerts ? list.verts[list.nVerts - 1].c0 : 0u;
	};
	const u32 initial = paintColor();
	const f64 firstFrameDelay = ui.nextTimerDelay();
	CHECK(firstFrameDelay > 0.0 && firstFrameDelay <= 1.0 / 60.0);
	std::this_thread::sleep_for(std::chrono::milliseconds(65));
	// No pointer or keyboard input is sent: the animation timer alone must wake the runtime.
	CHECK(ui.updateTimers());
	const u32 midway = paintColor();
	CHECK(midway != initial);
	std::this_thread::sleep_for(std::chrono::milliseconds(90));
	CHECK(paintColor() == 0xFFFFFFFFu);
	CHECK(ui.nextTimerDelay() < 0.0);
}

TEST(ui_runtime_keyframe_play_cancel_and_progress_value) {
	constexpr std::string_view xuiSource = R"(Window(size: 240x120) { ProgressBar[id: "meter", class: "meter", value: 0, width: 180px, height: 18px] })";
	constexpr std::string_view xssSource = R"(
ProgressBar.meter { resting: { animation: fill 120ms paused linear } }
keyframes fill { from { value: 0 } to { value: 1 } }
)";
	StringInterner strings; auto document = parseXui(xuiSource, "animation-control.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "animation-control.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.layout(240.f, 120.f); Painter painter;
	auto paint = [&]() { painter.begin(240, 120, 1.f); ui.paint(painter, 1.f); (void)painter.finish(); };
	paint();
	CHECK_EQ(std::get<f64>(*ui.value("meter")), 0.0);
	CHECK(ui.nextTimerDelay() < 0.0); // paused animations do not keep the event loop awake
	CHECK(ui.playAnimation("meter", AnimationMode::Once));
	CHECK(ui.nextTimerDelay() > 0.0 && ui.nextTimerDelay() <= 1.0 / 60.0);
	std::this_thread::sleep_for(std::chrono::milliseconds(65));
	CHECK(ui.updateTimers()); paint();
	const f64 halfway = std::get<f64>(*ui.value("meter"));
	CHECK(halfway > 0.0 && halfway < 1.0);
	std::this_thread::sleep_for(std::chrono::milliseconds(90));
	CHECK(ui.updateTimers()); paint();
	CHECK_EQ(std::get<f64>(*ui.value("meter")), 1.0);
	CHECK(ui.nextTimerDelay() < 0.0);
	CHECK(ui.setValue("meter", 0.0));
	CHECK(ui.playAnimation("meter", AnimationMode::Loop));
	std::this_thread::sleep_for(std::chrono::milliseconds(65));
	CHECK(ui.updateTimers()); paint();
	const f64 loopValue = std::get<f64>(*ui.value("meter"));
	CHECK(loopValue > 0.0 && loopValue < 1.0);
	CHECK(ui.cancelAnimation("meter"));
	paint();
	CHECK(std::fabs(std::get<f64>(*ui.value("meter")) - loopValue) < 0.01);
	CHECK(ui.nextTimerDelay() < 0.0);
	CHECK(!ui.cancelAnimation("meter"));
}

TEST(ui_runtime_xss_cursor_overrides_widget_default) {
	constexpr std::string_view xuiSource = R"(Window(size: 180x100) { Button[id: "custom-cursor", class: "crosshair", width: 120px, height: 40px] })";
	constexpr std::string_view xssSource = R"(Button.crosshair { resting: { cursor: crosshair } })";
	StringInterner strings; auto document = parseXui(xuiSource, "cursor.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "cursor.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.layout(180.f, 100.f); const Rect bounds = ui.bounds({ 1, 1 });
	CHECK(ui.cursorAt({ bounds.x + 8.f, bounds.y + 8.f }) == CursorShape::Crosshair);
}

TEST(ui_runtime_menu_bar_is_a_styled_row_for_client_area_menus) {
	constexpr std::string_view xuiSource = R"(
Window(size: 260x120) {
  MenuBar[class: "menu", width: 100%, height: 40px] {
    Dropdown[id: "file-menu", options: "New|Open|Save", value: "Open", width: 112px, height: 32px]
    Button(label: "Help", width: 72px, height: 32px)
  }
})";
	constexpr std::string_view xssSource = R"(
MenuBar.menu { resting: { bg: #202a3aff, pad: 4px, gap: 6px } }
Dropdown { resting: { bg: #33445eff, fg: #ffffffff, radius: 4px } }
)";
	StringInterner strings; auto document = parseXui(xuiSource, "menu-bar.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "menu-bar.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.layout(260.f, 120.f);
	const Rect menu = ui.bounds({ 1, 1 }), file = ui.bounds({ 2, 1 }), help = ui.bounds({ 3, 1 });
	CHECK(menu.w == 260.f && menu.h == 40.f && file.x < help.x);
	CHECK(ui.pointerDown({ file.x + 20.f, file.y + 12.f }));
	CHECK(ui.pointerDown({ file.x + 20.f, file.bottom() + 70.f }));
	CHECK(std::get<std::string>(*ui.value("file-menu")) == "Save");
}

TEST(ui_runtime_transform_moves_paint_and_hit_testing_together) {
	constexpr std::string_view xuiSource = R"(Window(size: 180x100) { Button[id: "moved", class: "moved", width: 80px, height: 40px, press: onPress] })";
	constexpr std::string_view xssSource = R"(Button.moved { resting: { bg: #ffffffff, transform: translate(40px, 0px) } })";
	StringInterner strings; auto document = parseXui(xuiSource, "transform.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "transform.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error; u32 calls = 0;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.setEventHandler([](void* context, std::string_view name) { if (name == "onPress") ++*static_cast<u32*>(context); }, &calls);
	ui.layout(180.f, 100.f); Painter painter; painter.begin(180, 100, 1.f); ui.paint(painter); (void)painter.finish();
	const Rect original = ui.bounds({ 1, 1 });
	CHECK(ui.pointerDown({ original.x + original.w * 0.5f + 40.f, original.y + original.h * 0.5f }));
	CHECK(ui.pointerUp({ original.x + original.w * 0.5f + 40.f, original.y + original.h * 0.5f }));
	CHECK_EQ(calls, 1u);
}

TEST(ui_runtime_empty_textbox_displays_focused_blinking_caret) {
	constexpr std::string_view xuiSource = R"(Window(size: 240x100) { TextBox[id: "field", placeholder: "Type here", width: 180px, height: 40px] })";
	constexpr std::string_view xssSource = R"(TextBox { resting: { fg: #ffffffff, bg: #202a3aff, pad: 8px } })";
	StringInterner strings; auto document = parseXui(xuiSource, "caret.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "caret.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; NullRenderer renderer; CHECK(text.initAtlas(&renderer));
	const std::string bytes = readTestFont(); std::string error;
	const FontHandle font = bytes.empty() ? FontHandle{} : text.loadFont(bytes, &error);
	UiRuntime ui; CHECK(ui.build(*document, *styles, strings, text, font, error)); if (!error.empty()) return;
	ui.layout(240.f, 100.f); const Rect bounds = ui.bounds({ 1, 1 });
	CHECK(ui.pointerDown({ bounds.x + 12.f, bounds.y + bounds.h * 0.5f }));
	Painter painter;
	auto hasCaret = [&]() {
		painter.begin(240, 100, 1.f); ui.paint(painter, 1.f); const DrawList list = painter.finish();
		for (u32 i = 0; i < list.nVerts; ++i) if (list.verts[i].prim == static_cast<u8>(Prim::Capsule)) return true;
		return false;
	};
	CHECK(hasCaret());
	std::this_thread::sleep_for(std::chrono::milliseconds(540));
	CHECK(ui.updateTimers());
	CHECK(!hasCaret());
}

TEST(ui_runtime_transparent_background_remains_hit_testable) {
	constexpr std::string_view xuiSource = R"(Window(size: 180x100) { Button[id: "transparent", class: "alpha", width: 120px, height: 40px, press: onSubmit] })";
	constexpr std::string_view xssSource = R"(Button.alpha { resting: { bg: #ff000000 } })";
	StringInterner strings; auto document = parseXui(xuiSource, "alpha.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss(xssSource, "alpha.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error; u32 calls = 0;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.setEventHandler([](void* p, std::string_view) { ++*static_cast<u32*>(p); }, &calls);
	ui.layout(180.f, 100.f); const Rect rect = ui.bounds({ 1, 1 });
	CHECK(ui.pointerDown({ rect.x + 10.f, rect.y + 10.f })); CHECK(ui.pointerUp({ rect.x + 10.f, rect.y + 10.f }));
	CHECK_EQ(calls, 1u);
}

TEST(ui_runtime_nested_tabs_respect_visibility_of_all_ancestors) {
	constexpr std::string_view xuiSource = R"(
Window(size: 320x180) {
  Tabs[id: "outer", selected: 0, width: 100%, height: 100%] {
    Tab(title: "Visible") {
      Button[id: "visible", label: "Visible", press: onVisible, width: 180px, height: 44px]
    }
    Tab(title: "Hidden") {
      Tabs[id: "inner", selected: 0, width: 100%, height: 100%] {
        Tab(title: "Inner") {
          Button[id: "hidden", label: "Must not intercept", press: onHidden, width: 180px, height: 44px]
        }
        Tab(title: "Other") { Text("Other page") }
      }
    }
  }
})";
	StringInterner strings; auto document = parseXui(xuiSource, "nested-tabs.xui", strings);
	CHECK(document.has_value()); if (!document) return;
	auto styles = compileXss("", "nested-tabs.xss", strings);
	CHECK(styles.has_value()); if (!styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error)); if (!error.empty()) return;
	std::vector<std::string> events;
	ui.setEventHandler([](void* context, std::string_view name) {
		static_cast<std::vector<std::string>*>(context)->emplace_back(name);
	}, &events);
	ui.layout(320.f, 180.f);
	const Rect active = ui.bounds({ 3, 1 });
	const Vec2 point{ active.x + active.w * 0.5f, active.y + active.h * 0.5f };
	CHECK(ui.pointerDown(point)); CHECK(ui.pointerUp(point));
	CHECK_EQ(events.size(), std::size_t{ 1 });
	if (!events.empty()) CHECK_EQ(events.front(), "onVisible");
}

TEST(cross_platform_example_assets_parse_and_build_in_shared_runtime) {
	StringInterner strings;
	const std::string xuiSource = readFixture("cross_platform.xui");
	const std::string xssSource = readFixture("cross_platform.xss");
	CHECK(!xuiSource.empty()); CHECK(!xssSource.empty());
	UiAssetBundle assets; std::string error;
	const bool loaded = loadUiAssets(xuiSource, xssSource, strings, assets, error, "cross_platform.xui", "cross_platform.xss");
	if (!loaded) std::cerr << "cross-platform assets: " << error << '\n';
	CHECK(loaded);
	if (!error.empty()) return;
	TextSystem text; UiRuntime ui;
	CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error));
	if (!error.empty()) return;
	ui.layout(1000, 680);
	CHECK_EQ(ui.nodeCount(), 23u);
	const Rect slider = ui.bounds({ 5, 1 });
	CHECK(slider.w > 0.f);
	const f32 originalValue = ui.controlValue({ 5, 1 });
	ui.pointerDown({ slider.x + slider.w * 0.95f, slider.y + slider.h * 0.5f });
	ui.pointerUp({ slider.x + slider.w * 0.95f, slider.y + slider.h * 0.5f });
	CHECK(ui.controlValue({ 5, 1 }) > originalValue);
	CHECK(ui.keyDown(Key::F3));
	CHECK(ui.fpsCounterEnabled());
	const auto fpsStart = std::chrono::steady_clock::now();
	const f64 fpsDelay = ui.nextTimerDelay(fpsStart);
	CHECK(fpsDelay > 0.20 && fpsDelay <= 0.25);
	CHECK(ui.updateTimers(fpsStart + std::chrono::milliseconds(300)));
	const f64 nextFpsDelay = ui.nextTimerDelay(fpsStart + std::chrono::milliseconds(300));
	CHECK(nextFpsDelay > 0.20 && nextFpsDelay <= 0.25);
	CHECK(ui.keyDown(Key::F3));
	CHECK(!ui.fpsCounterEnabled());
	const Rect split = ui.bounds({ 8, 1 });
	const Rect left = ui.bounds({ 9, 1 });
	CHECK(split.w > 0.f); CHECK(left.w > 0.f);
	const f32 gutterX = left.right() + 4.f;
	const Vec2 gutter{ gutterX, split.y + split.h * 0.5f };
	CHECK(ui.pointerDown(gutter));
	CHECK(ui.pointerMove({ gutter.x + 20.f, gutter.y }));
	ui.layout(1000, 680);
	CHECK(ui.bounds({ 9, 1 }).w > left.w + 5.f);
	ui.pointerUp({ gutter.x + 20.f, gutter.y });
}

TEST(xui_facade_loads_assets_attaches_backends_and_runs_frames) {
	Xui app;
	const auto xuiPath = (std::filesystem::path(XUI_TEST_FIXTURE_DIR) / "cross_platform.xui").string();
	const auto xssPath = (std::filesystem::path(XUI_TEST_FIXTURE_DIR) / "cross_platform.xss").string();
	CHECK(app.loadXui(xuiPath));
	CHECK(app.loadXss(xssPath));
	if (!app.lastError().empty()) return;
	NullRenderer renderer;
	NullPlatform platform;
	CHECK(app.attach(renderer, platform));
	CHECK_EQ(app.runtime().nodeCount(), 23u);
	CHECK_EQ(app.run({ 0.025, true }), 0);
	CHECK(app.lastRunStats().frames > 0);
	CHECK(app.lastRunStats().framesPerSecond > 0);
	Event close{}; close.type = EventType::CloseRequest;
	platform.pushEvent(close);
	CHECK_EQ(app.run(), 0);
	app.shutdown();
	CHECK(!app.window().valid());
}

TEST(ui_runtime_loads_structure_style_lays_out_and_routes_button_events) {
	constexpr std::string_view xuiSource = R"(
Window(title: "Runtime test", size: 320x180) {
  Column[class: "page"] {
    Text("Hello")
    Button[class: "primary", label: "Press", align: Center, press: onSubmit]
  }
}
)";
	constexpr std::string_view xssSource = R"(
tokens {
  color.window = #111827
  color.primary = #3b82f6
  color.primary.hover = #2563eb
  color.primary.active = #1d4ed8
  space.page = 16px
}
Window { resting: { bg: color.window } }
Column.page { resting: { pad: space.page, gap: 8px } }
Button.primary {
  resting: { bg: color.primary, fg: #ffffff, size: 200px 48px, radius: 8px }
  hover: { bg: color.primary.hover }
  pressed: { bg: color.primary.active }
}
)";

	StringInterner strings;
	auto document = parseXui(xuiSource, "runtime-test.xui", strings);
	CHECK(document.has_value());
	auto styles = compileXss(xssSource, "runtime-test.xss", strings);
	CHECK(styles.has_value());
	if (!document || !styles) return;

	SoftRenderer renderer;
	CHECK(renderer.init({ 320, 180, 1.f, nullptr, false }));
	TextSystem text;
	CHECK(text.initAtlas(&renderer));
	const std::string fontBytes = readTestFont();
	const FontHandle font = fontBytes.empty() ? FontHandle{} : text.loadFont(fontBytes);
	UiRuntime ui;
	std::string error;
	CHECK(ui.build(*document, *styles, strings, text, font, error));
	CHECK_EQ(ui.windowTitle(), std::string("Runtime test"));
	CHECK_EQ(ui.windowWidth(), 320u);
	CHECK_EQ(ui.windowHeight(), 180u);
	CHECK_EQ(ui.nodeCount(), 4u);
	if (!error.empty() || !ui.root().valid()) return;

	ui.layout(320, 180);
	const NodeId button{ 3, 1 }; // Window, Column, Text, Button in document order.
	const Rect buttonRect = ui.bounds(button);
	CHECK_EQ(buttonRect.x, 60.f); // centered in the Column's 288px content box
	CHECK_EQ(buttonRect.w, 200.f);
	CHECK_EQ(buttonRect.h, 48.f);

	Painter painter;
	painter.begin(320, 180, 1.f);
	text.beginFrame();
	ui.paint(painter);
	const DrawList rendered = painter.finish();
	renderer.beginFrame({ 320, 180, 1.f, Color{0, 0, 0, 0}, 1 });
	renderer.submit(rendered);
	renderer.endFrame();
	const Image image = renderer.snapshot();
	const u32 px = static_cast<u32>(buttonRect.x + 10.f);
	const u32 py = static_cast<u32>(buttonRect.y + 10.f);
	const u8* center = image.px(px, py);
	CHECK_EQ(center[0], 0x3Bu);
	CHECK_EQ(center[1], 0x82u);
	CHECK_EQ(center[2], 0xF6u);

	// On systems with a TTF available, also verify that the .xui label reaches the glyph atlas
	// and produces visible foreground pixels in the software-rendered button.
	if (font.valid()) {
		u32 brightPixels = 0;
		for (u32 y = static_cast<u32>(buttonRect.y); y < static_cast<u32>(buttonRect.bottom()); ++y)
			for (u32 x = static_cast<u32>(buttonRect.x); x < static_cast<u32>(buttonRect.right()); ++x) {
				const u8* pixel = image.px(x, y);
				if (pixel[0] > 220 && pixel[1] > 220 && pixel[2] > 220) ++brightPixels;
			}
		CHECK(brightPixels > 0);
	}

	u32 presses = 0;
	ui.setEventHandler(recordPress, &presses);
	const Vec2 point{ buttonRect.x + buttonRect.w * 0.5f, buttonRect.y + buttonRect.h * 0.5f };
	CHECK(ui.pointerMove(point));
	CHECK(ui.pointerDown(point));
	CHECK(ui.pointerUp(point));
	CHECK_EQ(presses, 1u);
	text.atlas()->shutdown();
	renderer.shutdown();
}

TEST(ui_runtime_resolves_stylesheet_percent_lengths_once) {
	constexpr std::string_view xuiSource = R"(
Window(title: "Percent", size: 400x200) {
  Column[class: "sized"] {
    Button(label: "Clamped")[class: "full", width: 80%, minWidth: 50%, height: 40px]
  }
}
)";
	constexpr std::string_view xssSource = R"(
Column.sized { resting: { size: 240px 120px, pad: 10px, border: 2px solid #334455 } }
Button.full { resting: { max-width: 60% } }
)";
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error));
	if (!error.empty()) return;
	TextSystem text; UiRuntime ui;
	CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error));
	if (!error.empty()) return;
	ui.layout(400, 200);
	const Rect button = ui.bounds({ 2, 1 });
	CHECK_NEAR(button.w, 129.6f, 0.01f); // XUI 80% width, min 50%, XSS max 60% of content width (216)
	CHECK_NEAR(button.h, 40.f, 0.01f);
}

TEST(ui_runtime_wraps_and_clips_text_to_its_styled_widget_bounds) {
	constexpr std::string_view xuiSource = R"(
Window(size: 160x60) { Column { Text("A long line of text that must wrap and remain in its small box")[class: "clipped"] } }
)";
	constexpr std::string_view xssSource = R"(
Text.clipped { resting: { size: 48px 20px, font-size: 13px, fg: #ffffff, pad: 0px } }
)";
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error));
	if (!error.empty()) return;
	const std::string fontBytes = readTestFont();
	if (fontBytes.empty()) return;
	SoftRenderer renderer; CHECK(renderer.init({ 160, 60, 1.f, nullptr, false }));
	TextSystem text; CHECK(text.initAtlas(&renderer));
	const FontHandle font = text.loadFont(fontBytes, &error); CHECK(font.valid());
	UiRuntime ui; CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error));
	CHECK(ui.setFont(font, error)); ui.layout(160, 60);
	const Rect bounds = ui.bounds({ 2, 1 });
	CHECK_EQ(bounds.w, 48.f); CHECK_EQ(bounds.h, 20.f);
	Painter painter; painter.begin(160, 60, 1.f); text.beginFrame(); ui.paint(painter);
	const DrawList list = painter.finish();
	bool foundGlyphs = false;
	for (u32 i = 0; i < list.nCmds; ++i) if (list.cmds[i].tex.valid()) {
		foundGlyphs = true;
		CHECK(list.cmds[i].clip.w <= 48);
		CHECK(list.cmds[i].clip.h <= 20);
	}
	CHECK(foundGlyphs);
	text.atlas()->shutdown(); renderer.shutdown();
}

TEST(ui_runtime_row_keeps_text_ink_inside_intrinsic_child_bounds) {
	constexpr std::string_view xuiSource = R"(
Window(title: "Xui Button", size: 760x480) {
    Column(gap: 10)[class: "page"] { Text("Hello My Little Pony") }
    Row(gap: 10)[class: "bar"] { Text("World! Extension") }
    Row(gap: 10) { Text("!") }
    Row(gap: 10) { Text("World!") }
    Row[width: 140px] { Text("Long text wraps inside a narrow row without losing its final glyph.")[width: 120px] }
    Row(gap: 10) { Button(label: "Non-text child") }
}
)";
	constexpr std::string_view xssSource = R"(
Text { resting: { font-size: 16px } }
Row.bar { resting: { bg: #cbd5e1 } }
)";
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error));
	const std::string fontBytes = readTestFont();
	if (fontBytes.empty()) return;
	SoftRenderer renderer; CHECK(renderer.init({ 760, 480, 1.f, nullptr, false }));
	TextSystem text; CHECK(text.initAtlas(&renderer));
	const FontHandle font = text.loadFont(fontBytes, &error); CHECK(font.valid());
	UiRuntime ui; CHECK(ui.build(assets.document, assets.styles, strings, text, font, error));
	for (const f32 dpi : { 1.f, 1.25f, 1.5f }) {
		ui.layout(760.f, 480.f, dpi);
		CHECK(ui.bounds({ 2, 1 }).w > 700.f); // Column still stretches text on its cross axis.
		for (const auto [index, sample] : { std::pair{ 4u, std::string_view("World! Extension") },
			std::pair{ 6u, std::string_view("!") },
			std::pair{ 8u, std::string_view("World!") } }) {
			const Rect rect = ui.bounds({ index, 1 });
			const TextLayout& measured = text.layout(font, sample, 16.f * dpi);
			CHECK_EQ(measured.glyphs.size(), static_cast<std::size_t>(std::count_if(sample.begin(), sample.end(), [](char ch) { return ch != ' '; })));
			CHECK_NEAR(rect.w, measured.width / dpi, 0.01f);
		}
	}
	const Rect wrapped = ui.bounds({ 10, 1 });
	CHECK_NEAR(wrapped.w, 120.f, 0.01f);
	CHECK(wrapped.h > text.layout(font, "Long text wraps inside a narrow row without losing its final glyph.", 24.f, 180.f).lineHeight / 1.5f);
	CHECK(ui.bounds({ 12, 1 }).w >= 96.f); // A button remains content-sized when hosted by Row.
	Painter painter; painter.begin(1140, 720, 1.5f); text.beginFrame(); ui.paint(painter, 1.5f);
	const DrawList list = painter.finish();
	for (const auto [index, expectedGlyphs] : { std::pair{ 4u, 15u }, std::pair{ 6u, 1u }, std::pair{ 8u, 6u } }) {
		const Rect rect = ui.bounds({ index, 1 });
		bool matchedClip = false;
		f32 rightmostGlyph = -std::numeric_limits<f32>::infinity();
		for (u32 ci = 0; ci < list.nCmds; ++ci) {
			const DrawCmd& command = list.cmds[ci];
			if (!command.tex.valid() || command.clip.y != static_cast<i32>(std::floor(rect.y * 1.5f))) continue;
			if (command.clip.x <= static_cast<i32>(std::floor(rect.x * 1.5f)) &&
				command.clip.x + command.clip.w >= static_cast<i32>(std::ceil(rect.right() * 1.5f))) {
				matchedClip = true;
				CHECK_EQ(command.idxCount / 6, expectedGlyphs);
				for (u32 ii = command.idxOffset; ii < command.idxOffset + command.idxCount; ++ii)
					rightmostGlyph = std::max(rightmostGlyph, list.verts[list.idx[ii]].x);
			}
		}
		CHECK(matchedClip);
		if (index == 4u) {
			const f32 firstWordRight = rect.x * 1.5f + text.layout(font, "World!", 24.f).width;
			CHECK(rightmostGlyph > firstWordRight + 4.f); // the Extension glyphs must be in the actual draw geometry
		}
	}
	text.atlas()->shutdown(); renderer.shutdown();
}

TEST(ui_runtime_row_keeps_two_long_text_children_on_one_visible_line) {
	constexpr std::string_view xuiSource = R"(
Window(size: 760x480) {
    Row[class: "bar"] {
        Text("World! Extension")[id: "first"]
        Text("World! Extension")[id: "second"]
    }
}
)";
	constexpr std::string_view xssSource = R"(
Text { resting: { font-size: 16px } }
Row.bar { resting: { bg: #cbd5e1, fg: #111827 } }
)";
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error));
	const std::string fontBytes = readTestFont();
	if (fontBytes.empty()) return;
	SoftRenderer renderer; CHECK(renderer.init({ 760, 480, 1.f, nullptr, false }));
	TextSystem text; CHECK(text.initAtlas(&renderer));
	const FontHandle font = text.loadFont(fontBytes, &error); CHECK(font.valid());
	UiRuntime ui; CHECK(ui.build(assets.document, assets.styles, strings, text, font, error));
	ui.layout(760.f, 480.f, 1.f);
	CHECK_EQ(ui.controlText("first"), "World! Extension");
	CHECK_EQ(ui.controlText("second"), "World! Extension");
	const Rect first = ui.bounds({ 2, 1 }), second = ui.bounds({ 3, 1 });
	const TextLayout& natural = text.layout(font, "World! Extension", 16.f);
	CHECK(first.w >= natural.width);
	CHECK(second.w >= natural.width);
	CHECK(second.x >= first.right());
	CHECK_EQ(text.layout(font, "World! Extension", 16.f, first.w).lineStarts.size(), std::size_t(1));
	CHECK_EQ(text.layout(font, "World! Extension", 16.f, second.w).lineStarts.size(), std::size_t(1));
	Painter painter; painter.begin(760, 480, 1.f); text.beginFrame(); ui.paint(painter);
	const DrawList list = painter.finish();
	for (const Rect rect : { first, second }) {
		bool found = false;
		for (u32 ci = 0; ci < list.nCmds; ++ci) {
			const DrawCmd& command = list.cmds[ci];
			if (!command.tex.valid() || command.clip.x != static_cast<i32>(std::floor(rect.x)) ||
				command.clip.y != static_cast<i32>(std::floor(rect.y))) continue;
			found = true;
			CHECK_EQ(command.idxCount / 6, 15u);
		}
		CHECK(found);
	}
	text.atlas()->shutdown(); renderer.shutdown();
}

TEST(ui_runtime_check_item_text_respects_padding_before_clip) {
	constexpr std::string_view xuiSource = R"(
Window(size: 240x80) { CheckItem(label: "Alpha", checked: false)[class: "item"] }
)";
	constexpr std::string_view xssSource = R"(
CheckItem.item { resting: { size: 180px 36px, pad: 6px, font-size: 15px, fg: #ffffff } }
)";
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error));
	if (!error.empty()) return;
	const std::string fontBytes = readTestFont();
	if (fontBytes.empty()) return;
	SoftRenderer renderer; CHECK(renderer.init({ 240, 80, 1.f, nullptr, false }));
	TextSystem text; CHECK(text.initAtlas(&renderer));
	const FontHandle font = text.loadFont(fontBytes, &error); CHECK(font.valid());
	UiRuntime ui; CHECK(ui.build(assets.document, assets.styles, strings, text, font, error));
	ui.layout(240, 80);
	const Rect bounds = ui.bounds({ 1, 1 });
	Painter painter; painter.begin(240, 80, 1.f); text.beginFrame(); ui.paint(painter);
	const DrawList list = painter.finish();
	bool foundGlyphs = false;
	for (u32 commandIndex = 0; commandIndex < list.nCmds; ++commandIndex) {
		const DrawCmd& command = list.cmds[commandIndex];
		if (!command.tex.valid()) continue;
		foundGlyphs = true;
		f32 firstX = std::numeric_limits<f32>::infinity();
		for (u32 i = command.idxOffset; i < command.idxOffset + command.idxCount; ++i)
			firstX = std::min(firstX, list.verts[list.idx[i]].x);
		// The first glyph quad must begin inside the padded text clip. Previously its origin
		// omitted the left padding and the renderer clipped the first character of each label.
		// The outline font can place a one-pixel negative bearing before the text origin.
		CHECK(firstX >= (bounds.x + 6.f + 26.f) - 1.01f);
	}
	CHECK(foundGlyphs);
	text.atlas()->shutdown(); renderer.shutdown();
}

TEST(ui_runtime_dropdown_and_color_picker_use_shared_popup_interaction) {
	constexpr std::string_view xuiSource = R"(
Window(size: 400x400) {
  Column {
    Dropdown[id: "density", options: "Compact|Comfortable|Spacious", value: "Compact", change: onDensity, class: "control"]
    ColorPicker[id: "accent", value: #336699, format: "hex", formats: "hex|rgb|hsl|hsv", precision: 2, alpha: true, change: onAccent, class: "control"]
  }
}
)";
	constexpr std::string_view xssSource = R"(
Dropdown.control { resting: { size: 220px 36px, bg: #223344, fg: #ffffff } hover: { bg: #334455 } open: { border: 1px solid #ffffff } }
ColorPicker.control { resting: { size: 220px 36px, bg: #223344, fg: #ffffff } hover: { bg: #334455 } open: { border: 1px solid #ffffff } }
)";
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error));
	TextSystem text; UiRuntime ui;
	CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error));
	ui.layout(400, 400);
	std::vector<std::string> events;
	ui.setEventHandler([](void* context, std::string_view name) { static_cast<std::vector<std::string>*>(context)->emplace_back(name); }, &events);
	const Rect dropdown = ui.bounds({ 2, 1 });
	CHECK(ui.controlText("density") == "Compact");
	CHECK(ui.pointerDown({ dropdown.x + 8, dropdown.y + 8 }));
	ui.pointerUp({ dropdown.x + 8, dropdown.y + 8 });
	CHECK(ui.pointerDown({ dropdown.x + 8, dropdown.y + dropdown.h + 38 }));
	CHECK(ui.controlText("density") == "Comfortable");
	CHECK(!events.empty() && events.back() == "onDensity");
	ui.pointerUp({ dropdown.x + 8, dropdown.y + dropdown.h + 38 });

	const Color before = ui.controlColor("accent");
	CHECK(ui.controlText("accent") == "#336699FF");
	CHECK(ui.setValue("accent", std::string("rgba(255, 0, 0, 0.5)")));
	CHECK(ui.controlColor("accent").r > 0.99f && ui.controlColor("accent").g < 0.01f);
	CHECK(ui.controlColor("accent").a > 0.49f && ui.controlColor("accent").a < 0.51f);
	CHECK(ui.setValue("accent", std::string("hsl(120, 100%, 50%)")));
	CHECK(ui.controlColor("accent").g > 0.99f && ui.controlColor("accent").r < 0.01f);
	const Color validColor = ui.controlColor("accent");
	CHECK(!ui.setValue("accent", std::string("hsl(not a color)")));
	CHECK(ui.controlColor("accent") == validColor);
	const Rect picker = ui.bounds({ 3, 1 });
	CHECK(ui.pointerDown({ picker.x + 8, picker.y + 8 }));
	ui.pointerUp({ picker.x + 8, picker.y + 8 });
	const f32 popupY = picker.y + picker.h + 352.f <= 400.f ? picker.y + picker.h : std::max(0.f, picker.y - 352.f);
	CHECK(ui.pointerDown({ picker.x + 150.f, popupY + 90.f }));
	CHECK(ui.controlColor("accent") != before);
	CHECK(events.back() == "onAccent");
	const Color clickedColor = ui.controlColor("accent");
	CHECK(ui.pointerMove({ picker.x + 220.f, popupY + 110.f }));
	CHECK(ui.controlColor("accent") != clickedColor);
	ui.pointerUp({ picker.x + 220.f, popupY + 110.f });
	CHECK(ui.pointerDown({ picker.x + 90.f,popupY + 283.f }));
	CHECK(ui.keyDown(Key::A, static_cast<u8>(Mod::Ctrl), nullptr));
	CHECK(ui.textInput("#ff00ff80"));
	CHECK(ui.controlColor("accent").r > 0.99f && ui.controlColor("accent").b > 0.99f);
	CHECK(ui.controlColor("accent").a > 0.49f && ui.controlColor("accent").a < 0.51f);
	CHECK(ui.pointerDown({ picker.x + 90.f,popupY + 228.f })); // HEX -> RGB
	ui.pointerUp({ picker.x + 90.f,popupY + 228.f });
	CHECK(ui.controlText("accent").starts_with("rgba("));
	CHECK(ui.pointerDown({ picker.x + 30.f,popupY + 283.f })); // R channel field
	CHECK(ui.keyDown(Key::A, static_cast<u8>(Mod::Ctrl), nullptr));
	CHECK(ui.textInput("0"));
	CHECK(ui.controlColor("accent").r < 0.01f);
	const Color channelEdited = ui.controlColor("accent");
	for (int i = 0; i < 3; ++i) {
		CHECK(ui.pointerDown({ picker.x + 90.f,popupY + 228.f }));
		ui.pointerUp({ picker.x + 90.f,popupY + 228.f });
		CHECK(ui.controlColor("accent") == channelEdited);
	}
	CHECK(ui.controlText("accent").starts_with("#"));
	CHECK(ui.pointerDown({ picker.x + 90.f,popupY + 283.f })); // HEX field is active again
	CHECK(ui.keyDown(Key::A, static_cast<u8>(Mod::Ctrl), nullptr));
	CHECK(ui.textInput("#ff00ff80"));
	CHECK(ui.controlColor("accent").r > 0.99f && ui.controlColor("accent").b > 0.99f);
	CHECK(ui.pointerDown({ picker.x + 200.f,popupY + 225.f }));
	CHECK(ui.controlText("accent").starts_with("rgb"));
	CHECK(std::get<Color>(*ui.value("accent")) == ui.controlColor("accent"));
	ui.pointerUp({ picker.x + 10.f,popupY + 170.f });
}

TEST(ui_runtime_slider_change_fires_for_each_live_drag_value) {
	constexpr std::string_view xuiSource = R"(
Window(size: 400x160) { Column { Slider[id: "amount", value: 0.25, min: 0, max: 1, change: onChange] } }
)";
	constexpr std::string_view xssSource = R"(Slider { resting: { size: 240px 24px } })";
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error));
	TextSystem text; UiRuntime ui; CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error)); ui.layout(400, 160);
	std::vector<std::string> events;
	ui.setEventHandler([](void* context, std::string_view event) { static_cast<std::vector<std::string>*>(context)->emplace_back(event); }, &events);
	const Rect slider = ui.bounds({ 2,1 });
	CHECK(ui.pointerDown({ slider.x + 24.f,slider.y + slider.h * 0.5f }));
	CHECK(events.size() == 1 && events.back() == "onChange");
	const f64 start = std::get<f64>(*ui.value("amount"));
	CHECK(ui.pointerMove({ slider.x + 190.f,slider.y + slider.h * 0.5f }));
	CHECK(events.size() == 2 && events.back() == "onChange");
	const f64 moved = std::get<f64>(*ui.value("amount"));
	CHECK(moved > start);
	ui.pointerUp({ slider.x + 190.f,slider.y + slider.h * 0.5f });
	CHECK(events.size() == 2); // release does not duplicate the last live change
}

TEST(ui_runtime_text_fields_check_box_tabs_and_delayed_tooltips) {
	constexpr std::string_view xuiSource = R"(
Window(title: "Inputs", size: 480x360) {
  Column {
    TextBox[id: "name", value: "A", placeholder: "Name", maxLength: 6, tooltip: "Type a short name", tooltipShowDelay: 100, tooltipHideDelay: 200, tooltipTrigger: "hover", change: onChange]
  TextArea[id: "notes", value: "", placeholder: "Notes", maxLength: 20, tabSize: 2, tooltip: "Use multiple lines", tooltipDisabled: true, change: onChange]
    CheckBox(label: "Enabled")[id: "enabled", checked: true, change: onChange]
    Tabs[id: "pages", selected: 0, change: onChange] {
      Tab(title: "First") { Button(label: "First page action")[press: onFirst, width: 180px, height: 34px] }
      Tab(title: "Second") { Button(label: "Second page action")[press: onSecond, width: 180px, height: 34px] }
    }
  }
}
)";
	constexpr std::string_view xssSource = R"(
TextBox { resting: { size: 180px 34px, pad: 6px } }
TextArea { resting: { size: 220px 90px, pad: 6px } }
CheckBox { resting: { size: 180px 32px, pad: 4px } }
Tabs { resting: { size: 260px 170px } }
Tooltip { resting: { bg: #202a40, fg: #ffffff, pad: 6px, radius: 4px } }
)";
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error));
	if (!error.empty()) return;
	TextSystem text; UiRuntime ui;
	CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error));
	if (!error.empty()) return;
	ui.layout(480, 360);
	CHECK_EQ(ui.nodeCount(), 10u);
	CHECK(std::get<std::string>(*ui.value("name")) == "A");
	CHECK(std::get<bool>(*ui.value("enabled")));
	CHECK_EQ(std::get<u32>(*ui.value("pages")), 0u);

	std::vector<std::string> events;
	ui.setEventHandler([](void* context, std::string_view name) { static_cast<std::vector<std::string>*>(context)->emplace_back(name); }, &events);
	CHECK(ui.setValue("name", std::string("XYZZ")));
	CHECK(std::get<std::string>(*ui.value("name")) == "XYZZ");
	CHECK(!ui.setValue("name", true));
	CHECK(ui.setValue("name", std::string("A")));
	const Rect name = ui.bounds({ 2, 1 });
	CHECK(ui.pointerMove({ name.x + 10, name.y + 10 }));
	const auto now = std::chrono::steady_clock::now();
	CHECK(ui.nextTimerDelay(now) > 0.09);
	CHECK(ui.updateTimers(now + std::chrono::milliseconds(110)));
	CHECK_EQ(ui.nextTimerDelay(now + std::chrono::seconds(1)), -1.0);
	ui.pointerMove({ 470.f, 350.f });
	const auto hideCheck = std::chrono::steady_clock::now();
	CHECK(ui.nextTimerDelay(hideCheck) > 0.19 && ui.nextTimerDelay(hideCheck) <= 0.2);
	CHECK(ui.updateTimers(hideCheck + std::chrono::milliseconds(250)));
	CHECK_EQ(ui.nextTimerDelay(hideCheck + std::chrono::milliseconds(250)), -1.0);
	CHECK(ui.pointerMove({ name.x + 10, name.y + 10 }));
	const Vec2 namePoint{ name.x + 10, name.y + 10 };
	CHECK(ui.pointerDown(namePoint));
	CHECK(ui.textInput('B'));
	CHECK(std::get<std::string>(*ui.value("name")) == "AB");
	CHECK(ui.keyDown(Key::Backspace));
	CHECK(std::get<std::string>(*ui.value("name")) == "A");
	CHECK(ui.textInput("😀"));
	CHECK(std::get<std::string>(*ui.value("name")) == "A😀");
	CHECK(ui.textInput('C'));
	CHECK(ui.textInput('D'));
	CHECK_EQ(std::get<std::string>(*ui.value("name")), "A😀CD");
	CHECK(ui.keyDown(Key::Home));
	CHECK(ui.keyDown(Key::Delete));
	CHECK_EQ(std::get<std::string>(*ui.value("name")), "😀CD");
	CHECK(ui.keyDown(Key::End, static_cast<u8>(Mod::Shift), nullptr));
	CHECK(ui.keyDown(Key::Backspace));
	CHECK_EQ(std::get<std::string>(*ui.value("name")), "");
	CHECK(ui.textInput("replace"));
	CHECK_EQ(std::get<std::string>(*ui.value("name")), "replac");
	CHECK(ui.keyDown(Key::A, static_cast<u8>(Mod::Ctrl), nullptr));
	CHECK(ui.textInput('X'));
	CHECK_EQ(std::get<std::string>(*ui.value("name")), "X");
	// The focused field keeps its caret blink scheduled; it must be the only pending timer.
	CHECK(ui.nextTimerDelay() >= 0.0);
	NullPlatform clipboard;
	CHECK(ui.setValue("name", std::string("hello")));
	CHECK(ui.keyDown(Key::Home));
	CHECK(ui.keyDown(Key::End, static_cast<u8>(Mod::Shift), nullptr));
	CHECK(ui.keyDown(static_cast<Key>('C'), static_cast<u8>(Mod::Ctrl), &clipboard));
	CHECK_EQ(clipboard.clipboardText(), "hello");
	CHECK(ui.setValue("name", std::string("")));
	CHECK(ui.keyDown(static_cast<Key>('V'), static_cast<u8>(Mod::Ctrl), &clipboard));
	CHECK_EQ(std::get<std::string>(*ui.value("name")), "hello");
	CHECK(!events.empty());

	const Rect check = ui.bounds({ 4, 1 });
	const Vec2 checkPoint{ check.x + 40, check.y + check.h * 0.5f };
	CHECK(ui.pointerDown(checkPoint)); CHECK(ui.pointerUp(checkPoint));
	CHECK(!std::get<bool>(*ui.value("enabled")));

	const Rect notes = ui.bounds({ 3, 1 });
	ui.pointerMove({ notes.x + 12, notes.y + 12 });
	CHECK_EQ(ui.nextTimerDelay(), -1.0);
	CHECK(ui.pointerDown({ notes.x + 12, notes.y + 12 }));
	CHECK(ui.keyDown(Key::Enter));
	CHECK(ui.textInput('N'));
	CHECK(std::get<std::string>(*ui.value("notes")) == "\nN");
	CHECK(ui.setValue("notes", std::string("first\nsecond")));
	CHECK(ui.keyDown(Key::Home));
	CHECK(ui.keyDown(Key::Backspace));
	CHECK_EQ(std::get<std::string>(*ui.value("notes")), "firstsecond");
	CHECK(ui.keyDown(Key::Enter));
	CHECK_EQ(std::get<std::string>(*ui.value("notes")), "first\nsecond");
	CHECK(ui.setValue("notes", std::string("a")));
	CHECK(ui.keyDown(Key::Home));
	CHECK(ui.keyDown(Key::Tab));
	CHECK_EQ(std::get<std::string>(*ui.value("notes")), "  a");
	CHECK(ui.keyDown(Key::Backspace));
	CHECK_EQ(std::get<std::string>(*ui.value("notes")), " a");
	CHECK(!ui.textInput('\b'));
	CHECK_EQ(std::get<std::string>(*ui.value("notes")), " a");

	const Rect tabs = ui.bounds({ 5, 1 });
	CHECK(ui.pointerDown({ tabs.x + tabs.w * 0.75f, tabs.y + 12.f }));
	CHECK_EQ(std::get<u32>(*ui.value("pages")), 1u);
	CHECK(ui.keyDown(Key::Left));
	CHECK_EQ(std::get<u32>(*ui.value("pages")), 0u);
	CHECK(ui.setValue("pages", u32{ 1 }));
	CHECK_EQ(std::get<u32>(*ui.value("pages")), 1u);
	events.clear();
	const Rect inactivePageButton = ui.bounds({ 7, 1 });
	const Vec2 hiddenPagePoint{ inactivePageButton.x + 20.f, inactivePageButton.y + inactivePageButton.h * 0.5f };
	CHECK(ui.pointerDown(hiddenPagePoint)); CHECK(ui.pointerUp(hiddenPagePoint));
	CHECK_EQ(events.size(), 1u);
	CHECK(events.back() == "onSecond");
}

TEST(controls_example_assets_parse_validate_and_build) {
	const std::string xuiSource = readFixture("controls.xui");
	const std::string xssSource = readFixture("controls.xss");
	CHECK(!xuiSource.empty()); CHECK(!xssSource.empty());
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error, "controls.xui", "controls.xss"));
	if (!error.empty()) return;
	TextSystem text; UiRuntime ui;
	CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error));
	if (!error.empty()) return;
	ui.layout(920, 640);
	CHECK(ui.value("username").has_value());
	CHECK(ui.value("notes").has_value());
	CHECK(ui.value("updates").has_value());
	CHECK(ui.value("sections").has_value());
	CHECK(ui.bounds(ui.root()).w > 0.f);
}

TEST(showcase_example_assets_parse_validate_and_build) {
	const std::string xuiSource = readExample("showcase.xui");
	const std::string xssSource = readExample("showcase.xss");
	CHECK(!xuiSource.empty()); CHECK(!xssSource.empty());
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error, "showcase.xui", "showcase.xss"));
	if (!error.empty()) { std::cerr << error << '\n'; return; }
	const std::string_view showcasedTypes[] = { "Text", "Label", "Button", "TextBox", "TextArea", "CheckBox", "CheckItem",
		"RadioGroup", "Switch", "ToggleButton", "Slider", "Dropdown", "ColorPicker", "Tabs", "Tab", "ScrollView",
		"ProgressBar", "Panel", "Split", "CheckList", "FpsCounter", "Image", "Spinner", "Separator", "Divider",
		"Toolbar", "StatusBar", "NumberField", "SearchField", "DatePicker", "SegmentedControl", "Stepper", "Rating",
		"Breadcrumb", "KeybindField" };
	for (const std::string_view type : showcasedTypes) {
		bool found = false;
		for (const XNode& node : assets.document.nodes) found |= strings.view(node.type) == type;
		CHECK(found);
	}
	TextSystem text; UiRuntime ui;
	CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error));
	if (!error.empty()) { std::cerr << error << '\n'; return; }
	ui.layout(1080, 760);
	CHECK(ui.value("display-name").has_value());
	CHECK(ui.value("passphrase").has_value());
	CHECK(ui.value("readonly-field").has_value());
	CHECK(ui.value("notes").has_value());
	CHECK(ui.value("updates").has_value());
	CHECK(ui.value("disabled-check").has_value());
	CHECK(ui.value("volume").has_value());
	CHECK(ui.value("density").has_value());
	CHECK(ui.value("accent-color").has_value());
	CHECK(ui.value("demo-sections").has_value());
	CHECK(ui.value("task-layout").has_value());
	CHECK(ui.value("task-resize").has_value());
	CHECK(ui.value("query").has_value());
	CHECK(ui.value("delay").has_value());
	CHECK(ui.value("whitespace-toggle").has_value());
	CHECK(ui.value("live-sync").has_value());
	CHECK(ui.value("mute").has_value());
	CHECK(ui.value("density-choice").has_value());
	CHECK(ui.value("shading").has_value());
	CHECK(ui.value("priority").has_value());
	CHECK(ui.value("subdivisions").has_value());
	CHECK(ui.value("due-date").has_value());
	CHECK(ui.value("save-key").has_value());
	CHECK(ui.value("trail").has_value());
	CHECK(ui.value("busy-bar").has_value());
	CHECK(ui.bounds(ui.root()).w > 0.f);

	std::unordered_map<std::string, u32> runtimeIndexById;
	u32 nextRuntimeIndex = 0;
	auto mapAuthoredIds = [&](auto&& self, u32 sourceIndex) -> void {
		const u32 runtimeIndex = nextRuntimeIndex++;
		const XNode& source = assets.document.nodes[sourceIndex];
		for (const XProp& prop : assets.document.propsOf(source)) {
			if (strings.view(prop.name) != "id" || prop.expr >= assets.document.exprs.size()) continue;
			const XExpr& expression = assets.document.exprs[prop.expr];
			if (expression.kind == XExprKind::String) runtimeIndexById.emplace(std::string(strings.view(expression.name)), runtimeIndex);
		}
		for (u32 child : assets.document.childrenOf(source)) self(self, child);
		};
	mapAuthoredIds(mapAuthoredIds, assets.document.roots.front());
	auto rectFor = [&](std::string_view id) {
		const auto found = runtimeIndexById.find(std::string(id));
		if (found == runtimeIndexById.end()) return Rect{};
		return ui.bounds({ found->second, 1 });
		};
	auto separatedVertically = [](Rect a, Rect b) { return a.empty() || b.empty() || a.bottom() <= b.y + 0.01f; };

	const Rect name = rectFor("display-name"), passphrase = rectFor("passphrase"), readOnly = rectFor("readonly-field");
	const Rect inputRow = rectFor("input-row"), notes = rectFor("notes");
	CHECK(name.w > 0.f && name.h > 0.f); CHECK(readOnly.w > 0.f && readOnly.h > 0.f);
	CHECK(name.right() <= passphrase.x + 0.01f); CHECK(passphrase.right() <= readOnly.x + 0.01f);
	CHECK(separatedVertically(inputRow, notes));

	// Each authored tab must produce drawable content, not just parse successfully.
	Painter painter;
	for (u32 section = 0; section < 6; ++section) {
		CHECK(ui.setValue("demo-sections", section));
		ui.layout(1080, 760);
		if (section == 0) {
			// Layout tab: both authored XUI and XSS floors hold, and sibling sizing stays ordered.
			const Rect fixedBox = rectFor("sizing-fixed"), pct = rectFor("sizing-pct"), minBox = rectFor("sizing-min");
			CHECK(fixedBox.w > 0.f && pct.w > 0.f && minBox.w > 0.f);
			CHECK(fixedBox.right() <= pct.x + 0.01f); CHECK(pct.right() <= minBox.x + 0.01f);
			CHECK(minBox.w >= 220.f - 0.01f && minBox.h >= 64.f - 0.01f);
			CHECK(rectFor("style-floor").w >= 200.f - 0.01f);
			CHECK(rectFor("layout-split").w >= 420.f - 0.01f);
			CHECK(rectFor("nested-left").w >= 180.f - 0.01f && rectFor("nested-left").h >= 90.f - 0.01f);
			const Rect positionBox = rectFor("position-box"), positionLabel = rectFor("position-label"), positionTag = rectFor("position-tag");
			CHECK(!positionBox.empty() && !positionLabel.empty() && !positionTag.empty());
			CHECK(positionTag.x >= positionBox.x && positionTag.right() <= positionBox.right() + 0.01f);
			CHECK(positionTag.y >= positionBox.y && positionTag.bottom() <= positionBox.bottom() + 0.01f);
			CHECK(positionLabel.right() <= positionTag.x);
			const Rect verticalSplit = rectFor("vertical-split"), verticalTop = rectFor("vertical-top"), verticalBottom = rectFor("vertical-bottom");
			CHECK(verticalSplit.h >= 132.f - 0.01f && verticalTop.h >= 56.f - 0.01f && verticalBottom.h >= 56.f - 0.01f);
			CHECK(verticalTop.bottom() <= verticalBottom.y + 0.01f);
			ui.layout(400, 300);
			CHECK(rectFor("sizing-min").w >= 220.f - 0.01f);
			CHECK(rectFor("style-floor").w >= 200.f - 0.01f);
			CHECK(rectFor("layout-split").w >= 420.f - 0.01f);
			CHECK(rectFor("nested-left").w >= 180.f - 0.01f);
		}
		else if (section == 2) {
			const Rect volume = rectFor("volume"), progress = rectFor("progress-demo"), busy = rectFor("busy-bar");
			CHECK(volume.w > 0.f && progress.w > 0.f && busy.w > 0.f);
			CHECK(separatedVertically(volume, progress)); CHECK(separatedVertically(progress, busy));
			const Rect radio = rectFor("density-choice");
			CHECK(radio.w > 0.f && radio.h > 0.f);
		}
		else if (section == 3) {
			const Rect density = rectFor("density"), color = rectFor("accent-color"), trail = rectFor("trail");
			CHECK(density.w > 0.f && color.w > 0.f && trail.w > 0.f);
		}
		else if (section == 4) {
			const Rect image = rectFor("demo-image"), toolbar = rectFor("demo-toolbar"), status = rectFor("demo-status");
			CHECK(image.w > 0.f && toolbar.w > 0.f && status.w > 0.f);
		}
		else if (section == 5) {
			const Rect split = rectFor("main-split"), tasks = rectFor("tasks-panel"), content = rectFor("content-panel");
			const Rect firstTask = rectFor("task-layout"), lastTask = rectFor("task-resize");
			const Rect scroll = rectFor("vertical-scroll");
			CHECK(split.w > 0.f && split.h > 300.f);
			CHECK(tasks.w > 0.f && content.w > 0.f && tasks.right() < content.x);
			CHECK(firstTask.w > 0.f && lastTask.h > 0.f);
			CHECK(rectFor("task-styles").y > firstTask.y);
			CHECK(rectFor("task-backend").y > rectFor("task-styles").y);
			CHECK(lastTask.y > rectFor("task-backend").y);
			CHECK(scroll.w > 0.f && scroll.h > 0.f);
		}
		text.beginFrame();
		painter.begin(1080, 760, 1.f);
		ui.paint(painter);
		const DrawList draw = painter.finish();
		CHECK(draw.nVerts > 0);
	}

	std::vector<std::string> events;
	ui.setEventHandler([](void* context, std::string_view name) {
		static_cast<std::vector<std::string>*>(context)->emplace_back(name);
		}, &events);
	CHECK(ui.setValue("demo-sections", u32{ 1 }));   // Input tab
	ui.layout(1080, 760);
	const Rect nameField = rectFor("display-name");
	const Vec2 namePoint{ nameField.x + 20.f, nameField.y + nameField.h * 0.5f };
	CHECK(ui.pointerDown(namePoint)); CHECK(ui.pointerUp(namePoint)); CHECK(ui.textInput('!'));
	CHECK(std::get<std::string>(*ui.value("display-name")) == "Guest!");

	const Rect tabs = rectFor("demo-sections");
	const Vec2 controlsHeader{ tabs.x + tabs.w * 0.3125f, tabs.y + 12.f };   // center of the third of eight equal tabs
	CHECK(ui.pointerDown(controlsHeader)); (void)ui.pointerUp(controlsHeader);
	CHECK_EQ(std::get<u32>(*ui.value("demo-sections")), 2u);
	ui.layout(1080, 760);
	const Rect action = rectFor("run-action");
	const Vec2 actionPoint{ action.x + action.w * 0.5f, action.y + action.h * 0.5f };
	CHECK(ui.pointerDown(actionPoint)); CHECK(ui.pointerUp(actionPoint));
	CHECK(std::find(events.begin(), events.end(), "onAction") != events.end());
	const Rect updates = rectFor("updates");
	const Vec2 updatesPoint{ updates.x + 30.f, updates.y + updates.h * 0.5f };
	CHECK(ui.pointerDown(updatesPoint)); CHECK(ui.pointerUp(updatesPoint));
	CHECK(!std::get<bool>(*ui.value("updates")));

	const Vec2 pickersHeader{ tabs.x + tabs.w * 0.4375f, tabs.y + 12.f };   // center of the fourth of eight equal tabs
	CHECK(ui.pointerDown(pickersHeader)); (void)ui.pointerUp(pickersHeader);
	CHECK_EQ(std::get<u32>(*ui.value("demo-sections")), 3u);
	ui.layout(1080, 760);
	const Rect density = rectFor("density");
	const Vec2 densityPoint{ density.x + 20.f, density.y + density.h * 0.5f };
	CHECK(ui.pointerDown(densityPoint));
	const Vec2 thirdOption{ density.x + 20.f, density.y + density.h + 70.f };
	CHECK(ui.pointerDown(thirdOption));
	CHECK(std::get<std::string>(*ui.value("density")) == "Spacious");
	const Rect color = rectFor("accent-color");
	CHECK(ui.pointerDown({ color.x + 20.f, color.y + color.h * 0.5f }));
	const Color beforeColor = std::get<Color>(*ui.value("accent-color"));
	const f32 colorPopupY = color.y + color.h + 352.f <= 760.f ? color.y + color.h : std::max(0.f, color.y - 352.f);
	CHECK(ui.pointerDown({ color.x + 150.f, colorPopupY + 90.f }));
	CHECK(std::get<Color>(*ui.value("accent-color")) != beforeColor);

	const Vec2 containersHeader{ tabs.x + tabs.w * 0.6875f, tabs.y + 12.f };   // center of the sixth of eight equal tabs
	CHECK(ui.pointerDown(containersHeader)); (void)ui.pointerUp(containersHeader);
	CHECK_EQ(std::get<u32>(*ui.value("demo-sections")), 5u);
	ui.layout(1080, 760);
	const Rect task = rectFor("task-backend");
	const Vec2 taskPoint{ task.x + 50.f, task.y + task.h * 0.5f };
	CHECK(ui.pointerDown(taskPoint)); CHECK(ui.pointerUp(taskPoint));
	CHECK(std::get<bool>(*ui.value("task-backend")));
	CHECK(std::find(events.begin(), events.end(), "onTasksChange") != events.end());
	const Rect split = rectFor("main-split"), leftPaneBefore = rectFor("tasks-panel");
	const Vec2 gutterPoint{ leftPaneBefore.right() + 4.f, split.y + split.h * 0.5f };
	CHECK(ui.pointerDown(gutterPoint));
	CHECK(ui.pointerMove({ gutterPoint.x + 24.f, gutterPoint.y }));
	ui.layout(1080, 760);
	CHECK(rectFor("tasks-panel").w > leftPaneBefore.w + 10.f);
	(void)ui.pointerUp({ gutterPoint.x + 24.f, gutterPoint.y });
}

TEST(turn_based_game_example_assets_parse_validate_and_build) {
	const std::string xuiSource = readExample("game/game.xui");
	const std::string xssSource = readExample("game/game.xss");
	CHECK(!xuiSource.empty()); CHECK(!xssSource.empty());
	StringInterner strings; UiAssetBundle assets; std::string error;
	CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error, "game.xui", "game.xss"));
	if (!error.empty()) { std::cerr << error << '\n'; return; }
	TextSystem text; UiRuntime ui;
	CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error));
	if (!error.empty()) { std::cerr << error << '\n'; return; }
	ui.layout(1000, 720);
	for (const std::string_view id : { "game-pages", "player-health", "enemy-health", "difficulty",
		"strike-power", "equipment", "rune-blade", "iron-shield", "combat-hints", "battle-log" }) {
		CHECK(ui.value(id).has_value());
	}
	CHECK_EQ(std::get<u32>(*ui.value("game-pages")), 0u);
	CHECK(ui.bounds(ui.root()).w > 0.f);
}

TEST(ui_runtime_scrollview_wheel_keyboard_programmatic_and_nested_scroll) {
	constexpr std::string_view source = R"(
Window(title: "Scroll", size: 420x300) {
  ScrollView[id: "outer", vertical: true, horizontal: false, width: 220px, height: 180px, change: onChange] {
    Column {
      Text("First")[height: 40px]
      ScrollView[id: "inner", vertical: true, width: 180px, height: 70px] {
        Column {
          Text("Inner 1")[height: 45px]
          Text("Inner 2")[height: 45px]
          Text("Inner 3")[height: 45px]
        }
      }
      Text("More content")[height: 120px]
      Text("Last target")[id: "last", height: 40px]
    }
  }
}
)";
	constexpr std::string_view stylesSource = R"(
ScrollView { resting: { scroll-behavior: instant, scrollbar-visibility: always, scrollbar-size: 10px, scroll-track: #222222, scroll-thumb: #aaaaaa } hover: { scroll-thumb: #ffffff } }
)";
	StringInterner strings; auto assets = parseXui(source, "scroll.xui", strings); auto styles = compileXss(stylesSource, "scroll.xss", strings);
	CHECK(assets.has_value()); CHECK(styles.has_value()); if (!assets || !styles) return;
	Program program; program.files.push_back(*assets); CHECK(validate(program, XuiSchema::withCoreWidgets(), strings).has_value());
	TextSystem text; UiRuntime ui; std::string error; CHECK(ui.build(*assets, *styles, strings, text, {}, error)); if (!error.empty()) return;
	ui.layout(420, 300);
	u32 counts[2]{}; ui.setEventHandler(recordPress, counts);
	const Rect outer = ui.bounds({ 1, 1 }); CHECK(outer.w == 220.f); CHECK(outer.h == 180.f);
	CHECK(ui.scrollWheel({ outer.x + 20.f, outer.y + 20.f }, { 0.f, -2.f }));
	CHECK(ui.scrollOffset("outer").y > 0.f);
	CHECK(counts[1] > 0u);
	CHECK(std::get<Vec2>(*ui.value("outer")) == ui.scrollOffset("outer"));
	CHECK(ui.scrollIntoView("last", false)); CHECK(ui.scrollOffset("outer").y > 0.f);
	const u32 beforeProgrammatic = counts[1];
	CHECK(ui.setValue("outer", UiValue{ Vec2{0.f, 0.f} })); CHECK(ui.scrollOffset("outer") == Vec2{});
	CHECK_EQ(counts[1], beforeProgrammatic); // setValue is silent by default
	CHECK(ui.setValue("outer", UiValue{ Vec2{0.f, 12.f} }, true)); CHECK_EQ(counts[1], beforeProgrammatic + 1u);
	CHECK(ui.setValue("outer", UiValue{ Vec2{0.f, 0.f} }));
	CHECK(ui.keyDown(Key::Tab)); CHECK(ui.keyDown(Key::Down)); CHECK(ui.scrollOffset("outer").y > 0.f);
	CHECK(ui.setValue("outer", UiValue{ Vec2{0.f, 0.f} }));
	const Rect inner = ui.bounds({ 4, 1 });
	CHECK(ui.scrollWheel({ inner.x + 20.f, inner.y + 20.f }, { 0.f, -2.f }));
	CHECK(ui.scrollOffset("inner").y > 0.f); // deepest scrollable ancestor consumes first
	(void)ui.scrollTo("inner", { 0.f, 10000.f }, false);
	CHECK(ui.scrollWheel({ inner.x + 20.f, inner.y + 20.f }, { 0.f, -2.f }));
	CHECK(ui.scrollOffset("outer").y > 0.f); // at its limit, inner lets the outer view consume
	CHECK(ui.scrollTo("outer", { 0.f, 0.f }, false));
	CHECK(ui.pointerDown({ outer.right() - 5.f, outer.y + 15.f }));
	CHECK(ui.pointerMove({ outer.right() - 5.f, outer.y + 35.f }));
	CHECK(ui.scrollOffset("outer").y > 0.f);
	CHECK(ui.pointerUp({ outer.right() - 5.f, outer.y + 35.f }));
	CHECK(ui.scrollTo("outer", { 0.f, 10000.f }, false)); CHECK(ui.scrollOffset("outer").y < 10000.f);
}

TEST(ui_runtime_can_activate_stylesheet_themes) {
	StringInterner strings;
	auto document = parseXui("Window { Button(label: \"Theme\") }", "theme.xui", strings);
	auto styles = compileXss(R"(
	tokens { color.surface = #ffffff }
	@theme dark { tokens { color.surface = #101010 } }
	Window { resting: { bg: color.surface } }
	)", "theme.xss", strings);
	CHECK(document.has_value()); CHECK(styles.has_value());
	if (!document || !styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error));
	CHECK_EQ(ui.theme(), 0u);
	CHECK(ui.setTheme("dark")); CHECK_EQ(ui.theme(), 1u);
	CHECK(!ui.setTheme("missing"));
	Painter painter; painter.begin(100, 80, 1.f); ui.layout(100, 80); ui.paint(painter);
	const DrawList draw = painter.finish();
	CHECK(draw.nVerts >= 4u);
	if (draw.nVerts >= 4u) CHECK_EQ(draw.verts[0].c0, Color::fromRGBA8(0x101010FFu).toRGBA8());
}

TEST(ui_runtime_titlebar_regions_caption_actions_and_drag_rules) {
	constexpr std::string_view xuiSource = R"(
Window(title: "Chrome", size: 320x180)[platformTopMenu: false] {
  TitleBar[id: "bar", platformButtons: true, width: 100%, height: 36px] {
    Button(label: "Menu")[id: "bar-button", width: 70px, height: 26px, press: onMenu]
    Text("App")[id: "bar-title", grow: 1, height: 26px]
  }
  Text("Body")
}
)";
	constexpr std::string_view xssSource = R"(
TitleBar { resting: { bg: #172235 } }
TitleButton { resting: { fg: #edf3fb } hover: { bg: #293d59 } pressed: { bg: #142033 } }
)";
	StringInterner strings;
	auto document = parseXui(xuiSource, "chrome.xui", strings);
	auto styles = compileXss(xssSource, "chrome.xss", strings);
	CHECK(document.has_value()); CHECK(styles.has_value());
	if (!document || !styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error));
	if (!error.empty()) { std::printf("  build error: %s\n", error.c_str()); return; }
	CHECK(ui.hasTitleBars());

	NullPlatform platform;
	const WindowHandle window = platform.createWindow({});
	ui.setPlatformWindow(&platform, window);
	ui.layout(320.f, 180.f);

	const std::vector<TitleBarRegion> regions = ui.titleBarRegions();
	CHECK_EQ(regions.size(), 1u);
	if (regions.empty()) return;
	const TitleBarRegion& region = regions.front();
	CHECK_EQ(region.bar.w, 320.f);
	CHECK_EQ(region.bar.h, 36.f);
	CHECK(region.buttons);
	// Caption zones: three equal zones ending at the bar's right edge, min | max | close.
	CHECK_NEAR(region.close.right(), region.bar.right(), 0.5f);
	CHECK_NEAR(region.close.w, region.maximize.w, 0.5f);
	CHECK_NEAR(region.minimize.right(), region.maximize.x, 0.5f);
	CHECK(region.minimize.x < region.maximize.x && region.maximize.x < region.close.x);

	const auto zoneCenter = [](const Rect& r) { return Vec2{ r.x + r.w * 0.5f, r.y + r.h * 0.5f }; };
	// Close triggers once, on release inside the zone.
	ui.pointerDown(zoneCenter(region.close));
	CHECK_EQ(platform.closeCount(), 0u);
	ui.pointerUp(zoneCenter(region.close));
	CHECK_EQ(platform.closeCount(), 1u);
	// Maximize toggles and reports the resulting state.
	ui.pointerDown(zoneCenter(region.maximize)); ui.pointerUp(zoneCenter(region.maximize));
	CHECK_EQ(platform.maximizeToggleCount(), 1u);
	CHECK(platform.windowMaximized(window));
	ui.pointerDown(zoneCenter(region.maximize)); ui.pointerUp(zoneCenter(region.maximize));
	CHECK_EQ(platform.maximizeToggleCount(), 2u);
	CHECK(!platform.windowMaximized(window));
	// Minimize.
	ui.pointerDown(zoneCenter(region.minimize)); ui.pointerUp(zoneCenter(region.minimize));
	CHECK_EQ(platform.minimizeCount(), 1u);
	// Releasing outside the zone cancels the caption action.
	ui.pointerDown(zoneCenter(region.minimize));
	ui.pointerUp({ zoneCenter(region.minimize).x, region.bar.bottom() + 10.f });
	CHECK_EQ(platform.minimizeCount(), 1u);

	// Double-click on the bar's empty stretch (the growing title text is non-interactive)
	// toggles maximize on the second press (press-to-press window).
	ui.pointerDown({ region.bar.x + 100.f, region.bar.y + 18.f });
	ui.pointerUp({ region.bar.x + 100.f, region.bar.y + 18.f });
	CHECK_EQ(platform.maximizeToggleCount(), 2u);
	CHECK_EQ(platform.systemMoveCount(), 1u);
	ui.pointerDown({ region.bar.x + 101.f, region.bar.y + 19.f });
	CHECK_EQ(platform.maximizeToggleCount(), 3u);
	CHECK(platform.windowMaximized(window));
	ui.pointerUp({ region.bar.x + 101.f, region.bar.y + 19.f });
	// A plain press elsewhere starts a native move but never toggles.
	ui.pointerDown({ region.bar.x + 170.f, region.bar.y + 18.f });
	CHECK_EQ(platform.systemMoveCount(), 2u);
	CHECK_EQ(platform.maximizeToggleCount(), 3u);
	ui.pointerUp({ region.bar.x + 170.f, region.bar.y + 18.f });
	// A non-interactive child (the bar title text) still drags the window.
	const Rect titleRect = ui.bounds(NodeId{ 3, 1 });
	ui.pointerDown({ titleRect.x + titleRect.w * 0.5f, titleRect.y + titleRect.h * 0.5f });
	CHECK_EQ(platform.systemMoveCount(), 3u);
	ui.pointerUp({ titleRect.x + titleRect.w * 0.5f, titleRect.y + titleRect.h * 0.5f });

	// Interactive children keep their clicks: no window move, no caption action.
	u32 menuPresses = 0;
	ui.setEventHandler([](void* ctx, std::string_view) { ++*static_cast<u32*>(ctx); }, &menuPresses);
	const Rect buttonRect = ui.bounds(NodeId{ 2, 1 });
	const Vec2 buttonCenter{ buttonRect.x + buttonRect.w * 0.5f, buttonRect.y + buttonRect.h * 0.5f };
	ui.pointerDown(buttonCenter);
	ui.pointerUp(buttonCenter);
	CHECK_EQ(menuPresses, 1u);
	CHECK_EQ(platform.systemMoveCount(), 3u);
	CHECK_EQ(platform.closeCount(), 1u);

	// The bar and its caption chrome paint without crashing.
	Painter painter; painter.begin(320, 180, 1.f);
	ui.paint(painter);
	const DrawList draw = painter.finish();
	CHECK(draw.nVerts > 0u);
}

TEST(ui_runtime_titlebar_requires_matching_platform_top_menu) {
	TextSystem text;
	{
		StringInterner strings;
		auto document = parseXui(R"(Window(size: 200x120)[platformTopMenu: false] { Text("x") })", "chrome-missing.xui", strings);
		auto styles = compileXss("", "chrome-missing.xss", strings);
		CHECK(document.has_value() && styles.has_value());
		if (document && styles) {
			UiRuntime ui; std::string error;
			CHECK(!ui.build(*document, *styles, strings, text, {}, error));
			CHECK(error.find("TitleBar") != std::string::npos);
		}
	}
	{
		StringInterner strings;
		auto document = parseXui(R"(Window(size: 200x120) { TitleBar[width: 100%, height: 32px] { Text("t") } })", "chrome-extra.xui", strings);
		auto styles = compileXss("", "chrome-extra.xss", strings);
		CHECK(document.has_value() && styles.has_value());
		if (document && styles) {
			UiRuntime ui; std::string error;
			CHECK(!ui.build(*document, *styles, strings, text, {}, error));
			CHECK(error.find("platformTopMenu") != std::string::npos);
		}
	}
	{
		StringInterner strings;
		auto withAttr = parseXui(R"(Window(size: 200x120)[platformTopMenu: false] { TitleBar[width: 100%, height: 32px] { Text("t") } })", "chrome-a.xui", strings);
		auto withoutAttr = parseXui(R"(Window(size: 200x120) { Text("x") })", "chrome-b.xui", strings);
		CHECK(withAttr.has_value() && withoutAttr.has_value());
		if (withAttr) CHECK(documentUsesClientChrome(*withAttr, strings));
		if (withoutAttr) CHECK(!documentUsesClientChrome(*withoutAttr, strings));
	}
}

TEST(ui_runtime_tween_reaches_final_value_without_input) {
	// Regression for the animation freeze: a transition must paint its exact final value and
	// then go idle. The old code erased completed tweens without applying `to`, leaving the
	// widget at the last sub-frame sample until the next input-driven repaint.
	constexpr std::string_view xuiSource = R"(
Window(size: 200x120) {
  Button(label: "Press")[id: "tween-button", width: 120px, height: 40px]
}
)";
	constexpr std::string_view xssSource = R"(
Button { resting: { bg: #222222FF } pressed: { bg: #EE4422FF } transition: resting -> pressed 80ms }
)";
	StringInterner strings;
	auto document = parseXui(xuiSource, "tween.xui", strings);
	auto styles = compileXss(xssSource, "tween.xss", strings);
	CHECK(document.has_value()); CHECK(styles.has_value());
	if (!document || !styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error));
	if (!error.empty()) { std::printf("  build error: %s\n", error.c_str()); return; }
	ui.layout(200.f, 120.f);
	const Rect button = ui.bounds(NodeId{ 1, 1 });
	const Vec2 center{ button.x + button.w * 0.5f, button.y + button.h * 0.5f };
	CHECK(ui.pointerDown(center));   // pressed state starts the transition; hold it
	Color final{};
	for (int frame = 0; frame < 40; ++frame) {
		ui.updateTimers(std::chrono::steady_clock::now());
		Painter painter; painter.begin(200, 120, 1.f);
		ui.paint(painter);
		const DrawList draw = painter.finish();
		if (draw.nVerts > 0) final = Color::fromRGBA8(draw.verts[draw.nVerts - 1].c0);
		std::this_thread::sleep_for(std::chrono::milliseconds(8));
		if (!ui.updateTimers(std::chrono::steady_clock::now())) break;   // idle again = tween done
	}
	CHECK_EQ(final.toRGBA8(), Color::fromRGBA8(0xEE4422FFu).toRGBA8());
	// After completion the loop must be idle: no timer keeps ticking for a finished tween.
	const auto quiet = std::chrono::steady_clock::now();
	CHECK(!ui.updateTimers(quiet + std::chrono::milliseconds(200)));
	CHECK(ui.nextTimerDelay(quiet + std::chrono::milliseconds(200)) < 0.0);
	ui.pointerUp(center);
}

TEST(ui_runtime_menu_anchors_items_keyboard_and_outside_close) {
	constexpr std::string_view xuiSource = R"(
Window(size: 320x220) {
  Column[class: "page"] {
    Button(label: "Open")[id: "menu-anchor", width: 80px, height: 28px]
    Menu[id: "ctx", anchor: "menu-anchor", width: 150px] {
      MenuItem(label: "Alpha")[press: onPick]
      MenuItem(label: "Beta")[disabled: true]
      MenuItem("-")
      MenuItem(label: "Gamma")[checked: true]
      MenuItem(label: "More") {
        MenuItem(label: "Inner")[press: onPick]
      }
    }
  }
}
)";
	StringInterner strings;
	auto document = parseXui(xuiSource, "menu.xui", strings);
	auto styles = compileXss("Menu { resting: { bg: #151E32FF } } MenuItem { resting: { fg: #EEEEEEFF } }", "menu.xss", strings);
	CHECK(document.has_value()); CHECK(styles.has_value());
	if (!document || !styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error));
	if (!error.empty()) { std::printf("  build error: %s\n", error.c_str()); return; }
	ui.layout(320.f, 220.f);
	u32 picks = 0;
	ui.setEventHandler([](void* ctx, std::string_view) { ++*static_cast<u32*>(ctx); }, &picks);

	const Rect anchor = ui.bounds(NodeId{ 2, 1 });
	const Vec2 anchorCenter{ anchor.x + anchor.w * 0.5f, anchor.y + anchor.h * 0.5f };
	// Popup rows: 4px panel inset + 28px per item row, 9px separators; popup opens 4px below.
	const f32 popupY = anchor.bottom() + 4.f + 4.f;
	const Vec2 alphaRow{ anchor.x + 40.f, popupY + 14.f };
	const Vec2 betaRow{ anchor.x + 40.f, popupY + 28.f + 14.f };
	const Vec2 moreRow{ anchor.x + 40.f, popupY + 28.f * 2 + 9.f + 28.f + 14.f };

	CHECK(ui.pointerDown(anchorCenter));
	CHECK_EQ(picks, 0u);
	// Disabled row: press is swallowed, the menu stays open.
	ui.pointerDown(betaRow); ui.pointerUp(betaRow);
	CHECK_EQ(picks, 0u);
	// Enabled row activates its handler, records the selection, closes the menu.
	ui.pointerDown(alphaRow); ui.pointerUp(alphaRow);
	CHECK_EQ(picks, 1u);
	bool stringOk = false;
	if (const auto value = ui.value("ctx"))
		if (const auto* label = std::get_if<std::string>(&*value)) stringOk = *label == "Alpha";
	CHECK(stringOk);
	// Escape closes an open menu.
	ui.pointerDown(anchorCenter); ui.pointerUp(anchorCenter);
	ui.keyDown(Key::Escape);
	// Outside press closes the menu without activating anything.
	ui.pointerDown(anchorCenter); ui.pointerUp(anchorCenter);
	ui.pointerDown({ 300.f, 200.f }); ui.pointerUp({ 300.f, 200.f });
	CHECK_EQ(picks, 1u);
	// Keyboard: open, Down to Alpha (first enabled row), Enter activates.
	ui.pointerDown(anchorCenter); ui.pointerUp(anchorCenter);
	ui.keyDown(Key::Down);
	ui.keyDown(Key::Enter);
	CHECK_EQ(picks, 2u);
	// Submenu: hovering the parent row opens it; its item activates from the submenu surface.
	ui.pointerDown(anchorCenter); ui.pointerUp(anchorCenter);
	ui.pointerMove(moreRow);
	// Opening a submenu must not leave the retained popup paint pass looping forever.
	Painter menuPainter; menuPainter.begin(320, 220, 1.f);
	ui.paint(menuPainter);
	const DrawList menuFrame = menuPainter.finish();
	CHECK(menuFrame.nVerts > 0);
	const Vec2 innerRow{ anchor.x + 150.f, moreRow.y };   // submenu opens right of the parent row
	ui.pointerDown(innerRow); ui.pointerUp(innerRow);
	CHECK_EQ(picks, 3u);
	if (const auto value = ui.value("ctx"))
		if (const auto* label = std::get_if<std::string>(&*value)) CHECK_EQ(*label, std::string("Inner"));
}

TEST(ui_runtime_number_field_live_values_invalid_state_and_blur_revert) {
	constexpr std::string_view xuiSource = R"(
Window(size: 260x140) {
  Column[class: "page"] {
    NumberField[id: "nf", value: 5, min: 0, max: 100, step: 2, unit: "ms", width: 160px, height: 30px, change: onNumber]
    Button(label: "Other")[id: "elsewhere", width: 60px, height: 24px]
  }
}
)";
	StringInterner strings;
	auto document = parseXui(xuiSource, "number.xui", strings);
	auto styles = compileXss("NumberField { resting: { bg: #101827FF } } error { border: 1px solid #C0392BFF }", "number.xss", strings);
	CHECK(document.has_value()); CHECK(styles.has_value());
	if (!document || !styles) return;
	TextSystem text; UiRuntime ui; std::string error;
	CHECK(ui.build(*document, *styles, strings, text, {}, error));
	if (!error.empty()) { std::printf("  build error: %s\n", error.c_str()); return; }
	ui.layout(260.f, 140.f);
	u32 changes = 0;
	ui.setEventHandler([](void* ctx, std::string_view) { ++*static_cast<u32*>(ctx); }, &changes);

	const Rect field = ui.bounds(NodeId{ 2, 1 });
	const Vec2 fieldCenter{ field.x + 40.f, field.y + field.h * 0.5f };
	ui.pointerDown(fieldCenter);
	CHECK(ui.keyDown(Key::A, static_cast<u8>(Mod::Ctrl), nullptr));   // select all
	// Live typing commits per keystroke: "4" then "2" -> 42 without pressing Enter.
	ui.textInput(static_cast<u32>('4'));
	if (const auto value = ui.value("nf")) { const auto* number = std::get_if<f64>(&*value); CHECK(number && *number == 4.0); }
	(void)changes;
	ui.textInput(static_cast<u32>('2'));
	if (const auto value = ui.value("nf")) { const auto* number = std::get_if<f64>(&*value); CHECK(number && *number == 42.0); }
	// Invalid text flags the field; blur reverts to the last valid canonical text.
	ui.keyDown(Key::Backspace);
	ui.keyDown(Key::Backspace);
	ui.textInput(static_cast<u32>('-'));
	ui.pointerDown({ field.x + field.w + 40.f, field.y + field.h + 20.f });   // blur
	if (const auto value = ui.value("nf")) { const auto* number = std::get_if<f64>(&*value); CHECK(number && *number == 4.0); }
	const std::string_view displayedValue = ui.controlText("nf");
	CHECK(displayedValue.find("4") != std::string_view::npos);
	// Arrow keys step from the committed value with live events.
	ui.pointerDown(fieldCenter);
	ui.keyDown(Key::Up);
	if (const auto value = ui.value("nf")) { const auto* number = std::get_if<f64>(&*value); CHECK(number && *number == 6.0); }
}
