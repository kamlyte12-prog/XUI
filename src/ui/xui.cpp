#include <xui/xui.hpp>
#include <xui/lang/xui.hpp>
#include <xui/style/xss.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <cmath>
#include <cctype>

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

		struct InitialWindowConfig { std::string title = "Xui"; u32 width = 720, height = 440; f32 cornerRadius = 0.f; };

		std::string_view expressionString(const Document& document, const StringInterner& strings, u32 expression) {
			if (expression >= document.exprs.size()) return {};
			const XExpr& value = document.exprs[expression];
			if (value.kind == XExprKind::String || value.kind == XExprKind::Name) return strings.view(value.name);
			return {};
		}

		bool initialDimension(const Atom& atom, u32& out) {
			if (atom.kind != AtomKind::Number || (atom.unit != Unit::None && atom.unit != Unit::Px) ||
				!std::isfinite(atom.num) || atom.num <= 0.f || atom.num > 32768.f) return false;
			out = static_cast<u32>(std::lround(atom.num));
			return out > 0;
		}

		InitialWindowConfig authoredWindowConfig(const Document& document, StyleSheet& styles,
			StringInterner& strings, u32 theme) {
			InitialWindowConfig config;
			if (document.roots.empty()) return config;
			const XNode& root = document.nodes[document.roots.front()];
			std::optional<std::string> documentTitle;
			std::optional<std::pair<u32, u32>> documentSize;
			for (const XArg& arg : document.argsOf(root)) {
				const std::string_view name = strings.view(arg.name);
				if (name == "title") {
					const std::string_view value = expressionString(document, strings, arg.expr);
					if (!value.empty()) documentTitle = std::string(value);
				}
				else if (name == "size" && arg.expr < document.exprs.size()) {
					const XExpr& value = document.exprs[arg.expr];
					if (value.kind == XExprKind::Size && value.num > 0.f && value.num2 > 0.f && value.num <= 32768.f && value.num2 <= 32768.f)
						documentSize = std::pair{ static_cast<u32>(std::lround(value.num)), static_cast<u32>(std::lround(value.num2)) };
				}
			}

			std::vector<StrId> classes;
			StrId id{};
			for (const XProp& prop : document.propsOf(root)) {
				const std::string_view name = strings.view(prop.name);
				const std::string_view value = expressionString(document, strings, prop.expr);
				if (name == "id" && !value.empty()) id = strings.intern(value);
				else if (name == "class") {
					std::size_t start = 0;
					while (start < value.size()) {
						while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
						std::size_t end = start;
						while (end < value.size() && !std::isspace(static_cast<unsigned char>(value[end]))) ++end;
						if (end > start) classes.push_back(strings.intern(value.substr(start, end - start)));
						start = end;
					}
				}
			}
			MatchInput input; input.type = root.type; input.id = id; input.classes = classes;
			const StyleSetId set = styles.match(input);
			PropTable resolved{};
			styles.resolve(set, stateBit(State::Resting), theme, resolved);
			if (resolved.has(PropId::WindowTitle)) {
				const StyleValue& title = resolved.get(PropId::WindowTitle);
				if (title.count && (title.atoms[0].kind == AtomKind::String || title.atoms[0].kind == AtomKind::Ident))
					config.title.assign(strings.view(title.atoms[0].str()));
			}
			if (resolved.has(PropId::Size)) {
				const StyleValue& size = resolved.get(PropId::Size);
				u32 width = 0, height = 0;
				if (size.count >= 2 && initialDimension(size.atoms[0], width) && initialDimension(size.atoms[1], height))
					{ config.width = width; config.height = height; }
			}
			if (resolved.has(PropId::Radius)) {
				const StyleValue& radius = resolved.get(PropId::Radius);
				if (radius.count && radius.atoms[0].kind == AtomKind::Number &&
					(radius.atoms[0].unit == Unit::None || radius.atoms[0].unit == Unit::Px))
					config.cornerRadius = std::clamp(radius.atoms[0].num, 0.f, 64.f);
			}
			if (documentTitle) config.title = std::move(*documentTitle);
			if (documentSize) { config.width = documentSize->first; config.height = documentSize->second; }
			return config;
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
		std::error_code pathError;
		const std::filesystem::path absolutePath = std::filesystem::absolute(std::filesystem::path(path), pathError);
		m_assetDirectory = (pathError ? std::filesystem::path(path) : absolutePath).parent_path().string();
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

	bool Xui::setTheme(std::string_view name) {
		if (!m_hasXss || name.empty()) { setError("Load a .xss stylesheet and provide a theme name before selecting a theme."); return false; }
		const auto index = m_assets.styles.themeIndex(m_strings.intern(name));
		if (!index) { setError("The loaded .xss file has no theme named '" + std::string(name) + "'."); return false; }
		m_themeName.assign(name);
		if (m_attached && !m_runtime.setTheme(name)) { setError("Could not activate the requested .xss theme."); return false; }
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

		const u32 activeTheme = m_themeName.empty() ? 0u : m_assets.styles.themeIndex(m_strings.intern(m_themeName)).value_or(0u);
		InitialWindowConfig initial = authoredWindowConfig(m_assets.document, m_assets.styles, m_strings, activeTheme);
		if (m_options.windowTitleOverride || m_options.windowTitle != "Xui") initial.title = m_options.windowTitle;
		if (m_options.windowWidthOverride || m_options.width != 720) initial.width = m_options.width;
		if (m_options.windowHeightOverride || m_options.height != 440) initial.height = m_options.height;
		WindowDesc windowDesc;
		windowDesc.title = initial.title;
		windowDesc.width = initial.width; windowDesc.height = initial.height;
		windowDesc.minWidth = m_options.minWidth; windowDesc.minHeight = m_options.minHeight;
		windowDesc.resizable = m_options.resizable; windowDesc.visible = m_options.visible;
		windowDesc.glContext = m_options.glContext;
		// Window[platformTopMenu: false] requests client-drawn chrome; the backend either hosts
		// it (borderless + native drag/snap on Windows) or keeps native decorations and reports
		// the fallback through Platform::setDecorated.
		windowDesc.decorations = !documentUsesClientChrome(m_assets.document, m_strings);
		windowDesc.cornerRadius = initial.cornerRadius;
		m_window = m_platform->createWindow(windowDesc);
		if (!m_window.valid()) { setError("Could not create the application window."); shutdown(); return false; }
		m_platform->makeGlCurrent(m_window);

		const Vec2i pixels = m_platform->windowSizePixels(m_window);
		const f32 dpi = m_platform->dpiScale(m_window);
		if (pixels.x <= 0 || pixels.y <= 0 || !m_renderer->init({ u32(pixels.x), u32(pixels.y), dpi,
				m_platform->nativeWindowHandle(m_window), m_options.vsync })) {
			const std::string_view detail = m_renderer->lastError();
			setError(detail.empty() ? "Could not initialize the renderer backend."
				: "Could not initialize the renderer backend: " + std::string(detail));
			shutdown(); return false;
		}
		m_rendererReady = true;
	m_runtime.setRenderer(&renderer);
	m_runtime.setAssetDirectory(m_assetDirectory);
	m_platform->setLiveResizeHook(m_window, renderFrameThunk, this);
	m_runtime.setPlatformWindow(m_platform, m_window);
		m_widthPixels = u32(pixels.x); m_heightPixels = u32(pixels.y);
		if (!m_text.initAtlas(m_renderer, m_options.atlasSize)) {
			setError("Could not initialize the glyph atlas."); shutdown(); return false;
		}

		const FontHandle defaultFont = m_fonts.empty() ? FontHandle{} : m_fonts.front().handle;
		if (!m_runtime.build(m_assets.document, m_assets.styles, m_strings, m_text, defaultFont, m_error)) {
			shutdown(); return false;
		}
		m_runtime.setWindowMetadata(initial.title, initial.width, initial.height);
		if (defaultFont.valid()) {
			for (const FontFace& font : m_fonts) {
				if (!m_runtime.registerFont(font.family, font.weight, font.handle, m_error)) {
					shutdown(); return false;
				}
			}
		}
		if (!m_themeName.empty() && !m_runtime.setTheme(m_themeName)) {
			setError("Could not activate the requested .xss theme."); shutdown(); return false;
		}
		m_runtime.setEventHandler(m_eventHandler, m_eventContext);
		m_attached = true;
		m_error.clear();
		return true;
	}

	int Xui::run(XuiRunOptions options) {
		if (!m_attached || !m_renderer || !m_platform) { setError("Call attach() before run()."); return 1; }
		bool running = true, redraw = true;
		bool wasVisible = m_platform->windowVisible(m_window);
		m_runtime.setAnimationsPaused(!wasVisible);
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
				case EventType::MouseEnter:
					redraw |= m_runtime.pointerMove({ event.x, event.y });
					m_platform->setCursor(m_runtime.cursorAt({ event.x, event.y }));
					break;
				case EventType::MouseLeave:
					redraw |= m_runtime.pointerLeave();
					m_platform->setCursor(CursorShape::Arrow);
					break;
				case EventType::MouseDown:
					if (event.button == MouseButton::Left || event.button == MouseButton::Right)
						redraw |= m_runtime.pointerDown({ event.x, event.y }, event.button);
					break;
				case EventType::MouseUp:
					if (event.button == MouseButton::Left) redraw |= m_runtime.pointerUp({ event.x, event.y });
					break;
				case EventType::Wheel:
					redraw |= m_runtime.scrollWheel({ event.x, event.y }, { event.dx, event.dy });
					break;
				case EventType::KeyDown:
					if (!((event.key == Key::F3 || event.key == Key::F4) && event.repeat))
						redraw |= m_runtime.keyDown(event.key, event.mods, m_platform);
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
			const bool visible = m_platform->windowVisible(m_window);
			if (visible != wasVisible) {
				m_runtime.setAnimationsPaused(!visible);
				wasVisible = visible;
				if (visible) redraw = true;
				else redraw = false;
			}
			if (!visible) redraw = false;
			// Enabling the FPS overlay opts into continuous rendering so it measures actual
			// throughput instead of counting its own 250 ms idle refresh timer as 4 FPS.
			if (visible && (options.redrawContinuously || m_runtime.fpsCounterEnabled())) redraw = true;
			if (visible) redraw |= m_runtime.updateTimers();

			const Vec2 logical = m_platform->windowSizeLogical(m_window);
			const Vec2i pixels = m_platform->windowSizePixels(m_window);
			const f32 dpi = m_platform->dpiScale(m_window);
			if (pixels.x > 0 && pixels.y > 0 &&
				(resized || u32(pixels.x) != m_widthPixels || u32(pixels.y) != m_heightPixels)) {
				m_renderer->resize(u32(pixels.x), u32(pixels.y), dpi);
				m_widthPixels = u32(pixels.x); m_heightPixels = u32(pixels.y);
				redraw = true;
			}
			if (visible && redraw && pixels.x > 0 && pixels.y > 0) {
				renderFrame();
				redraw = false;
			}

			m_stats.elapsedSeconds = std::chrono::duration<f64>(Clock::now() - start).count();
			if (options.maxSeconds > 0 && m_stats.elapsedSeconds >= options.maxSeconds) break;
			// FPS sampling schedules a bounded 250 ms refresh through nextTimerDelay().
			// It never inserts a wait into continuous redraw/profile mode.
			if (!redraw && (!options.redrawContinuously || !visible)) {
				const f64 timerDelay = visible ? m_runtime.nextTimerDelay() : -1.0;
				if (options.maxSeconds > 0) {
					const f64 remaining = std::max(0.0, options.maxSeconds - m_stats.elapsedSeconds);
					const f64 wait = timerDelay < 0 ? remaining : std::min(remaining, timerDelay);
					// A zero timeout means "wait indefinitely" in Platform. A timer already due
					// must instead be processed by the next loop iteration; passing zero here can
					// strand animations, caret blinks, or bounded runs until unrelated input arrives.
					if (wait > 0.0) m_platform->waitEvents(wait);
				}
				else if (timerDelay < 0.0) m_platform->waitEvents(0.0);
				else if (timerDelay > 0.0) m_platform->waitEvents(timerDelay);
			}
		}
		m_stats.elapsedSeconds = std::chrono::duration<f64>(Clock::now() - start).count();
		if (m_stats.elapsedSeconds > 0) m_stats.framesPerSecond = f64(m_stats.frames) / m_stats.elapsedSeconds;
		m_error.clear();
		return 0;
	}

	/// One full layout/paint/present pass at the window's current size. Called from the normal
	/// loop when a redraw is pending and from the platform's live-resize hook, which fires from
	/// inside the modal sizing loop on Windows — the only place that can still draw mid-drag.
	/// Reading sizes every call keeps renderer backbuffer, DPI, and layout in step per step.
	void Xui::renderFrame() {
		if (!m_attached || !m_renderer || !m_platform || !m_window.valid()) return;
		const Vec2 logical = m_platform->windowSizeLogical(m_window);
		const Vec2i pixels = m_platform->windowSizePixels(m_window);
		const f32 dpi = m_platform->dpiScale(m_window);
		if (pixels.x <= 0 || pixels.y <= 0) return;
		if (u32(pixels.x) != m_widthPixels || u32(pixels.y) != m_heightPixels) {
			m_renderer->resize(u32(pixels.x), u32(pixels.y), dpi);
			m_widthPixels = u32(pixels.x); m_heightPixels = u32(pixels.y);
		}
		m_text.beginFrame();
		m_runtime.setDrawBatchCount(m_painter.stats().drawCalls);
		// Image intrinsic size is discovered during its first paint. If that changes auto sizing,
		// resolve layout once more and repaint before presenting the frame.
		for (u8 pass = 0; pass < 2; ++pass) {
			m_runtime.layout(logical.x, logical.y, dpi);
			m_painter.begin(u32(pixels.x), u32(pixels.y), dpi);
			m_runtime.paint(m_painter, dpi);
			if (!m_runtime.takeImageLayoutInvalidation()) break;
		}
		const DrawList draw = m_painter.finish();
		m_renderer->beginFrame({ u32(pixels.x), u32(pixels.y), dpi, Color{0.04f, 0.05f, 0.08f, 1}, m_stats.frames });
		m_renderer->submit(draw);
		m_renderer->endFrame(); m_renderer->present(); m_platform->swapBuffers(m_window);
		++m_stats.frames;
	}

	void Xui::setEventHandler(UiEventFn handler, void* context) {
		m_eventHandler = handler; m_eventContext = context;
		if (m_attached) m_runtime.setEventHandler(handler, context);
	}

	void Xui::setWindowTitle(std::string_view title) {
		if (m_platform && m_window.valid()) m_platform->setWindowTitle(m_window, title);
		m_runtime.updateWindowTitle(title);
	}

	void Xui::shutdown() {
		m_runtime.setRenderer(nullptr);
		m_runtime.setPlatformWindow(nullptr, WindowHandle{});
		if (m_platform && m_window.valid()) m_platform->setLiveResizeHook(m_window, nullptr, nullptr);
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
