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
		std::string appName = "xui-app";
		std::string windowTitle = "Xui";
		u32 width = 720, height = 440;
		u32 minWidth = 128, minHeight = 96;
		bool resizable = true;
		bool visible = true;
		bool glContext = true;
		bool vsync = false;
		u32 atlasSize = GlyphAtlas::kMinSize;
	};

	struct XuiRunOptions {
		/// Zero runs until the platform reports close. A positive value bounds the run (for demos/tools).
		f64 maxSeconds = 0;
		/// Redraw every loop iteration. Enabling the FPS overlay also opts into continuous rendering
		/// until the overlay is disabled, so its sample measures frame throughput instead of idle waits.
		bool redrawContinuously = false;
	};

	struct XuiRunStats { u64 frames = 0; f64 elapsedSeconds = 0; f64 framesPerSecond = 0; };

	/// Owns framework-side application state and the standard load/attach/event/render loop.
	/// Concrete Renderer and Platform instances remain application-owned and are passed by reference.
	class Xui {
	public:
		Xui() = default;
		explicit Xui(XuiOptions options) : m_options(std::move(options)) {}
		Xui(const Xui&) = delete;
		Xui& operator=(const Xui&) = delete;
		~Xui();

		bool loadFont(std::string_view path, std::string_view family = "default", u16 weight = 400);
		bool loadXui(std::string_view path);
		bool loadXss(std::string_view path);
		/// Select an @theme declared by the loaded .xss file (defaults to its base theme).
		bool setTheme(std::string_view name);
		void setWindowOptions(XuiOptions options) { m_options = std::move(options); }

		/// Initializes the platform, creates its window, initializes the renderer and connects the UI.
		bool attach(Renderer& renderer, Platform& platform);
		/// Runs until close or XuiRunOptions::maxSeconds; returns zero on normal close, nonzero on error.
		int run(XuiRunOptions options = {});
		void shutdown();

		void setEventHandler(UiEventFn handler, void* context);
		void setWindowTitle(std::string_view title);
		bool playAnimation(std::string_view id, AnimationMode mode = AnimationMode::Once) { return m_runtime.playAnimation(id, mode); }
		bool cancelAnimation(std::string_view id) { return m_runtime.cancelAnimation(id); }
		UiRuntime& runtime() { return m_runtime; }
		const UiRuntime& runtime() const { return m_runtime; }
		TextSystem& textSystem() { return m_text; }
		WindowHandle window() const { return m_window; }
		const std::string& lastError() const { return m_error; }
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
