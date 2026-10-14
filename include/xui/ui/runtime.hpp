#pragma once
/// @file runtime.hpp  Small retained runtime that connects parsed .xui/.xss assets to layout and paint.
#include <xui/lang/xui.hpp>
#include <xui/layout/layout.hpp>
#include <xui/platform/platform.hpp>
#include <xui/render/painter.hpp>
#include <xui/style/xss.hpp>
#include <xui/text/text_layout.hpp>
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <variant>

namespace xui {

/// Parsed structure and compiled style assets for one runtime document.
struct UiAssetBundle { Document document; StyleSheet styles; };

/// Parse, compile and schema-check a basic single-file UI without repeating host boilerplate.
/// Import loading and dynamic expression evaluation remain explicit higher-level steps.
bool loadUiAssets(std::string_view xuiSource, std::string_view xssSource, StringInterner& strings,
                  UiAssetBundle& out, std::string& error,
                  std::string_view xuiName = "<memory>.xui", std::string_view xssName = "<memory>.xss");

/// Function called when an authored widget event is activated.
using UiEventFn = void (*)(void* context, std::string_view handlerName);

/// Values exposed by stateful controls through their authored `.xui` id.
using UiValue = std::variant<bool, f64, std::string, Color, u32>;

/// Runtime bridge for a deliberately small, usable widget subset.
///
/// It materializes the core authored controls from a parsed Document, matches
/// selectors in a compiled StyleSheet, maps layout declarations into LayoutTree, and emits styled
/// shapes/text into Painter. The parser and stylesheet compiler remain usable independently.
class UiRuntime {
public:
    UiRuntime() = default;
    UiRuntime(const UiRuntime&) = delete;
    UiRuntime& operator=(const UiRuntime&) = delete;

    /// Build a runtime tree from a document with one Window root.
    /// Supports literal Text/Button content and `press: handlerName`; dynamic expressions, loops,
    /// component expansion, and property-filter selectors are not evaluated by this first runtime.
    bool build(const Document& document, StyleSheet& styles, StringInterner& strings,
               TextSystem& text, FontHandle font, std::string& error);
    /// Attach a font after a renderer-backed glyph atlas is ready.
    bool setFont(FontHandle font, std::string& error);
    /// Register a loaded font face for the `.xss` font-family and font-weight properties.
    bool registerFont(std::string_view family, u16 weight, FontHandle font, std::string& error);

    /// Lay out the retained tree in logical pixels.
    void layout(f32 width, f32 height);
    /// Paint styled shapes and glyph-atlas-backed text in logical coordinates. Glyphs are rasterized
    /// at physical resolution so high-DPI displays stay crisp.
    void paint(Painter& painter, f32 dpiScale = 1.f);

    /// Route mouse motion and button events. Returns true when visual state changed.
    bool pointerMove(Vec2 point);
    /// Cursor feedback for the frontmost widget under the pointer.
    CursorShape cursorAt(Vec2 point) const;
    bool pointerDown(Vec2 point);
    bool pointerUp(Vec2 point);
    bool keyDown(Key key);
    bool keyDown(Key key, u8 mods, Platform* platform);
    bool textInput(u32 codepoint);
    bool textInput(std::string_view utf8);
    bool updateTimers(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    f64 nextTimerDelay(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;
    bool fpsCounterEnabled() const { return m_fpsEnabled; }
    bool takeDamage(Rect& out) { if (!m_hasDamage) return false; out = m_damage; m_hasDamage = false; return true; }

    void setEventHandler(UiEventFn fn, void* context) { m_eventFn = fn; m_eventContext = context; }
    NodeId root() const { return m_root; }
    Rect bounds(NodeId node) const { return m_layout.absRect(node); }
    f32 controlValue(NodeId node) const { const Node* n = findNode(node); return n ? n->value : 0.f; }
    /// Read a ColorPicker's current color by its authored .xui id from a change callback.
    Color controlColor(std::string_view id) const;
    std::string_view controlText(std::string_view id) const;
    std::optional<UiValue> value(std::string_view id) const;
    bool setValue(std::string_view id, const UiValue& value, bool notify = false);
    u32 windowWidth() const { return m_windowWidth; }
    u32 windowHeight() const { return m_windowHeight; }
    const std::string& windowTitle() const { return m_windowTitle; }
    /// Number of authored .xui nodes (internal popup editor nodes are excluded).
    u32 nodeCount() const { return m_authoredNodeCount; }

private:
    struct Node {
        NodeId layout;
        StrId type, id, pressHandler;
        u32 parentIndex = 0xFFFFFFFFu;
        std::vector<StrId> classes;
        std::string text, placeholder, tooltipText, tabTitle;
        StyleSetId styleSet;
        PropTable style;
        FontHandle font;
        StateMask resolvedStates = 0;
        bool styleResolved = false;
        u8 inheritedTextMask = 0;
        bool button = false, checkItem = false, checkBox = false, split = false, slider = false, progressBar = false;
        bool dropdown = false, colorPicker = false, popupOpen = false, pickerAlpha = true;
        bool pickerEditor = false;
        bool pickerShowAlpha = true, pickerShowInputs = true, pickerShowFormatSwitcher = true;
        bool pickerShowHue = true, pickerShowSaturationValue = true;
        bool textBox = false, textArea = false, tabs = false, tab = false, readOnly = false, password = false;
        bool fpsCounter = false, showFrameTime = true, disabled = false;
        bool checked = false, hovered = false, pressed = false, focused = false;
        bool tooltipDisabled = false, tooltipHideOnClick = true, tooltipDismissed = false;
        u8 tooltipTrigger = 2; // 0=none, 1=hover, 2=focus, 3=both
        bool hasXuiPadding = false;
        Edges xuiPadding;
        f32 splitRatio = 0.5f, value = 0.5f, minValue = 0.f, maxValue = 1.f;
        u32 maxLength = 0, selectedTab = 0, tooltipShowDelayMs = 500, tooltipHideDelayMs = 120;
        std::size_t caret = 0, selectionAnchor = 0;
        Color pickerColor{0.2f, 0.5f, 0.9f, 1.f};
        std::string pickerFormat = "hex";
        std::vector<std::string> pickerFormats{"hex", "rgb", "hsl"};
        u32 pickerPrecision = 0;
        f32 pickerWidth = 240.f, pickerHeight = 264.f;
        u32 pickerOwner = 0xFFFFFFFFu;
        u8 pickerSlot = 0;
        std::vector<std::string> options;
        u32 selectedOption = 0;
        i32 hoveredOption = -1;
    };

    static Size measureLeaf(void* context, NodeId node, Constraint constraint);
    Size measure(NodeId node, Constraint constraint) const;
    bool buildNode(const Document& document, u32 index, u32 parentIndex, std::string& error);
    void applyLayoutStyle(Node& node);
    void updateSlider(Node& node, Vec2 point);
    bool updateColorPicker(Node& node, Vec2 point);
    Rect pickerEditorRect(const Node& node) const;
    void syncPickerEditors(u32 ownerIndex, u32 preserveIndex = 0xFFFFFFFFu);
    bool updatePickerFromEditor(Node& editor);
    bool isNodeVisible(u32 index) const;
    u32 hitTestNode(Vec2 point) const;
    void refreshTooltipTarget(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    void addDamage(Rect rect);
    Node* findNode(NodeId layoutNode);
    const Node* findNode(NodeId layoutNode) const;
    StateMask activeStates(const Node& node) const;
    static f32 number(const StyleValue& value, std::size_t index, f32 fallback = 0);
    static Length length(const StyleValue& value, std::size_t index);
    Align align(const Atom& atom) const;
    Justify justify(const Atom& atom) const;
    static Color color(const PropTable& style, PropId property, Color fallback);
    FontHandle selectFont(const PropTable& style) const;

    LayoutTree m_layout;
    std::vector<Node> m_nodes;
    std::vector<u32> m_interactiveNodes;
    std::vector<u32> m_splitNodes;
    StyleSetId m_tooltipStyleSet;
    std::vector<GlyphQuad> m_glyphScratch;
    NodeId m_root;
    StyleSheet* m_styles = nullptr;
    StringInterner* m_strings = nullptr;
    TextSystem* m_text = nullptr;
    FontHandle m_font;
    struct FontFace { StrId family; u16 weight = 400; FontHandle font; };
    std::vector<FontFace> m_fontFaces;
    std::string m_windowTitle = "Xui";
    u32 m_windowWidth = 720, m_windowHeight = 440;
    u32 m_authoredNodeCount = 0;
    UiEventFn m_eventFn = nullptr;
    void* m_eventContext = nullptr;
    NodeId m_dragSplit;
    SplitDrag m_splitDrag;
    f32 m_dragStart = 0;
    f32 m_dpiScale = 1.f;
    bool m_fpsEnabled = false;
    f64 m_fpsElapsed = 0;
    u32 m_fpsFrames = 0;
    f32 m_fpsValue = 0;
    std::chrono::steady_clock::time_point m_fpsLastFrame{};
    std::chrono::steady_clock::time_point m_fpsDeadline{};
    Vec2 m_pointerPosition{};
    Rect m_damage{};
    bool m_hasDamage = false;
    f32 m_lastLayoutWidth = -1.f, m_lastLayoutHeight = -1.f;
    u32 m_tooltipOwner = 0xFFFFFFFFu;
    std::chrono::steady_clock::time_point m_tooltipDeadline{};
    std::chrono::steady_clock::time_point m_tooltipHideDeadline{};
    bool m_tooltipVisible = false;
    f32 m_frameTimeMs = 0.f;
};

} // namespace xui
