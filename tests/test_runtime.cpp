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
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>

using namespace xui;

namespace {
void recordPress(void* context, std::string_view name) {
    if (name == "onSubmit") ++*static_cast<u32*>(context);
    if (name == "onChange") ++static_cast<u32*>(context)[1];
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
        if (file) return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
    return {};
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
    const Rect leftBefore = ui.bounds({2, 1});
    const Rect itemRect = ui.bounds({4, 1});
    CHECK(leftBefore.w > 0.f); CHECK(itemRect.h == 32.f);
    u32 counts[2]{}; ui.setEventHandler(recordPress, counts);
    const Vec2 itemCenter{itemRect.x + itemRect.w * 0.5f, itemRect.y + itemRect.h * 0.5f};
    CHECK(ui.pointerDown(itemCenter)); CHECK(ui.pointerUp(itemCenter));
    CHECK_EQ(counts[1], 1u);
    ui.keyDown(Key::Space);
    // The just-clicked checklist item remains focused and is keyboard-toggleable.
    CHECK_EQ(counts[1], 2u);
    const Rect splitRect = ui.bounds({1, 1});
    const f32 gutterX = leftBefore.right() + 4.f;
    const Vec2 gutterCenter{gutterX, splitRect.y + splitRect.h * 0.5f};
    CHECK(ui.pointerDown(gutterCenter));
    CHECK(ui.pointerMove({gutterCenter.x + 24.f, gutterCenter.y}));
    ui.layout(600, 360);
    const Rect leftAfter = ui.bounds({2, 1});
    CHECK(leftAfter.w > leftBefore.w + 10.f);
    ui.pointerUp({gutterCenter.x + 24.f, gutterCenter.y});
}

std::string readExample(std::string_view filename) {
    const std::filesystem::path path = std::filesystem::path(XUI_EXAMPLE_DIR) / filename;
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
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
Tabs.overlap-tabs { resting: { margin: -38px, bg: #101820 } }
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
    const Rect behind = ui.bounds({2, 1}), front = ui.bounds({5, 1});
    const f32 overlapTop = std::max(behind.y, front.y);
    const f32 overlapBottom = std::min(behind.bottom(), front.bottom());
    CHECK(overlapBottom > overlapTop);
    if (overlapBottom <= overlapTop) return;

    std::vector<std::string> events;
    ui.setEventHandler([](void* context, std::string_view name) {
        static_cast<std::vector<std::string>*>(context)->emplace_back(name);
    }, &events);
    const f32 overlapLeft = std::max({behind.x, front.x, 0.f});
    const Vec2 overlapPoint{overlapLeft + 12.f, (overlapTop + overlapBottom) * 0.5f};
    CHECK(ui.pointerMove(overlapPoint));
    CHECK(ui.pointerDown(overlapPoint));
    CHECK(ui.pointerUp(overlapPoint));
    CHECK_EQ(events.size(), 1u);
    CHECK(events.back() == "onFront");

    // If the covered dropdown received the first pointer event, its popup would
    // consume this point and invoke onDropdown instead of leaving the value alone.
    const Vec2 coveredPopupPoint{behind.x + 12.f, behind.bottom() + 8.f};
    CHECK(ui.pointerDown(coveredPopupPoint));
    CHECK(ui.pointerUp(coveredPopupPoint));
    CHECK(ui.controlText("behind") == "One");
    CHECK_EQ(events.size(), 2u);
    CHECK(events.back() == "onFront");
}

TEST(cross_platform_example_assets_parse_and_build_in_shared_runtime) {
    StringInterner strings;
    const std::string xuiSource = readExample("cross_platform.xui");
    const std::string xssSource = readExample("cross_platform.xss");
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
    const Rect slider = ui.bounds({5, 1});
    CHECK(slider.w > 0.f);
    const f32 originalValue = ui.controlValue({5, 1});
    ui.pointerDown({slider.x + slider.w * 0.95f, slider.y + slider.h * 0.5f});
    ui.pointerUp({slider.x + slider.w * 0.95f, slider.y + slider.h * 0.5f});
    CHECK(ui.controlValue({5, 1}) > originalValue);
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
    const Rect split = ui.bounds({8, 1});
    const Rect left = ui.bounds({9, 1});
    CHECK(split.w > 0.f); CHECK(left.w > 0.f);
    const f32 gutterX = left.right() + 4.f;
    const Vec2 gutter{gutterX, split.y + split.h * 0.5f};
    CHECK(ui.pointerDown(gutter));
    CHECK(ui.pointerMove({gutter.x + 20.f, gutter.y}));
    ui.layout(1000, 680);
    CHECK(ui.bounds({9, 1}).w > left.w + 5.f);
    ui.pointerUp({gutter.x + 20.f, gutter.y});
}

TEST(xui_facade_loads_assets_attaches_backends_and_runs_frames) {
    Xui app;
    const auto xuiPath = (std::filesystem::path(XUI_EXAMPLE_DIR) / "cross_platform.xui").string();
    const auto xssPath = (std::filesystem::path(XUI_EXAMPLE_DIR) / "cross_platform.xss").string();
    CHECK(app.loadXui(xuiPath));
    CHECK(app.loadXss(xssPath));
    if (!app.lastError().empty()) return;
    NullRenderer renderer;
    NullPlatform platform;
    CHECK(app.attach(renderer, platform));
    CHECK_EQ(app.runtime().nodeCount(), 23u);
    CHECK_EQ(app.run({0.025, true}), 0);
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
    CHECK(renderer.init({320, 180, 1.f, nullptr, false}));
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
    const NodeId button{3, 1}; // Window, Column, Text, Button in document order.
    const Rect buttonRect = ui.bounds(button);
    CHECK_EQ(buttonRect.x, 60.f); // centered in the Column's 288px content box
    CHECK_EQ(buttonRect.w, 200.f);
    CHECK_EQ(buttonRect.h, 48.f);

    Painter painter;
    painter.begin(320, 180, 1.f);
    text.beginFrame();
    ui.paint(painter);
    const DrawList rendered = painter.finish();
    renderer.beginFrame({320, 180, 1.f, Color{0, 0, 0, 0}, 1});
    renderer.submit(rendered);
    renderer.endFrame();
    const Image image = renderer.snapshot();
    const u32 px = static_cast<u32>(buttonRect.x + buttonRect.w / 2);
    const u32 py = static_cast<u32>(buttonRect.y + buttonRect.h / 2);
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
    const Vec2 point{buttonRect.x + buttonRect.w * 0.5f, buttonRect.y + buttonRect.h * 0.5f};
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
  Button(label: "Full width")[class: "full"]
}
)";
    constexpr std::string_view xssSource = R"(
Button.full { resting: { size: 100% 40px } }
)";
    StringInterner strings; UiAssetBundle assets; std::string error;
    CHECK(loadUiAssets(xuiSource, xssSource, strings, assets, error));
    if (!error.empty()) return;
    TextSystem text; UiRuntime ui;
    CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error));
    if (!error.empty()) return;
    ui.layout(400, 200);
    const Rect button = ui.bounds({1, 1});
    CHECK_NEAR(button.w, 400.f, 0.01f);
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
    SoftRenderer renderer; CHECK(renderer.init({160, 60, 1.f, nullptr, false}));
    TextSystem text; CHECK(text.initAtlas(&renderer));
    const FontHandle font = text.loadFont(fontBytes, &error); CHECK(font.valid());
    UiRuntime ui; CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error));
    CHECK(ui.setFont(font, error)); ui.layout(160, 60);
    const Rect bounds = ui.bounds({2, 1});
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
    SoftRenderer renderer; CHECK(renderer.init({240, 80, 1.f, nullptr, false}));
    TextSystem text; CHECK(text.initAtlas(&renderer));
    const FontHandle font = text.loadFont(fontBytes, &error); CHECK(font.valid());
    UiRuntime ui; CHECK(ui.build(assets.document, assets.styles, strings, text, font, error));
    ui.layout(240, 80);
    const Rect bounds = ui.bounds({1, 1});
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
    const Rect dropdown = ui.bounds({2, 1});
    CHECK(ui.controlText("density") == "Compact");
    CHECK(ui.pointerDown({dropdown.x + 8, dropdown.y + 8}));
    ui.pointerUp({dropdown.x + 8, dropdown.y + 8});
    CHECK(ui.pointerDown({dropdown.x + 8, dropdown.y + dropdown.h + 38}));
    CHECK(ui.controlText("density") == "Comfortable");
    CHECK(!events.empty() && events.back() == "onDensity");
    ui.pointerUp({dropdown.x + 8, dropdown.y + dropdown.h + 38});

    const Color before = ui.controlColor("accent");
    CHECK(ui.controlText("accent") == "#336699FF");
    CHECK(ui.setValue("accent", std::string("rgba(255, 0, 0, 0.5)")));
    CHECK(ui.controlColor("accent").r > 0.99f && ui.controlColor("accent").g < 0.01f);
    CHECK(ui.controlColor("accent").a > 0.49f && ui.controlColor("accent").a < 0.51f);
    CHECK(ui.setValue("accent", std::string("hsl(120, 100%, 50%)")));
    CHECK(ui.controlColor("accent").g > 0.99f && ui.controlColor("accent").r < 0.01f);
    const Color validColor=ui.controlColor("accent");
    CHECK(!ui.setValue("accent", std::string("hsl(not a color)")));
    CHECK(ui.controlColor("accent") == validColor);
    const Rect picker = ui.bounds({3, 1});
    CHECK(ui.pointerDown({picker.x + 8, picker.y + 8}));
    ui.pointerUp({picker.x + 8, picker.y + 8});
    CHECK(ui.pointerDown({picker.x + 10, picker.y + picker.h + 10}));
    CHECK(ui.controlColor("accent") != before);
    CHECK(events.back() == "onAccent");
    const Color clickedColor = ui.controlColor("accent");
    CHECK(ui.pointerMove({picker.x + 150, picker.y + picker.h + 70}));
    CHECK(ui.controlColor("accent") != clickedColor);
    ui.pointerUp({picker.x + 150, picker.y + picker.h + 70});
    const f32 popupY = picker.y + picker.h;
    CHECK(ui.pointerDown({picker.x+20.f,popupY+240.f}));
    CHECK(ui.keyDown(Key::A, static_cast<u8>(Mod::Ctrl), nullptr));
    CHECK(ui.textInput("#ff00ff80"));
    CHECK(ui.controlColor("accent").r>0.99f && ui.controlColor("accent").b>0.99f);
    CHECK(ui.controlColor("accent").a>0.49f && ui.controlColor("accent").a<0.51f);
    CHECK(ui.pointerDown({picker.x+10.f,popupY+200.f}));
    CHECK(ui.keyDown(Key::A, static_cast<u8>(Mod::Ctrl), nullptr));
    CHECK(ui.textInput("0"));
    CHECK(ui.controlColor("accent").r < 0.01f);
    CHECK(ui.pointerDown({picker.x+10.f,popupY+170.f}));
    CHECK(ui.controlText("accent").starts_with("rgba("));
    CHECK(std::get<Color>(*ui.value("accent")) == ui.controlColor("accent"));
    ui.pointerUp({picker.x+10.f,popupY+170.f});
}

TEST(ui_runtime_slider_change_fires_for_each_live_drag_value) {
    constexpr std::string_view xuiSource=R"(
Window(size: 400x160) { Column { Slider[id: "amount", value: 0.25, min: 0, max: 1, change: onChange] } }
)";
    constexpr std::string_view xssSource=R"(Slider { resting: { size: 240px 24px } })";
    StringInterner strings; UiAssetBundle assets; std::string error;
    CHECK(loadUiAssets(xuiSource,xssSource,strings,assets,error));
    TextSystem text; UiRuntime ui; CHECK(ui.build(assets.document,assets.styles,strings,text,{},error)); ui.layout(400,160);
    std::vector<std::string> events;
    ui.setEventHandler([](void* context,std::string_view event){ static_cast<std::vector<std::string>*>(context)->emplace_back(event); },&events);
    const Rect slider=ui.bounds({2,1});
    CHECK(ui.pointerDown({slider.x+24.f,slider.y+slider.h*0.5f}));
    CHECK(events.size()==1 && events.back()=="onChange");
    const f64 start=std::get<f64>(*ui.value("amount"));
    CHECK(ui.pointerMove({slider.x+190.f,slider.y+slider.h*0.5f}));
    CHECK(events.size()==2 && events.back()=="onChange");
    const f64 moved=std::get<f64>(*ui.value("amount"));
    CHECK(moved>start);
    ui.pointerUp({slider.x+190.f,slider.y+slider.h*0.5f});
    CHECK(events.size()==2); // release does not duplicate the last live change
}

TEST(ui_runtime_text_fields_check_box_tabs_and_delayed_tooltips) {
    constexpr std::string_view xuiSource = R"(
Window(title: "Inputs", size: 480x360) {
  Column {
    TextBox[id: "name", value: "A", placeholder: "Name", maxLength: 6, tooltip: "Type a short name", tooltipShowDelay: 100, tooltipHideDelay: 200, tooltipTrigger: "hover", change: onChange]
    TextArea[id: "notes", value: "", placeholder: "Notes", maxLength: 20, tooltip: "Use multiple lines", tooltipDisabled: true, change: onChange]
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
    const Rect name = ui.bounds({2, 1});
    CHECK(ui.pointerMove({name.x + 10, name.y + 10}));
    const auto now = std::chrono::steady_clock::now();
    CHECK(ui.nextTimerDelay(now) > 0.09);
    CHECK(ui.updateTimers(now + std::chrono::milliseconds(110)));
    CHECK_EQ(ui.nextTimerDelay(now + std::chrono::seconds(1)), -1.0);
    ui.pointerMove({470.f, 350.f});
    const auto hideCheck = std::chrono::steady_clock::now();
    CHECK(ui.nextTimerDelay(hideCheck) > 0.19 && ui.nextTimerDelay(hideCheck) <= 0.2);
    CHECK(ui.updateTimers(hideCheck + std::chrono::milliseconds(250)));
    CHECK_EQ(ui.nextTimerDelay(hideCheck + std::chrono::milliseconds(250)), -1.0);
    CHECK(ui.pointerMove({name.x + 10, name.y + 10}));
    const Vec2 namePoint{name.x + 10, name.y + 10};
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
    CHECK_EQ(ui.nextTimerDelay(), -1.0);
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

    const Rect check = ui.bounds({4, 1});
    const Vec2 checkPoint{check.x + 40, check.y + check.h * 0.5f};
    CHECK(ui.pointerDown(checkPoint)); CHECK(ui.pointerUp(checkPoint));
    CHECK(!std::get<bool>(*ui.value("enabled")));

    const Rect notes = ui.bounds({3, 1});
    ui.pointerMove({notes.x + 12, notes.y + 12});
    CHECK_EQ(ui.nextTimerDelay(), -1.0);
    CHECK(ui.pointerDown({notes.x + 12, notes.y + 12}));
    CHECK(ui.keyDown(Key::Enter));
    CHECK(ui.textInput('N'));
    CHECK(std::get<std::string>(*ui.value("notes")) == "\nN");
    CHECK(ui.setValue("notes", std::string("first\nsecond")));
    CHECK(ui.keyDown(Key::Home));
    CHECK(ui.keyDown(Key::Backspace));
    CHECK_EQ(std::get<std::string>(*ui.value("notes")), "firstsecond");
    CHECK(ui.keyDown(Key::Enter));
    CHECK_EQ(std::get<std::string>(*ui.value("notes")), "first\nsecond");

    const Rect tabs = ui.bounds({5, 1});
    CHECK(ui.pointerDown({tabs.x + tabs.w * 0.75f, tabs.y + 12.f}));
    CHECK_EQ(std::get<u32>(*ui.value("pages")), 1u);
    CHECK(ui.keyDown(Key::Left));
    CHECK_EQ(std::get<u32>(*ui.value("pages")), 0u);
    CHECK(ui.setValue("pages", u32{1}));
    CHECK_EQ(std::get<u32>(*ui.value("pages")), 1u);
    events.clear();
    const Rect inactivePageButton = ui.bounds({7, 1});
    const Vec2 hiddenPagePoint{inactivePageButton.x + 20.f, inactivePageButton.y + inactivePageButton.h * 0.5f};
    CHECK(ui.pointerDown(hiddenPagePoint)); CHECK(ui.pointerUp(hiddenPagePoint));
    CHECK_EQ(events.size(), 1u);
    CHECK(events.back() == "onSecond");
}

TEST(controls_example_assets_parse_validate_and_build) {
    const std::string xuiSource = readExample("controls.xui");
    const std::string xssSource = readExample("controls.xss");
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
    TextSystem text; UiRuntime ui;
    CHECK(ui.build(assets.document, assets.styles, strings, text, {}, error));
    if (!error.empty()) { std::cerr << error << '\n'; return; }
    ui.layout(1080, 760);
    CHECK(ui.value("display-name").has_value());
    CHECK(ui.value("readonly-field").has_value());
    CHECK(ui.value("notes").has_value());
    CHECK(ui.value("updates").has_value());
    CHECK(ui.value("disabled-check").has_value());
    CHECK(ui.value("volume").has_value());
    CHECK(ui.value("density").has_value());
    CHECK(ui.value("accent-color").has_value());
    CHECK(ui.value("demo-sections").has_value());
    CHECK(ui.value("task-layout").has_value());
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
        return ui.bounds({found->second, 1});
    };
    auto separatedVertically = [](Rect a, Rect b) { return a.empty() || b.empty() || a.bottom() <= b.y + 0.01f; };

    const Rect name = rectFor("display-name"), readOnly = rectFor("readonly-field");
    const Rect inputRow = rectFor("input-row"), notes = rectFor("notes"), checks = rectFor("checks-row");
    CHECK(name.w > 0.f && name.h > 0.f); CHECK(readOnly.w > 0.f && readOnly.h > 0.f);
    CHECK(inputRow.w > 0.f && inputRow.h > 0.f); CHECK(name.right() <= readOnly.x + 0.01f);
    CHECK(separatedVertically(inputRow, notes)); CHECK(separatedVertically(notes, checks));

    // Each authored tab must produce drawable content, not just parse successfully.
    Painter painter;
    for (u32 section = 0; section < 3; ++section) {
        CHECK(ui.setValue("demo-sections", section));
        ui.layout(1080, 760);
        if (section == 1) {
            const Rect actions = rectFor("actions-row"), slider = rectFor("volume");
            const Rect progress = rectFor("progress-demo"), picker = rectFor("picker-row");
            CHECK(actions.w > 0.f && actions.h > 0.f); CHECK(slider.w > 0.f && slider.h > 0.f);
            CHECK(separatedVertically(actions, slider)); CHECK(separatedVertically(slider, progress));
            CHECK(separatedVertically(progress, picker));
            CHECK(rectFor("density").w > 0.f); CHECK(rectFor("accent-color").w > 0.f);
        } else if (section == 2) {
            const Rect split = rectFor("main-split"), tasks = rectFor("tasks-panel"), content = rectFor("content-panel");
            const Rect firstTask = rectFor("task-layout"), lastTask = rectFor("task-resize");
            CHECK(split.w > 0.f && split.h > 300.f);
            CHECK(tasks.w > 0.f && content.w > 0.f && tasks.right() < content.x);
            CHECK(firstTask.w > 0.f && lastTask.h > 0.f);
            CHECK(rectFor("task-styles").y > firstTask.y);
            CHECK(rectFor("task-backend").y > rectFor("task-styles").y);
            CHECK(lastTask.y > rectFor("task-backend").y);
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
    CHECK(ui.setValue("demo-sections", u32{0}));
    ui.layout(1080, 760);
    const Rect nameField = rectFor("display-name");
    const Vec2 namePoint{nameField.x + 20.f, nameField.y + nameField.h * 0.5f};
    CHECK(ui.pointerDown(namePoint)); CHECK(ui.pointerUp(namePoint)); CHECK(ui.textInput('!'));
    CHECK(std::get<std::string>(*ui.value("display-name")) == "Guest!");
    const Rect updates = rectFor("updates");
    const Vec2 updatesPoint{updates.x + 30.f, updates.y + updates.h * 0.5f};
    CHECK(ui.pointerDown(updatesPoint)); CHECK(ui.pointerUp(updatesPoint));
    CHECK(!std::get<bool>(*ui.value("updates")));

    const Rect tabs = rectFor("demo-sections");
    const Vec2 controlsHeader{tabs.x + tabs.w * 0.5f, tabs.y + 12.f};
    CHECK(ui.pointerDown(controlsHeader)); (void)ui.pointerUp(controlsHeader);
    CHECK_EQ(std::get<u32>(*ui.value("demo-sections")), 1u);
    ui.layout(1080, 760);
    const Rect action = rectFor("run-action");
    const Vec2 actionPoint{action.x + action.w * 0.5f, action.y + action.h * 0.5f};
    CHECK(ui.pointerDown(actionPoint)); CHECK(ui.pointerUp(actionPoint));
    CHECK(std::find(events.begin(), events.end(), "onAction") != events.end());

    const Rect density = rectFor("density");
    const Vec2 densityPoint{density.x + 20.f, density.y + density.h * 0.5f};
    CHECK(ui.pointerDown(densityPoint));
    const Vec2 thirdOption{density.x + 20.f, density.y + density.h + 70.f};
    CHECK(ui.pointerDown(thirdOption));
    CHECK(std::get<std::string>(*ui.value("density")) == "Spacious");
    const Rect color = rectFor("accent-color");
    CHECK(ui.pointerDown({color.x + 20.f, color.y + color.h * 0.5f}));
    const Color beforeColor = std::get<Color>(*ui.value("accent-color"));
    CHECK(ui.pointerDown({color.x + 100.f, color.y + color.h + 55.f}));
    CHECK(std::get<Color>(*ui.value("accent-color")) != beforeColor);

    const Vec2 layoutHeader{tabs.x + tabs.w * 0.84f, tabs.y + 12.f};
    CHECK(ui.pointerDown(layoutHeader)); (void)ui.pointerUp(layoutHeader);
    CHECK_EQ(std::get<u32>(*ui.value("demo-sections")), 2u);
    ui.layout(1080, 760);
    const Rect task = rectFor("task-backend");
    const Vec2 taskPoint{task.x + 50.f, task.y + task.h * 0.5f};
    CHECK(ui.pointerDown(taskPoint)); CHECK(ui.pointerUp(taskPoint));
    CHECK(std::get<bool>(*ui.value("task-backend")));
    CHECK(std::find(events.begin(), events.end(), "onTaskBackend") != events.end());
    const Rect split = rectFor("main-split"), leftPaneBefore = rectFor("tasks-panel");
    const Vec2 gutterPoint{leftPaneBefore.right() + 4.f, split.y + split.h * 0.5f};
    CHECK(ui.pointerDown(gutterPoint));
    CHECK(ui.pointerMove({gutterPoint.x + 24.f, gutterPoint.y}));
    ui.layout(1080, 760);
    CHECK(rectFor("tasks-panel").w > leftPaneBefore.w + 10.f);
    (void)ui.pointerUp({gutterPoint.x + 24.f, gutterPoint.y});
}
