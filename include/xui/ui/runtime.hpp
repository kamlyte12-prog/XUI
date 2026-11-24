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
#include <xui/ui/model.hpp>
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <memory>
#include <variant>
#include <unordered_map>
#include <functional>
#include <utility>

namespace xui {

	/// Parsed structure and compiled style assets for one runtime document.
	struct UiAssetBundle {
		Document document;
		StyleSheet styles;
		std::vector<DiagnosticList> validationWarnings;
	};

	/// Parse, compile and schema-check a basic single-file UI without repeating host boilerplate.
	/// Import loading and dynamic expression evaluation remain explicit higher-level steps.
	bool loadUiAssets(std::string_view xuiSource, std::string_view xssSource, StringInterner& strings,
					  UiAssetBundle& out, std::string& error, std::string_view xuiName = "<memory>.xui",
					  std::string_view xssName = "<memory>.xss", XuiValidationOptions validation = {});

	/// Function called when an authored widget event is activated.
	using UiEventFn = void (*)(void* context, std::string_view handlerName);
	/// Return true to accept the proposed text; return false to reject it and optionally provide an error.
	using UiInputValidationFn = bool (*)(void* context, std::string_view handlerName, std::string_view widgetId,
										 std::string_view proposedText, std::string& errorMessage);

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
		bool osWindow = false; ///< authored floatMode: "os-window" policy; actual surface ownership is facade-managed
		bool closed = false;
	};

	/// IDs of checked CheckItem children, returned by a CheckList's authored id.
	using CheckListValue = std::vector<std::string>;
	using UiValue = std::variant<bool, f64, std::string, Color, u32, Vec2, DockPanelValue, CheckListValue>;

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
		/// Non-owning, virtualized row source for ListView and Table. Callbacks run on the UI thread;
		/// `text` must assign one row label and `cell` one Table cell without retaining `out`.
		struct ListItemProvider {
			void* context = nullptr;
			u32 (*rowCount)(void* context) = nullptr;
			bool (*text)(void* context, u32 row, std::string& out) = nullptr;
			bool (*cell)(void* context, u32 row, u32 column, std::string& out) = nullptr;
		};
		UiRuntime() = default;
		UiRuntime(const UiRuntime&) = delete;
		UiRuntime& operator=(const UiRuntime&) = delete;

		/// Build a runtime tree from a document with one Window root.
		/// Supports literal and model-bound Text/Button content, `if`/`for` control flow, and `press: handlerName`.
		/// For loops expand the current model collection during build; use ListItemProvider for live large collections.
		bool build(const Document& document, StyleSheet& styles, StringInterner& strings,
			TextSystem& text, FontHandle font, std::string& error);
		/// Build from an import-loaded program after expanding its declared components.
		bool build(const Program& program, StyleSheet& styles, StringInterner& strings,
			TextSystem& text, FontHandle font, std::string& error);
		/// Set a dotted model path (for example `user.name`) and reevaluate dependent bindings.
		bool setModelValue(std::string_view path, ModelValue value);
		ModelValue modelValue(std::string_view path) const;
		/// Provide rows without serializing a large data set into a pipe-separated property.
		/// Register after build; visible rows/cells are requested lazily during paint and interaction.
		bool setListItemProvider(std::string_view id, ListItemProvider provider);
		/// Attach a font after a renderer-backed glyph atlas is ready.
		bool setFont(FontHandle font, std::string& error);
		/// Register a loaded font face for the `.xss` font-family and font-weight properties.
		bool registerFont(std::string_view family, u16 weight, FontHandle font, std::string& error);
		/// Activate a named .xss palette theme. Existing controls keep their matched selectors and
		/// resolve palette slots against this theme on the next draw.
		bool setTheme(std::string_view name);
		/// Set locale-sensitive formatting for NumberField and DatePicker display text.
		void setLocale(std::string_view locale);
		/// Disable motion effects when requested by the application or operating system.
		void setReducedMotion(bool reduced);
		/// Export the authored widget tree and inferred accessibility semantics as JSON.
		std::string dumpAccessibilityTree() const;
		u32 theme() const { return m_theme; }
		/// Synchronize the resolved native-window defaults selected by the Xui facade.
		void setWindowMetadata(std::string_view title, u32 width, u32 height) {
			m_windowTitle.assign(title); m_windowWidth = width; m_windowHeight = height;
		}
		void updateWindowTitle(std::string_view title) { m_windowTitle.assign(title); }

		/// Lay out the retained tree in logical pixels, retaining the last DPI scale.
		void layout(f32 width, f32 height);
		/// Pass the current DPI so intrinsic text measurements match paint()'s glyph raster size.
		void layout(f32 width, f32 height, f32 dpiScale);
		/// Paint styled shapes and glyph-atlas-backed text in logical coordinates. Glyphs are rasterized
		/// at physical resolution so high-DPI displays stay crisp.
		void paint(Painter& painter, f32 dpiScale = 1.f);
		/// Paint only the named floating DockPanel subtree and its header, for a secondary native surface.
		void paintDockPanel(Painter& painter, f32 dpiScale, std::string_view panelId);

		/// Route mouse motion and button events. Returns true when visual state changed.
		bool pointerMove(Vec2 point);
		/// Cursor feedback for the frontmost widget under the pointer.
		CursorShape cursorAt(Vec2 point) const;
		/// Route a primary-button press at a logical client-space position.
		bool pointerDown(Vec2 point);
		/// Button-aware press routing. Right presses only reach context menus; ordinary widgets
		/// keep left-only activation.
		bool pointerDown(Vec2 point, MouseButton button);
		/// Clears pointer hover state when the platform reports that the cursor left the client area.
		/// Active drags continue; only hover-dependent state is updated.
		bool pointerLeave();
		/// Route release of the active pointer press at a logical client-space position.
		bool pointerUp(Vec2 point);
		/// Route wheel delta in logical pixels, starting at the deepest scrollable hit target.
		bool scrollWheel(Vec2 point, Vec2 delta);
		/// Route a key using default modifiers and without a platform clipboard adapter.
		bool keyDown(Key key);
		/// Route a key with modifier flags and optional platform clipboard access.
		bool keyDown(Key key, u8 mods, Platform* platform);
		/// Insert a Unicode code point or UTF-8 text into the focused editable control.
		bool textInput(u32 codepoint);
		bool textInput(std::string_view utf8);
		/// Update the focused editor's IME preedit text; commit through textInput().
		bool imeCompose(std::string_view utf8, i32 selectionStart = -1, i32 selectionEnd = -1);
		/// Cancel any active preedit when the native window loses focus.
		bool cancelImeComposition();
		/// Current focused editor caret rectangle in logical client coordinates, or empty.
		Rect imeCursorRect() const;
		/// Advance timers and animations. Returns true when a redraw is needed.
		bool updateTimers(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
		/// Delay until the next timer-driven visual update, or infinity when no timer is pending.
		f64 nextTimerDelay(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;
		/// Pause visual animation clocks while the host window is hidden/minimized.
		void setAnimationsPaused(bool paused, std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
		/// Whether the runtime FPS overlay is enabled.
		bool fpsCounterEnabled() const { return m_fpsEnabled; }
		/// Whether the optional Painter draw-batch count is displayed.
		bool drawBatchCounterEnabled() const { return m_drawBatchCounterEnabled; }
		void setDrawBatchCount(u32 batches) { m_drawBatchCount = batches; }
		/// Pushes that exceeded the rounded-clip shape cap in the last frame. Shown next to the
		/// batch count when non-zero, so over-nested rounded clipping is visible in a real app
		/// instead of only in painter stats.
		void setClipShapeCapFallbacks(u32 fallbacks) {
			m_clipShapeCapFallbacks = fallbacks;
		}
		/// Resolve relative Image sources from the directory containing the loaded entry document.
		void setAssetDirectory(std::string_view directory) { m_assetDirectory.assign(directory); }
		/// Take and clear the current logical-pixel damage rectangle. Returns false if none is pending.
		bool takeDamage(Rect& out) { if (!m_hasDamage) return false; out = m_damage; m_hasDamage = false; return true; }
		bool takeImageLayoutInvalidation() { const bool invalidated = m_imageLayoutInvalidated; m_imageLayoutInvalidated = false; return invalidated; }

		/// Set the callback invoked with authored handler names when runtime events fire.
		void setEventHandler(UiEventFn fn, void* context) { m_eventFn = fn; m_eventContext = context; }
		/// Set the synchronous filter/validator callback used by editable text/value controls.
		void setInputValidationHandler(UiInputValidationFn fn, void* context) {
			m_inputValidationFn = fn;
			m_inputValidationContext = context;
		}
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
		void setRenderer(Renderer* renderer);
		/// Root retained-layout node, or invalid when build has not succeeded.
		NodeId root() const { return m_root; }
		/// Current arranged logical-pixel bounds of a layout node.
		Rect bounds(NodeId node) const { return m_layout.absRect(node); }
		/// Numeric control value by retained node handle; use value(id) for authored-ID access.
		f32 controlValue(NodeId node) const { const Node* n = findNode(node); return n ? n->value : 0.f; }
		/// Read a ColorPicker's current color by its authored .xui id from a change callback.
		Color controlColor(std::string_view id) const;
		/// Read the displayed/edited text of a control by its authored ID; empty when unsupported or absent.
		std::string_view controlText(std::string_view id) const;
		/// Current validation message for a control, or empty when it has no validation error.
		std::string_view controlError(std::string_view id) const;
		/// Coalesce invalidation while a group of application changes is being applied.
		void beginUpdate() {
			++m_updateDepth;
		}
		/// End a matching batch; true at the outermost level when accumulated damage exists.
		bool endUpdate();
		/// Read a stateful value (bool, number, string, color, selection, scroll offset, checklist, or dock state).
		std::optional<UiValue> value(std::string_view id) const;
		std::vector<std::string> dockPanelIds() const;
		/// Serialize dock panel placement and visibility to a JSON layout snapshot.
		std::string dumpLayout() const;
		/// Restore a snapshot produced by dumpLayout(); returns false for malformed or incompatible data.
		bool loadLayout(std::string_view json);
		Rect dockPanelRect(std::string_view id) const;
		/// Largest authored corner radius of a panel's resolved style, used for native tear-off windows.
		f32 dockPanelRadius(std::string_view id) const;
		f32 dockPanelHeaderHeight(std::string_view id) const;
		std::string_view dockPanelTitle(std::string_view id) const;
		bool dockPanelUsesNativeTitleBar(std::string_view id) const;
		/// True when a point in a native floating panel's client area hits its painted close affordance.
		bool dockPanelCloseButtonContains(std::string_view id, Vec2 panelPoint) const;
		void setDockPanelNativeSurface(std::string_view id, bool enabled);
		/// Update/clear the shared DockPreview while an OS-window panel is dragged over the main window.
		bool updateDockDropPreview(std::string_view id, Vec2 clientPoint);
		void clearDockDropPreview(std::string_view id);
		/// Dock a native-floating panel into the host group under a client-space drop point.
		bool redockPanelAt(std::string_view id, Vec2 clientPoint);
		/// Set a supported control value. If notify is true, dispatches its change handler when applicable.
		bool setValue(std::string_view id, const UiValue& value, bool notify = false);
		/// Restart the XSS animation declared for an authored node. Once completes at its final frame;
		/// Loop repeats until cancelAnimation(). A new play request overrides and restarts current playback.
		bool playAnimation(std::string_view id, AnimationMode mode = AnimationMode::Once);
		/// Stop an authored animation at its current sampled values. The next playAnimation() starts over.
		bool cancelAnimation(std::string_view id);
		/// Return a ScrollView's current logical-pixel offset, or zero for an unknown/non-scroll ID.
		Vec2 scrollOffset(std::string_view id) const;
		/// Set a ScrollView offset, optionally animating the change.
		bool scrollTo(std::string_view id, Vec2 offset, bool smooth = false);
		/// Scroll the nearest ancestor ScrollView so the authored target is visible.
		bool scrollIntoView(std::string_view id, bool smooth = false);
		u32 windowWidth() const { return m_windowWidth; }
		u32 windowHeight() const { return m_windowHeight; }
		const std::string& windowTitle() const { return m_windowTitle; }
		/// Number of authored .xui nodes (internal popup editor nodes are excluded).
		u32 nodeCount() const { return m_authoredNodeCount; }

	private:
		friend struct UiRuntimeFontWeightProbe; // test-only white-box check; not a public runtime API
		struct StyleTween {
			PropId property = PropId::Bg;
			StyleValue from, to;
			std::chrono::steady_clock::time_point start{};
			f32 durationMs = 0, delayMs = 0;
			Easing easing;
		};
		struct Node {
			struct ModelBinding {
				StrId property;
				u32 expression = kNoExpr;
				XPropRole role = XPropRole::Value;
			};
			struct TreeEntry {
				std::string path, label;
				i32 parent = -1;
				u32 depth = 0;
				bool hasChildren = false, expanded = true;
			};
			NodeId layout;
			StrId type, id, pressHandler, filterHandler, validateHandler;
			std::vector<ModelBinding> modelBindings;
			std::shared_ptr<const ModelValue::Object> modelScope;
			std::string modelBindPath;
			u32 ifExpression = kNoExpr;
			bool modelVisible = true, modelIfVisible = true, modelPropertyVisible = true;
			bool ifLayoutInitialized = false;
			LayoutStyle authoredIfLayout;
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
			std::chrono::steady_clock::time_point keyframeStopAt{};
			f32 keyframeDurationMs = 0;
			u32 keyframeIterations = 1;
			bool keyframeInfinite = false, keyframeActive = false;
			bool keyframeCancelFinish = false, keyframeFinishRequested = false;
			i8 keyframeCancelOverride = -1; // -1 uses animation-cancel, otherwise 0=stop / 1=finish
			bool animationTriggered = false, animationCancelled = false;
			AnimationMode animationMode = AnimationMode::Once;
			Easing keyframeEase;
			FontHandle font;
			StrId fontSelectionFamily;
			f32 fontSelectionWeight = 400.f;
			bool fontSelectionCached = false;
			StateMask resolvedStates = 0;
			bool styleResolved = false;
			u8 inheritedTextMask = 0;
			bool button = false, iconButton = false, checkList = false, checkItem = false, checkBox = false,
				 split = false, scrollView = false, slider = false, rangeSlider = false, progressBar = false;
			bool dropdown = false, comboBox = false, colorPicker = false, listView = false, table = false,
				 popupOpen = false, pickerAlpha = true;
			bool pickerEditor = false;
			bool pickerShowAlpha = true, pickerShowInputs = true, pickerShowFormatSwitcher = true;
			bool pickerShowHue = true, pickerShowSaturationValue = true;
			bool textBox = false, textArea = false, tabs = false, tab = false, readOnly = false, password = false;
			bool fpsCounter = false, showFrameTime = true, disabled = false;
			bool radioGroup = false, switchToggle = false, toggleButton = false;
			bool numberField = false, searchField = false, numberBounded = false;
			bool label = false, badge = false, separator = false, spinner = false, image = false, iconWidget = false;
			bool treeView = false;
			bool datePicker = false, segmented = false, stepper = false, rating = false, breadcrumb = false, keybindField = false;
			bool indeterminate = false;
			bool dockHost = false, dockPanel = false, dockClosable = true, dockClosed = false;
			bool dockNativeTitleBar = true;
			bool dockFloatingInitial = false, dockOsWindow = false, dockNativeSurface = false;
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
			std::string validationError;
			Vec2 dockPreferredSize{ 360.f, 260.f };
			f32 dockHeaderHeight = 30.f, dockGutter = 5.f;
			bool scrollSmooth = false, scrollAnimating = false, scrollDragX = false, scrollDragY = false;
			bool xuiScrollBehavior = false, xuiScrollSpeed = false, xuiScrollbarVisibility = false;
			f32 scrollSpeed = 42.f, scrollBarSize = 10.f;
			u8 scrollBarVisibility = 0; // 0 auto, 1 always, 2 never
			Vec2 scrollTarget{}, scrollVelocity{};
			f32 scrollBarOpacity = 0.f;
			std::chrono::steady_clock::time_point scrollActivity{}, scrollLastTick{};
			u8 stepperZone = 0;   // 0 none, 1 minus button, 2 plus button (pressed visual)
			i32 dateYear = 2026, viewYear = 2026;
			u32 dateMonth = 1, dateDay = 1, viewMonth = 1;
			bool imageLoaded = false, imageFailed = false;
			u8 imageFit = 1;   // 0 Fill, 1 Contain, 2 Cover
			u32 imageW = 0, imageH = 0;
			u32 imageRasterRequestW = 0, imageRasterRequestH = 0;
			f32 imageIntrinsicW = 0, imageIntrinsicH = 0;
			std::shared_ptr<SvgImage> imageSvg;
			TextureHandle imageTexture;
			Image imagePixels;
			std::string imageSource, unit;
			std::string iconName, treeSelection;
			i32 treeHover = -1;
			u32 numberPrecision = 0;   // 0 formats with %g (shortest round-trip style)
			f32 stepValue = 1.f;
			f32 spinnerPhase = 0.f;    // radians; advanced by the timer tick
			i32 hoveredTab = -1;
			bool checked = false, hovered = false, pressed = false, focused = false, rangeDraggingHigh = false;
			bool tooltipDisabled = false, tooltipHideOnClick = true, tooltipDismissed = false,
				 tooltipFollowCursor = false;
			u8 tooltipTrigger = 2; // 0=none, 1=hover, 2=focus, 3=both
			bool hasXuiPadding = false;
			Edges xuiPadding;
			f32 splitRatio = 0.5f, value = 0.5f, minValue = 0.f, maxValue = 1.f, rangeLow = 0.f, rangeHigh = 1.f;
			f32 listScrollY = 0.f;
			i32 listSelectedRow = -1;
			i32 tableSelectedRow = -1, tableResizeColumn = -1;
			f32 tableScrollY = 0.f, tableResizeStartX = 0.f, tableResizeStartWidth = 0.f;
			u32 maxLength = 0, selectedTab = 0, tooltipShowDelayMs = 500, tooltipHideDelayMs = 120, tabSize = 4;
			std::size_t caret = 0, selectionAnchor = 0;
			f32 textScrollY = 0.f;
			struct EditSnapshot {
				std::string text;
				std::size_t caret = 0, anchor = 0;
			};
			std::vector<EditSnapshot> undoStack, redoStack;
			std::string imePreedit;
			i32 imeSelectionStart = -1, imeSelectionEnd = -1;
			Color pickerColor{ 0.2f, 0.5f, 0.9f, 1.f };
			std::string pickerFormat = "hex";
			u32 pickerPrecision = 0;
			f32 pickerWidth = 300.f, pickerHeight = 352.f;
			u32 pickerOwner = 0xFFFFFFFFu;
			u8 pickerSlot = 0;
			u32 selectedOption = 0;
			i32 hoveredOption = -1;
			u32 payloadIndex = 0xFFFFFFFFu;
		};
		/// Cold, variable-sized widget data is stored only for controls that need it.
		struct NodePayload {
			std::vector<Node::TreeEntry> treeEntries;
			std::vector<std::string> pickerFormats{"hex", "rgb", "hsl"};
			std::vector<std::string> options;
			std::vector<std::string> tableColumns;
			std::vector<std::vector<std::string>> tableRows;
			std::vector<f32> tableColumnWidths;
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
		// One contiguous floating-panel subtree in the overlay paint order. The chrome
		// for a floating panel is emitted immediately before the next group's body so a
		// front panel body can occlude a back panel's title bar.
		struct DockPaintGroup {
			u32 hostIndex = 0xFFFFFFFFu;
			u32 panelOrder = 0xFFFFFFFFu;
			u32 begin = 0, end = 0;
		};
		struct TextQuadCache {
			std::string text;
			FontHandle font;
			u16 rasterSize = 0;
			i32 wrapWidth = 0;
			f32 dpiScale = 1.f, letterSpacing = 0.f;
			u32 atlasGeneration = 0;
			u32 fallbackGeneration = 0;
			std::vector<GlyphQuad> quads;
		};

		static Size measureLeaf(void* context, NodeId node, Constraint constraint);
		Size measure(NodeId node, Constraint constraint) const;
		void visibleTreeEntries(const Node& tree, std::vector<u32>& rows) const;
		bool buildNode(const Document& document, u32 index, u32 parentIndex, std::string& error,
					   std::shared_ptr<const ModelValue::Object> modelScope = {}, bool expandFor = true);
		ModelValue evaluateModelExpression(const Document& document, u32 expression,
										   const ModelValue::Object* scope = nullptr) const;
		void reevaluateModelNode(u32 index);
		void registerModelDependencies(u32 nodeIndex, const Document& document);
		void publishModelValue(const Node& node, std::string_view text);
		u32 listItemCount(const Node& node) const;
		bool listItemText(const Node& node, u32 row, std::string& out) const;
		bool tableCellText(const Node& node, u32 row, u32 column, std::string& out) const;
		void applyLayoutStyle(Node& node);
		void updateSlider(Node& node, Vec2 point);
		bool validateInput(Node& field, std::string_view proposedText);
		bool updateColorPicker(Node& node, Vec2 point);
		Rect pickerEditorRect(const Node& node) const;
		std::size_t textIndexAtPoint(const Node& node, Vec2 point) const;
		PickerGeometry pickerGeometry(const Node& node) const;
		void syncPickerEditors(u32 ownerIndex, u32 preserveIndex = 0xFFFFFFFFu);
		bool updatePickerFromEditor(Node& editor);
		struct PaintScope {
			u32 dockPanel = 0xFFFFFFFFu;
			bool active() const {
				return dockPanel != 0xFFFFFFFFu;
			}
		};
		bool isNodeVisible(u32 index, PaintScope scope = {}) const;
		void paintImpl(Painter& painter, f32 dpiScale, PaintScope scope = {});
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
		NodePayload& payload(Node& node) {
			return m_nodePayloads[node.payloadIndex];
		}
		const NodePayload& payload(const Node& node) const {
			return m_nodePayloads[node.payloadIndex];
		}
		StateMask activeStates(const Node& node) const;
		static f32 number(const StyleValue& value, std::size_t index, f32 fallback = 0);
		static Length length(const StyleValue& value, std::size_t index);
		Align align(const Atom& atom) const;
		Justify justify(const Atom& atom) const;
		static Color color(const PropTable& style, PropId property, Color fallback);
		FontHandle selectFont(const PropTable& style) const;
		void updateNodeFont(Node& node) const;

		LayoutTree m_layout;
		std::vector<Node> m_nodes;
		std::vector<NodePayload> m_nodePayloads;
		std::unordered_map<StrId, u32> m_nodeById;
		Document m_modelDocument;
		ModelValue::Object m_model;
		std::unordered_map<std::string, std::vector<u32>> m_bindingNodes;
		std::unordered_map<StrId, ListItemProvider> m_listItemProviders;
		std::vector<u32> m_treeRowsScratch;
		std::vector<u32> m_interactiveNodes;
		std::vector<u32> m_splitNodes;
		std::vector<u32> m_titleBars;
		std::vector<std::pair<u32, std::string>> m_pendingMenuAnchors;   // menu node -> authored anchor id
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
		std::vector<u32> m_paintOrderScratch;
		std::vector<u32> m_basePaintOrderScratch, m_paintParentScratch, m_paintFirstChildScratch,
			m_paintLastChildScratch, m_paintNextSiblingScratch;
		std::vector<i32> m_paintStackZScratch;
		std::vector<std::pair<i32, u32>> m_paintReadyScratch;
		mutable std::vector<u32> m_hitVisibilityStamp;
		mutable std::vector<u32> m_hitScrollStamp;
		mutable std::vector<u8> m_hitVisibilityValue;
		mutable std::vector<u8> m_hitScrollValue;
		mutable u32 m_hitTestGeneration = 0;
		std::vector<DockPaintGroup> m_dockPaintGroups;
		std::vector<DockHostState> m_dockHosts;
		std::vector<u32> m_dockHostByPanel;
		StyleSetId m_tooltipStyleSet;
		std::vector<GlyphQuad> m_glyphScratch;
		std::unordered_map<u32, TextQuadCache> m_textQuadCache;
		NodeId m_root;
		StyleSheet* m_styles = nullptr;
		StringInterner* m_strings = nullptr;
		TextSystem* m_text = nullptr;
		FontHandle m_font;
		struct FontFace { StrId family; u16 weight = 400; FontHandle font; };
		std::vector<FontFace> m_fontFaces;
		std::string m_windowTitle = "Xui";
		std::string m_locale;
		u32 m_windowWidth = 720, m_windowHeight = 440;
		u32 m_authoredNodeCount = 0;
		UiEventFn m_eventFn = nullptr;
		void* m_eventContext = nullptr;
		UiInputValidationFn m_inputValidationFn = nullptr;
		void* m_inputValidationContext = nullptr;
		Renderer* m_renderer = nullptr;
		std::string m_assetDirectory;
		std::chrono::steady_clock::time_point m_spinnerDeadline{};
		std::chrono::steady_clock::time_point m_animationDeadline{};
		std::chrono::steady_clock::time_point m_animationsPausedAt{};
		bool m_animationsPaused = false;
		std::chrono::steady_clock::time_point m_caretBlinkDeadline{};
		bool m_caretVisible = true;
		bool m_showWhitespace = false;
		NodeId m_dragSplit;
		SplitDrag m_splitDrag;
		f32 m_dragStart = 0;
		u32 m_dragScroll = 0xFFFFFFFFu;
		u32 m_textSelectDrag = 0xFFFFFFFFu;
		u32 m_textClickNode = 0xFFFFFFFFu;
		u8 m_textClickCount = 0;
		Vec2 m_textClickPoint{};
		std::chrono::steady_clock::time_point m_textClickTime{};
		bool m_dragScrollX = false;
		f32 m_dragScrollStart = 0, m_dragPointerStart = 0;
		f32 m_textAutoScrollVelocity = 0.f;
		std::chrono::steady_clock::time_point m_textAutoScrollDeadline{}, m_textAutoScrollLast{};
		f32 m_dpiScale = 1.f;
		bool m_fpsEnabled = false;
		bool m_drawBatchCounterEnabled = false;
		u32 m_drawBatchCount = 0;
		u32 m_clipShapeCapFallbacks = 0;
		u32 m_theme = 0;
		bool m_reducedMotion = false;
		f64 m_fpsElapsed = 0;
		u32 m_fpsFrames = 0;
		f32 m_fpsValue = 0;
		std::chrono::steady_clock::time_point m_fpsLastFrame{};
		std::chrono::steady_clock::time_point m_fpsDeadline{};
		Vec2 m_pointerPosition{};
		Rect m_damage{};
		bool m_hasDamage = false;
		u32 m_updateDepth = 0;
		bool m_updateChanged = false;
		bool m_imageLayoutInvalidated = false;
		f32 m_lastLayoutWidth = -1.f, m_lastLayoutHeight = -1.f;
		u32 m_tooltipOwner = 0xFFFFFFFFu;
		std::chrono::steady_clock::time_point m_tooltipDeadline{};
		std::chrono::steady_clock::time_point m_tooltipHideDeadline{};
		bool m_tooltipVisible = false;
		f32 m_frameTimeMs = 0.f;
		u32 m_dockCandidate = 0xFFFFFFFFu, m_dockDragPanel = 0xFFFFFFFFu;
		u32 m_dockDragHost = 0xFFFFFFFFu, m_dockDragSplit = 0xFFFFFFFFu;
		u32 m_focusedDockHost = 0xFFFFFFFFu;
		u32 m_keyboardDockPanel = 0xFFFFFFFFu;
		u32 m_openMenu = 0xFFFFFFFFu;   // menu node currently open (or nested submenu chain root)
		Vec2 m_dockDragStart{};
		Rect m_dockFloatStart{};
		DockPreview m_dockPreview{};
		u32 m_externalDockPreviewPanel = 0xFFFFFFFFu;
		bool m_dockDragging = false, m_dockSplitDragging = false, m_dockSplitHorizontal = true;
		f32 m_dockSplitStartRatio = 0.f, m_dockSplitStartPointer = 0.f, m_dockSplitExtent = 1.f;
		f32 m_dockTabDragX = 0.f;
	};

} // namespace xui
