#pragma once
/// @file runtime.hpp  Small retained runtime that connects parsed .xui/.xss assets to layout and paint.
#include <xui/lang/xui.hpp>
#include <xui/layout/layout.hpp>
#include <xui/layout/docking.hpp>
#include <xui/platform/platform.hpp>
#include <xui/render/image_io.hpp>
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

	enum class AnimationMode : u8 { Once, Loop };

	/// Values exposed by stateful controls through their authored `.xui` id.
	struct DockPanelValue {
		DockZone region = DockZone::Center;
		bool floating = false;
		Rect bounds{};
		std::string host;
		std::string targetPanel;
		std::string activeTab;
		u32 order = 0;
		bool active = false;
	};

	using UiValue = std::variant<bool, f64, std::string, Color, u32, Vec2, DockPanelValue>;

	/// Layout snapshot of one TitleBar widget. The bar paints through the normal widget path;
	/// `minimize`/`maximize`/`close` are the framework-drawn caption zones (valid when `buttons`).
	struct TitleBarRegion {
		u32 node = 0xFFFFFFFFu;
		Rect bar{};
		Rect minimize{}, maximize{}, close{};
		bool buttons = false;
	};

	/// True when the document's Window root opts into client-drawn chrome
	/// (`Window[platformTopMenu: false]`). The facade reads this before creating the window.
	bool documentUsesClientChrome(const Document& document, StringInterner& strings);

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
		/// Activate a named .xss palette theme. Existing controls keep their matched selectors and
		/// resolve palette slots against this theme on the next draw.
		bool setTheme(std::string_view name);
		u32 theme() const { return m_theme; }

		/// Lay out the retained tree in logical pixels, retaining the last DPI scale.
		void layout(f32 width, f32 height);
		/// Pass the current DPI so intrinsic text measurements match paint()'s glyph raster size.
		void layout(f32 width, f32 height, f32 dpiScale);
		/// Paint styled shapes and glyph-atlas-backed text in logical coordinates. Glyphs are rasterized
		/// at physical resolution so high-DPI displays stay crisp.
		void paint(Painter& painter, f32 dpiScale = 1.f);

		/// Route mouse motion and button events. Returns true when visual state changed.
		bool pointerMove(Vec2 point);
		/// Cursor feedback for the frontmost widget under the pointer.
		CursorShape cursorAt(Vec2 point) const;
		bool pointerDown(Vec2 point);
		/// Button-aware press routing. Right presses only reach context menus; ordinary widgets
		/// keep left-only activation.
		bool pointerDown(Vec2 point, MouseButton button);
		/// Clears pointer hover state when the platform reports that the cursor left the client area.
		/// Active drags continue; only hover-dependent state is updated.
		bool pointerLeave();
		bool pointerUp(Vec2 point);
		bool scrollWheel(Vec2 point, Vec2 delta);
		bool keyDown(Key key);
		bool keyDown(Key key, u8 mods, Platform* platform);
		bool textInput(u32 codepoint);
		bool textInput(std::string_view utf8);
		bool updateTimers(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
		f64 nextTimerDelay(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;
		/// Pause visual animation clocks while the host window is hidden/minimized.
		void setAnimationsPaused(bool paused, std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
		bool fpsCounterEnabled() const { return m_fpsEnabled; }
		bool drawBatchCounterEnabled() const { return m_drawBatchCounterEnabled; }
		void setDrawBatchCount(u32 batches) { m_drawBatchCount = batches; }
		bool takeDamage(Rect& out) { if (!m_hasDamage) return false; out = m_damage; m_hasDamage = false; return true; }

		void setEventHandler(UiEventFn fn, void* context) { m_eventFn = fn; m_eventContext = context; }
		/// Wire the platform so TitleBar caption buttons and window dragging act on the window.
		/// Without it a TitleBar still renders, but window controls degrade gracefully.
		void setPlatformWindow(Platform* platform, WindowHandle window) { m_platform = platform; m_windowHandle = window; }
		/// True when the document authors at least one TitleBar widget (client-drawn chrome).
		bool hasTitleBars() const { return !m_titleBars.empty(); }
		/// Current TitleBar geometry for hit-testing/debug tools/tests.
		std::vector<TitleBarRegion> titleBarRegions() const;
		/// Debug aid: render spaces as a middle dot and tabs as a right arrow in editable fields.
		/// Caret and selection geometry follow the substituted glyphs so positions stay aligned.
		void setWhitespaceVisible(bool visible) { m_showWhitespace = visible; }
		bool whitespaceVisible() const { return m_showWhitespace; }
		/// Supplies the renderer used to create Image widget textures (lazily, at paint time).
		/// May be nullptr for headless clients; Image widgets then paint their placeholder frame.
		void setRenderer(Renderer* renderer) { m_renderer = renderer; }
		NodeId root() const { return m_root; }
		Rect bounds(NodeId node) const { return m_layout.absRect(node); }
		f32 controlValue(NodeId node) const { const Node* n = findNode(node); return n ? n->value : 0.f; }
		/// Read a ColorPicker's current color by its authored .xui id from a change callback.
		Color controlColor(std::string_view id) const;
		std::string_view controlText(std::string_view id) const;
		std::optional<UiValue> value(std::string_view id) const;
		bool setValue(std::string_view id, const UiValue& value, bool notify = false);
		/// Restart the XSS animation declared for an authored node. Once completes at its final frame;
		/// Loop repeats until cancelAnimation(). A new play request overrides and restarts current playback.
		bool playAnimation(std::string_view id, AnimationMode mode = AnimationMode::Once);
		/// Stop an authored animation at its current sampled values. The next playAnimation() starts over.
		bool cancelAnimation(std::string_view id);
		Vec2 scrollOffset(std::string_view id) const;
		bool scrollTo(std::string_view id, Vec2 offset, bool smooth = false);
		bool scrollIntoView(std::string_view id, bool smooth = false);
		u32 windowWidth() const { return m_windowWidth; }
		u32 windowHeight() const { return m_windowHeight; }
		const std::string& windowTitle() const { return m_windowTitle; }
		/// Number of authored .xui nodes (internal popup editor nodes are excluded).
		u32 nodeCount() const { return m_authoredNodeCount; }

	private:
		struct StyleTween {
			PropId property = PropId::Bg;
			StyleValue from, to;
			std::chrono::steady_clock::time_point start{};
			f32 durationMs = 0, delayMs = 0;
			Easing easing;
		};
		struct Node {
			NodeId layout;
			StrId type, id, pressHandler;
			u32 parentIndex = 0xFFFFFFFFu;
			std::vector<StrId> classes;
			std::string text, placeholder, tooltipText, tabTitle, dockZoneName, dockTargetId, dockIcon;
			std::string dockHandler, undockHandler, resizeHandler, tabChangeHandler, closeHandler;
			StyleSetId styleSet;
			StyleSetId dockHeaderStyleSet, dockTabStyleSet, dockCloseStyleSet, dockSplitterStyleSet, dockPreviewStyleSet;
			Mat3 paintTransform;
			PropTable style;
			std::vector<StyleTween> styleTweens;
			StrId keyframeName;
			std::chrono::steady_clock::time_point keyframeStart{};
<<<<<<< HEAD
			std::chrono::steady_clock::time_point keyframeStopAt{};
			f32 keyframeDurationMs = 0;
			u32 keyframeIterations = 1;
			bool keyframeInfinite = false, keyframeActive = false;
			bool keyframeCancelFinish = false, keyframeFinishRequested = false;
			i8 keyframeCancelOverride = -1; // -1 uses animation-cancel, otherwise 0=stop / 1=finish
=======
			f32 keyframeDurationMs = 0;
			u32 keyframeIterations = 1;
			bool keyframeInfinite = false, keyframeActive = false;
>>>>>>> d262d3498d1532fcaf2d920c57fbc4ddc03ae8e3
			bool animationTriggered = false, animationCancelled = false;
			AnimationMode animationMode = AnimationMode::Once;
			Easing keyframeEase;
			FontHandle font;
			StateMask resolvedStates = 0;
			bool styleResolved = false;
			u8 inheritedTextMask = 0;
			bool button = false, checkItem = false, checkBox = false, split = false, scrollView = false, slider = false, progressBar = false;
			bool dropdown = false, colorPicker = false, popupOpen = false, pickerAlpha = true;
			bool pickerEditor = false;
			bool pickerShowAlpha = true, pickerShowInputs = true, pickerShowFormatSwitcher = true;
			bool pickerShowHue = true, pickerShowSaturationValue = true;
			bool textBox = false, textArea = false, tabs = false, tab = false, readOnly = false, password = false;
			bool fpsCounter = false, showFrameTime = true, disabled = false;
			bool radioGroup = false, switchToggle = false, toggleButton = false;
			bool numberField = false, searchField = false, numberBounded = false;
			bool label = false, badge = false, separator = false, spinner = false, image = false;
			bool datePicker = false, segmented = false, stepper = false, rating = false, breadcrumb = false, keybindField = false;
			bool indeterminate = false;
			bool dockHost = false, dockPanel = false, dockClosable = true, dockClosed = false;
			bool dockFloatingInitial = false;
<<<<<<< HEAD
			bool menu = false, menuItem = false, submenuOpen = false;
			std::string menuShortcut;
			std::string menuSelectedItem;   // Menu: label of the last activated item
			u32 menuAnchor = 0xFFFFFFFFu;   // Menu: widget id this menu opens from
			bool menuContext = false;       // Menu: open on right-click over the anchor/at the pointer
			i32 menuHover = -1;             // Menu/submenu owner: hovered row ordinal
			Vec2 menuPoint{};               // Menu: position used by context opens
			Rect submenuRect{};             // MenuItem: on-screen row rect while its submenu is open
			bool submenuPainted = false;   // MenuItem: transient paint-pass marker for open submenus
			bool titleBar = false, titleBarButtons = true;
			StyleSetId titleButtonStyleSet;
			StyleSetId numberButtonStyleSet;
			bool inputInvalid = false;   // NumberField: current text does not parse
=======
			bool titleBar = false, titleBarButtons = true;
			StyleSetId titleButtonStyleSet;
>>>>>>> d262d3498d1532fcaf2d920c57fbc4ddc03ae8e3
			Vec2 dockPreferredSize{ 360.f, 260.f };
			f32 dockHeaderHeight = 30.f, dockGutter = 5.f;
			bool scrollSmooth = false, scrollAnimating = false, scrollDragX = false, scrollDragY = false;
			bool xuiScrollBehavior = false, xuiScrollSpeed = false, xuiScrollbarVisibility = false;
			f32 scrollSpeed = 42.f, scrollBarSize = 10.f;
			u8 scrollBarVisibility = 0; // 0 auto, 1 always, 2 never
			Vec2 scrollTarget{};
			u8 stepperZone = 0;   // 0 none, 1 minus button, 2 plus button (pressed visual)
			i32 dateYear = 2026, viewYear = 2026;
			u32 dateMonth = 1, dateDay = 1, viewMonth = 1;
			bool imageLoaded = false, imageFailed = false;
			u8 imageFit = 1;   // 0 Fill, 1 Contain, 2 Cover
			u32 imageW = 0, imageH = 0;
			TextureHandle imageTexture;
			Image imagePixels;
			std::string imageSource, unit;
			u32 numberPrecision = 0;   // 0 formats with %g (shortest round-trip style)
			f32 stepValue = 1.f;
			f32 spinnerPhase = 0.f;    // radians; advanced by the timer tick
			bool checked = false, hovered = false, pressed = false, focused = false;
			bool tooltipDisabled = false, tooltipHideOnClick = true, tooltipDismissed = false;
			u8 tooltipTrigger = 2; // 0=none, 1=hover, 2=focus, 3=both
			bool hasXuiPadding = false;
			Edges xuiPadding;
			f32 splitRatio = 0.5f, value = 0.5f, minValue = 0.f, maxValue = 1.f;
			u32 maxLength = 0, selectedTab = 0, tooltipShowDelayMs = 500, tooltipHideDelayMs = 120, tabSize = 4;
			std::size_t caret = 0, selectionAnchor = 0;
			Color pickerColor{ 0.2f, 0.5f, 0.9f, 1.f };
			std::string pickerFormat = "hex";
			std::vector<std::string> pickerFormats{ "hex", "rgb", "hsl" };
			u32 pickerPrecision = 0;
			f32 pickerWidth = 300.f, pickerHeight = 352.f;
			u32 pickerOwner = 0xFFFFFFFFu;
			u8 pickerSlot = 0;
			std::vector<std::string> options;
			u32 selectedOption = 0;
			i32 hoveredOption = -1;
		};
		struct PickerGeometry {
			Rect popup, saturationValue, hue, alpha, format, channelLabels, hexLabel, hexValue;
			Rect channels[4];
		};
		struct DockHostState {
			u32 node = 0xFFFFFFFFu;
			DockTree tree;
			std::vector<u32> panelNodes;
			std::vector<LayoutStyle> authoredPanelStyles;
			std::vector<Rect> appliedBounds;
			Rect contentArea{};
			f32 gutter = 5.f;
		};

		static Size measureLeaf(void* context, NodeId node, Constraint constraint);
		Size measure(NodeId node, Constraint constraint) const;
		bool buildNode(const Document& document, u32 index, u32 parentIndex, std::string& error);
		void applyLayoutStyle(Node& node);
		void updateSlider(Node& node, Vec2 point);
		bool updateColorPicker(Node& node, Vec2 point);
		Rect pickerEditorRect(const Node& node) const;
		PickerGeometry pickerGeometry(const Node& node) const;
		void syncPickerEditors(u32 ownerIndex, u32 preserveIndex = 0xFFFFFFFFu);
		bool updatePickerFromEditor(Node& editor);
		bool isNodeVisible(u32 index) const;
		bool pointInsideScrollAncestors(u32 index, Vec2 point) const;
		Rect scrollBarRect(u32 index, bool horizontal, Rect* track = nullptr) const;
		bool setScrollOffset(u32 index, Vec2 offset, bool smooth);
		bool ensureVisible(u32 index, bool smooth);
		bool caretEditableFocused() const;
		u32 hitTestNode(Vec2 point) const;
		Rect searchClearRect(const Node& node) const;
		void applyNumberStep(Node& node, f32 direction);
		bool commitNumberText(Node& node);
		void resetCaretBlink();
		bool selectSegment(Node& node, u32 index, bool notify);
		void updateDockLayouts();
		void updateDockHostLayout(DockHostState& host, bool force = false);
		// TitleBar: caption buttons, drag-to-move, double-click maximize. Window actions run
		// through the Platform interface set by setPlatformWindow().
		u32 frontTitleBar(Vec2 point) const;
		bool isInteractiveNode(u32 index) const;
		void titleCaptionRects(const Rect& barRect, Rect& minimize, Rect& maximize, Rect& close) const;
		bool titlePointerDown(Vec2 point);
		bool titlePointerMove(Vec2 point);
		bool titlePointerUp(Vec2 point);
<<<<<<< HEAD
		// Popup menus: virtual anchored overlays (rows are retained nodes, never laid out).
		u32 nodeIndexOf(const Node& node) const;
		u32 firstMenuChildIndex(u32 owner) const;
		u32 nextMenuSiblingIndex(u32 index) const;
		static bool menuRowIsSeparator(const Node& row);
		Rect menuAnchorHitRect(const Node& menu) const;
		Rect menuPopupRect(const Node& menu) const;
		Rect submenuPopupRect(const Node& item) const;
		u32 menuHitRow(const Node& owner, const Rect& popupRect, Vec2 point, Rect* rowRect = nullptr) const;
		void closeAllMenus(const Node* except = nullptr);
		bool menuOpenAtAnchor(Vec2 point, MouseButton button);
		bool menuPointerDown(Vec2 point);
		bool menuPointerMove(Vec2 point);
		bool menuRowPress(u32 itemIndex, const Rect& row);
		void menuActivateItem(Node& item);
		bool menuKeyDown(Key key);
=======
>>>>>>> d262d3498d1532fcaf2d920c57fbc4ddc03ae8e3
		DockHostState* dockHostForPanel(u32 panelNode);
		const DockHostState* dockHostForPanel(u32 panelNode) const;
		DockHostState* dockHostAt(Vec2 point);
		u32 dockTabAt(const DockHostState& host, u32 groupPanel, Vec2 point) const;
		bool dockPointerDown(Vec2 point);
		bool dockPointerMove(Vec2 point);
		bool dockPointerUp(Vec2 point);
		void fireDockHandler(u32 panelNode, const std::string& handler);
		void addDockDamage(const DockHostState& host);
		void stepViewMonth(Node& node, i32 direction);
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
		std::vector<u32> m_titleBars;
<<<<<<< HEAD
		std::vector<std::pair<u32, std::string>> m_pendingMenuAnchors;   // menu node -> authored anchor id
=======
>>>>>>> d262d3498d1532fcaf2d920c57fbc4ddc03ae8e3
		Platform* m_platform = nullptr;
		WindowHandle m_windowHandle{};
		bool m_clientChrome = false;
		// TitleBar interaction state (one gesture at a time; multiple bars share it).
		u32 m_titleDragBar = 0xFFFFFFFFu;
		u32 m_titleLastDownBar = 0xFFFFFFFFu;
		u8 m_titlePress = 0;      // 0 none, 1 minimize, 2 maximize, 3 close
		u8 m_titleHover = 0;
		Vec2 m_titleLastDownPos{};
		std::chrono::steady_clock::time_point m_titleLastDown{};
		std::vector<u32> m_dockOrderScratch;
		std::vector<DockHostState> m_dockHosts;
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
		Renderer* m_renderer = nullptr;
		std::chrono::steady_clock::time_point m_spinnerDeadline{};
		std::chrono::steady_clock::time_point m_animationDeadline{};
<<<<<<< HEAD
		std::chrono::steady_clock::time_point m_animationsPausedAt{};
		bool m_animationsPaused = false;
=======
>>>>>>> d262d3498d1532fcaf2d920c57fbc4ddc03ae8e3
		std::chrono::steady_clock::time_point m_caretBlinkDeadline{};
		bool m_caretVisible = true;
		bool m_showWhitespace = false;
		NodeId m_dragSplit;
		SplitDrag m_splitDrag;
		f32 m_dragStart = 0;
		u32 m_dragScroll = 0xFFFFFFFFu;
		bool m_dragScrollX = false;
		f32 m_dragScrollStart = 0, m_dragPointerStart = 0;
		f32 m_dpiScale = 1.f;
		bool m_fpsEnabled = false;
		bool m_drawBatchCounterEnabled = false;
		u32 m_drawBatchCount = 0;
		u32 m_theme = 0;
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
		u32 m_dockCandidate = 0xFFFFFFFFu, m_dockDragPanel = 0xFFFFFFFFu;
		u32 m_dockDragHost = 0xFFFFFFFFu, m_dockDragSplit = 0xFFFFFFFFu;
		u32 m_openMenu = 0xFFFFFFFFu;   // menu node currently open (or nested submenu chain root)
		Vec2 m_dockDragStart{};
		Rect m_dockFloatStart{};
		DockPreview m_dockPreview{};
		bool m_dockDragging = false, m_dockSplitDragging = false, m_dockSplitHorizontal = true;
		f32 m_dockSplitStartRatio = 0.f, m_dockSplitStartPointer = 0.f, m_dockSplitExtent = 1.f;
		f32 m_dockTabDragX = 0.f;
	};

} // namespace xui
