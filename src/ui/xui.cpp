#include <xui/xui.hpp>
#include <xui/lang/xui.hpp>
#include <xui/style/xss.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>

namespace xui {
	namespace {
		using Clock = std::chrono::steady_clock;

		bool readPath(std::string_view path, std::string& output) {
			std::ifstream file(std::string(path), std::ios::binary);
			if (!file) return false;
			output.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
			return true;
		}

		std::optional<std::string> readImportedPath(void*, std::string_view path) {
			std::string contents;
			if (!readPath(path, contents)) return std::nullopt;
			return contents;
		}
	}

	Xui::~Xui() { shutdown(); }

	void Xui::setError(std::string message) { m_error = std::move(message); }

	bool Xui::readFile(std::string_view path, std::string& output) {
		if (readPath(path, output)) return true;
		setError("Could not read file: " + std::string(path));
		return false;
	}

	bool Xui::loadFont(std::string_view path, std::string_view family, u16 weight) {
		if (m_attached) { setError("Fonts must be loaded before attach()."); return false; }
		std::string bytes, fontError;
		if (!readFile(path, bytes)) return false;
		const FontHandle handle = m_text.loadFont(bytes, &fontError);
		if (!handle.valid()) { setError("Could not load font '" + std::string(path) + "': " + fontError); return false; }
		m_fonts.push_back({ std::string(family), weight, handle });
		m_error.clear();
		return true;
	}

	bool Xui::loadXui(std::string_view path) {
		if (m_attached) { setError("The UI document must be loaded before attach()."); return false; }
		auto program = xui::loadXui(path, readImportedPath, nullptr, m_strings);
		if (!program) { setError(program.error().format()); return false; }
		auto valid = validate(*program, XuiSchema::withCoreWidgets(), m_strings);
		if (!valid) { setError(valid.error().format()); return false; }
		if (program->files.empty()) { setError("The .xui loader returned no documents."); return false; }
		m_program = std::move(*program);
		m_assets.document = m_program.files.front();
		m_hasXui = true;
		m_error.clear();
		return true;
	}

	bool Xui::loadXss(std::string_view path) {
		if (m_attached) { setError("The stylesheet must be loaded before attach()."); return false; }
		std::string source;
		if (!readFile(path, source)) return false;
		auto styles = compileXss(source, path, m_strings);
		if (!styles) { setError(styles.error().format()); return false; }
		m_assets.styles = std::move(*styles);
		m_hasXss = true;
		m_error.clear();
		return true;
	}

	bool Xui::attach(Renderer& renderer, Platform& platform) {
		if (m_attached) { setError("Xui is already attached."); return false; }
		if (!m_hasXui || !m_hasXss) { setError("Load both a .xui document and a .xss stylesheet before attach()."); return false; }

		m_renderer = &renderer;
		m_platform = &platform;
		if (!m_platform->init({ m_options.appName, true })) {
			setError("Could not initialize the platform backend."); shutdown(); return false;
		}
		m_platformReady = true;

		WindowDesc windowDesc;
		windowDesc.title = m_options.windowTitle;
		windowDesc.width = m_options.width; windowDesc.height = m_options.height;
		windowDesc.minWidth = m_options.minWidth; windowDesc.minHeight = m_options.minHeight;
		windowDesc.resizable = m_options.resizable; windowDesc.visible = m_options.visible;
		windowDesc.glContext = m_options.glContext;
		m_window = m_platform->createWindow(windowDesc);
		if (!m_window.valid()) { setError("Could not create the application window."); shutdown(); return false; }
		m_platform->makeGlCurrent(m_window);

		const Vec2i pixels = m_platform->windowSizePixels(m_window);
		const f32 dpi = m_platform->dpiScale(m_window);
		if (pixels.x <= 0 || pixels.y <= 0 || !m_renderer->init({ u32(pixels.x), u32(pixels.y), dpi,
				m_platform->nativeWindowHandle(m_window), m_options.vsync })) {
			setError("Could not initialize the renderer backend."); shutdown(); return false;
		}
		m_rendererReady = true;
		m_runtime.setRenderer(&renderer);
		m_widthPixels = u32(pixels.x); m_heightPixels = u32(pixels.y);
		if (!m_text.initAtlas(m_renderer, m_options.atlasSize)) {
			setError("Could not initialize the glyph atlas."); shutdown(); return false;
		}

		const FontHandle defaultFont = m_fonts.empty() ? FontHandle{} : m_fonts.front().handle;
		if (!m_runtime.build(m_assets.document, m_assets.styles, m_strings, m_text, defaultFont, m_error)) {
			shutdown(); return false;
		}
		if (defaultFont.valid()) {
			for (const FontFace& font : m_fonts) {
				if (!m_runtime.registerFont(font.family, font.weight, font.handle, m_error)) {
					shutdown(); return false;
				}
			}
		}
		m_runtime.setEventHandler(m_eventHandler, m_eventContext);
		m_attached = true;
		m_error.clear();
		return true;
	}

	int Xui::run(XuiRunOptions options) {
		if (!m_attached || !m_renderer || !m_platform) { setError("Call attach() before run()."); return 1; }
		bool running = true, redraw = true;
		const auto start = Clock::now();
		m_stats = {};
		while (running) {
			bool resized = false;
			Event event;
			while (m_platform->pumpEvents(event)) {
				switch (event.type) {
				case EventType::Quit:
				case EventType::CloseRequest: running = false; break;
				case EventType::MouseMove:
					redraw |= m_runtime.pointerMove({ event.x, event.y });
					m_platform->setCursor(m_runtime.cursorAt({ event.x, event.y }));
					break;
				case EventType::MouseDown:
					if (event.button == MouseButton::Left) redraw |= m_runtime.pointerDown({ event.x, event.y });
					break;
				case EventType::MouseUp:
					if (event.button == MouseButton::Left) redraw |= m_runtime.pointerUp({ event.x, event.y });
					break;
				case EventType::Wheel:
					redraw |= m_runtime.scrollWheel({ event.x, event.y }, { event.dx, event.dy });
					break;
				case EventType::KeyDown:
					if (!(event.key == Key::F3 && event.repeat)) redraw |= m_runtime.keyDown(event.key, event.mods, m_platform);
					break;
				case EventType::TextInput:
					if (event.codepoint) redraw |= m_runtime.textInput(event.codepoint);
					else if (event.text && *event.text) redraw |= m_runtime.textInput(event.text);
					break;
				case EventType::ImeCommit:
					if (event.text && *event.text) redraw |= m_runtime.textInput(event.text);
					else redraw |= m_runtime.textInput(event.codepoint);
					break;
				case EventType::Resize:
				case EventType::DpiChange: resized = true; redraw = true; break;
				case EventType::Tick: redraw = true; break;
				default: break;
				}
			}
			if (!running) break;
			if (options.redrawContinuously) redraw = true;
			redraw |= m_runtime.updateTimers();

			const Vec2 logical = m_platform->windowSizeLogical(m_window);
			const Vec2i pixels = m_platform->windowSizePixels(m_window);
			const f32 dpi = m_platform->dpiScale(m_window);
			if (pixels.x > 0 && pixels.y > 0 &&
				(resized || u32(pixels.x) != m_widthPixels || u32(pixels.y) != m_heightPixels)) {
				m_renderer->resize(u32(pixels.x), u32(pixels.y), dpi);
				m_widthPixels = u32(pixels.x); m_heightPixels = u32(pixels.y);
				redraw = true;
			}
			if (redraw && pixels.x > 0 && pixels.y > 0) {
				m_runtime.layout(logical.x, logical.y);
				m_text.beginFrame();
				m_painter.begin(u32(pixels.x), u32(pixels.y), dpi);
				m_runtime.paint(m_painter, dpi);
				const DrawList draw = m_painter.finish();
				m_renderer->beginFrame({ u32(pixels.x), u32(pixels.y), dpi, Color{0.04f, 0.05f, 0.08f, 1}, m_stats.frames });
				m_renderer->submit(draw);
				m_renderer->endFrame(); m_renderer->present(); m_platform->swapBuffers(m_window);
				++m_stats.frames;
				redraw = false;
			}

			m_stats.elapsedSeconds = std::chrono::duration<f64>(Clock::now() - start).count();
			if (options.maxSeconds > 0 && m_stats.elapsedSeconds >= options.maxSeconds) break;
			// FPS sampling schedules a bounded 250 ms refresh through nextTimerDelay().
			// It never inserts a wait into continuous redraw/profile mode.
			if (!redraw && !options.redrawContinuously) {
				const f64 timerDelay = m_runtime.nextTimerDelay();
				if (options.maxSeconds > 0) {
					const f64 remaining = std::max(0.0, options.maxSeconds - m_stats.elapsedSeconds);
					m_platform->waitEvents(timerDelay < 0 ? remaining : std::min(remaining, timerDelay));
				}
				else m_platform->waitEvents(timerDelay < 0 ? 0.0 : timerDelay);
			}
		}
		m_stats.elapsedSeconds = std::chrono::duration<f64>(Clock::now() - start).count();
		if (m_stats.elapsedSeconds > 0) m_stats.framesPerSecond = f64(m_stats.frames) / m_stats.elapsedSeconds;
		m_error.clear();
		return 0;
	}

	void Xui::setEventHandler(UiEventFn handler, void* context) {
		m_eventHandler = handler; m_eventContext = context;
		if (m_attached) m_runtime.setEventHandler(handler, context);
	}

	void Xui::setWindowTitle(std::string_view title) {
		if (m_platform && m_window.valid()) m_platform->setWindowTitle(m_window, title);
	}

	void Xui::shutdown() {
		m_runtime.setRenderer(nullptr);
		if (m_rendererReady && m_text.atlas()->ready()) m_text.atlas()->shutdown();
		if (m_rendererReady && m_renderer) m_renderer->shutdown();
		m_rendererReady = false;
		if (m_platform && m_window.valid()) m_platform->destroyWindow(m_window);
		m_window = {};
		if (m_platformReady && m_platform) m_platform->shutdown();
		m_platformReady = false;
		m_attached = false;
		m_renderer = nullptr; m_platform = nullptr;
	}

} // namespace xui
