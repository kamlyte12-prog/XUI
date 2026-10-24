#pragma once
/// @file xui.hpp  Small application facade for loading an authored UI and running it on a backend.
#include <xui/platform/platform.hpp>
#include <xui/render/renderer.hpp>
#include <xui/text/text_layout.hpp>
#include <xui/ui/runtime.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xui {

	struct XuiOptions {
		std::string appName = "xui-app"; ///< Application identity passed to the platform when supported.
		std::string windowTitle = "Xui"; ///< Fallback title; authored document values apply unless this field is explicit.
		u32 width = 720, height = 440; ///< Fallback initial client size in logical pixels.
		u32 minWidth = 128, minHeight = 96; ///< Native minimum client dimensions in logical pixels.
		bool resizable = true; ///< Allow platform window resizing.
		bool visible = true; ///< Show the created window immediately.
		bool glContext = true; ///< Request a graphics context from platforms that use one.
		bool vsync = false; ///< Request vertical synchronization from the renderer.
		u32 atlasSize = GlyphAtlas::kMinSize; ///< Initial glyph atlas dimension in pixels.
		/// Set these when explicitly choosing the legacy defaults (`Xui`, 720x440); non-default direct
		/// assignments are detected automatically. XuiOptions overrides document and stylesheet values.
		bool windowTitleOverride = false, windowWidthOverride = false, windowHeightOverride = false;
	};

	struct XuiRunOptions {
		/// Zero runs until the platform reports close. A positive value bounds the run (for demos/tools).
		f64 maxSeconds = 0; ///< Maximum run duration; zero means run until the platform requests close.
		/// Redraw every loop iteration. Enabling the FPS overlay also opts into continuous rendering
		/// until the overlay is disabled, so its sample measures frame throughput instead of idle waits.
		bool redrawContinuously = false; ///< Redraw each loop iteration instead of waiting for damage/timers.
	};

	/// Summary of the most recent `run()` call. FPS is frames divided by elapsed wall time.
	struct XuiRunStats { u64 frames = 0; f64 elapsedSeconds = 0; f64 framesPerSecond = 0; };

	/// Owns framework-side application state and the standard load/attach/event/render loop.
	/// Concrete Renderer and Platform instances remain application-owned and are passed by reference.
	class Xui {
	public:
		Xui() = default;
		/// Construct the facade with fallback window and rendering options.
		explicit Xui(XuiOptions options) : m_options(std::move(options)) {}
		Xui(const Xui&) = delete;
		Xui& operator=(const Xui&) = delete;
		~Xui();

		/// Load a TTF or TrueType-outline OTF face and register it under a family and nominal weight.
		/// Variable `wght` axes are selected through `.xss font-weight` when available; see font docs for limits.
		bool loadFont(std::string_view path, std::string_view family = "default", u16 weight = 400);
		/// Parse and schema-check an `.xui` entry document, resolving its imports relative to the file.
		bool loadXui(std::string_view path);
		/// Parse and compile an `.xss` stylesheet for the loaded UI.
		bool loadXss(std::string_view path);
		/// Select an @theme declared by the loaded .xss file (defaults to its base theme).
		bool setTheme(std::string_view name);
		/// Replace window/run options. Call before `attach()` for deterministic startup configuration.
		void setWindowOptions(XuiOptions options) { m_options = std::move(options); }

		/// Initializes the platform, creates its window, initializes the renderer and connects the UI.
		/// Both referenced objects remain application-owned and must outlive `shutdown()` or this facade.
		bool attach(Renderer& renderer, Platform& platform);
		/// Runs until close or XuiRunOptions::maxSeconds; returns zero on normal close, nonzero on error.
		int run(XuiRunOptions options = {});
		/// Release renderer, window, and platform resources. Safe to call before destruction.
		void shutdown();

		/// Set the callback used by `.xui` event properties; callbacks receive the authored handler name.
		void setEventHandler(UiEventFn handler, void* context);
		/// Change the native title after attachment and synchronize the runtime's title metadata.
		void setWindowTitle(std::string_view title);
		/// Start or restart a named `.xss` keyframe animation on an authored node ID.
		bool playAnimation(std::string_view id, AnimationMode mode = AnimationMode::Once) { return m_runtime.playAnimation(id, mode); }
		/// Stop playback of the authored keyframe animation on an authored node ID.
		bool cancelAnimation(std::string_view id) { return m_runtime.cancelAnimation(id); }
		/// Access the retained runtime for values, scrolling, animation, and lower-level control.
		UiRuntime& runtime() { return m_runtime; }
		const UiRuntime& runtime() const { return m_runtime; }
		/// Access the text system for advanced font and text operations.
		TextSystem& textSystem() { return m_text; }
		/// Native platform window handle, empty before successful attachment.
		WindowHandle window() const { return m_window; }
		/// Diagnostic from the last failed load, attach, or run operation.
		const std::string& lastError() const { return m_error; }
		/// Frame and elapsed-time summary from the most recent `run()` call.
		const XuiRunStats& lastRunStats() const { return m_stats; }

	private:
		struct FontFace { std::string family; u16 weight = 400; FontHandle handle; };
		bool readFile(std::string_view path, std::string& output);
		void setError(std::string message);
		/// One full layout/paint/present pass. Also invoked by the platform's live-resize hook so
		/// interactive resize (a modal loop inside pumpEvents on Windows) redraws every step.
		void renderFrame();
		static void renderFrameThunk(void* context) { static_cast<Xui*>(context)->renderFrame(); }

		XuiOptions m_options;
		StringInterner m_strings;
		Program m_program;
		UiAssetBundle m_assets;
		TextSystem m_text;
		UiRuntime m_runtime;
		Painter m_painter;
		std::vector<FontFace> m_fonts;
		Renderer* m_renderer = nullptr;
		Platform* m_platform = nullptr;
		WindowHandle m_window;
		UiEventFn m_eventHandler = nullptr;
		void* m_eventContext = nullptr;
		XuiRunStats m_stats;
		std::string m_error;
		std::string m_themeName;
		std::string m_assetDirectory;
		bool m_hasXui = false, m_hasXss = false;
		bool m_platformReady = false, m_rendererReady = false, m_attached = false;
		u32 m_widthPixels = 0, m_heightPixels = 0;
	};

} // namespace xui
