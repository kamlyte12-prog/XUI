#ifdef _WIN32
// windows.h must precede glfw3.h: glfw3.h defines APIENTRY/WINGDIAPI/CALLBACK for its own use
// and never un-defines them, so the reverse order collides with the SDK definitions.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <glfw/glfw_platform.hpp>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <algorithm>
#include <cmath>

using namespace xui;

#ifdef _WIN32
namespace { void applySnapStyles(HWND hwnd); }
#endif

bool GlfwPlatform::init(const PlatformDesc&) { return glfwInit() == GLFW_TRUE; }
void GlfwPlatform::shutdown() {
	for (Window& w : m_windows) if (w.native) { removeChrome(w); glfwDestroyWindow(w.native); }
	m_windows.clear();
	if (m_cursor) glfwDestroyCursor(m_cursor);
	m_cursor = nullptr;
	m_cursorShapeSet = false;
	m_activeWindow = nullptr;
	glfwTerminate();
}

WindowHandle GlfwPlatform::createWindow(const WindowDesc& d) {
	glfwWindowHint(GLFW_RESIZABLE, d.resizable ? GLFW_TRUE : GLFW_FALSE);
	// Framework policy: only platforms that can also provide native window move/resize keep
	// native decorations removed. On Windows the chrome subclass below restores drag, snap and
	// resize borders; elsewhere GLFW has no move API, so the native title bar is retained.
#ifdef _WIN32
	glfwWindowHint(GLFW_DECORATED, d.decorations ? GLFW_TRUE : GLFW_FALSE);
#else
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
#endif
	if (d.glContext) {
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	}
	GLFWwindow* native = glfwCreateWindow(static_cast<int>(d.width), static_cast<int>(d.height), std::string(d.title).c_str(), nullptr, nullptr);
	if (!native) return {};
	const WindowHandle handle{ static_cast<u32>(m_windows.size()), 1 };
	m_windows.push_back({ native, handle, this });
	glfwSetWindowUserPointer(native, &m_windows.back());
	glfwSetWindowSizeCallback(native, onSize);
	glfwSetCursorPosCallback(native, onCursor);
	glfwSetCursorEnterCallback(native, onCursorEnter);
	glfwSetScrollCallback(native, onScroll);
	glfwSetMouseButtonCallback(native, onMouse);
	glfwSetKeyCallback(native, onKey);
	glfwSetCharCallback(native, onChar);
	glfwSetWindowCloseCallback(native, onClose);
#ifdef _WIN32
	if (!d.decorations) installChrome(m_windows.back());
#endif
	if (!d.visible) glfwHideWindow(native);
	return handle;
}

void GlfwPlatform::destroyWindow(WindowHandle h) {
	Window* w = find(h); if (!w) return;
	removeChrome(*w);
	glfwDestroyWindow(w->native); w->native = nullptr;
}
void GlfwPlatform::setWindowTitle(WindowHandle h, std::string_view s) { if (auto* w = find(h)) glfwSetWindowTitle(w->native, std::string(s).c_str()); }
Vec2 GlfwPlatform::windowSizeLogical(WindowHandle h) const {
	int w = 0, height = 0; if (auto* rec = find(h)) glfwGetWindowSize(rec->native, &w, &height); return { f32(w), f32(height) };
}
Vec2i GlfwPlatform::windowSizePixels(WindowHandle h) const {
	int w = 0, height = 0; if (auto* rec = find(h)) glfwGetFramebufferSize(rec->native, &w, &height); return { w, height };
}
f32 GlfwPlatform::dpiScale(WindowHandle h) const {
	const Vec2 logical = windowSizeLogical(h); const Vec2i pixels = windowSizePixels(h);
	return logical.x > 0 ? f32(pixels.x) / logical.x : 1.f;
}
bool GlfwPlatform::windowVisible(WindowHandle h) const {
	const Window* window = find(h);
	return window && window->native &&
		glfwGetWindowAttrib(window->native, GLFW_VISIBLE) == GLFW_TRUE &&
		glfwGetWindowAttrib(window->native, GLFW_ICONIFIED) != GLFW_TRUE;
}
void GlfwPlatform::setLiveResizeHook(WindowHandle h, void (*fn)(void*), void* context) {
	if (Window* w = find(h)) { w->liveResizeFn = fn; w->liveResizeContext = context; }
}

bool GlfwPlatform::pumpEvents(Event& e) {
	if (m_events.empty()) {
		if (m_polledForCurrentDrain) {
			m_polledForCurrentDrain = false;
			e = {};
			return false;
		}
		m_polledForCurrentDrain = true;
		glfwPollEvents();
		if (m_events.empty()) {
			m_polledForCurrentDrain = false;
			e = {};
			return false;
		}
	}
	e = m_events.front(); m_events.pop_front(); return true;
}
void GlfwPlatform::waitEvents(f64 timeout) { if (timeout > 0) glfwWaitEventsTimeout(timeout); else glfwWaitEvents(); }
f64 GlfwPlatform::time() const { return glfwGetTime(); }
void* GlfwPlatform::nativeWindowHandle(WindowHandle h) { if (auto* w = find(h)) return w->native; return nullptr; }
void* GlfwPlatform::nativeGlContext(WindowHandle) { return glfwGetCurrentContext(); }
void GlfwPlatform::swapBuffers(WindowHandle h) { if (auto* w = find(h)) glfwSwapBuffers(w->native); }
void GlfwPlatform::makeGlCurrent(WindowHandle h) {
	if (auto* w = find(h)) {
		m_activeWindow = w->native;
		glfwMakeContextCurrent(w->native);
		glfwSwapInterval(0); // redraw-driven UI should be able to exceed a 60 Hz display cap
	}
}
void GlfwPlatform::setCursor(CursorShape shape) {
	if (m_cursorShapeSet && shape == m_cursorShape) return;
	m_cursorShape = shape;
	m_cursorShapeSet = true;
	int id = GLFW_ARROW_CURSOR;
	if (shape == CursorShape::IBeam) id = GLFW_IBEAM_CURSOR;
	else if (shape == CursorShape::Crosshair) id = GLFW_CROSSHAIR_CURSOR;
	else if (shape == CursorShape::Hand) id = GLFW_HAND_CURSOR;
	else if (shape == CursorShape::ResizeEW) id = GLFW_HRESIZE_CURSOR;
	else if (shape == CursorShape::ResizeNS) id = GLFW_VRESIZE_CURSOR;
	else if (shape == CursorShape::ResizeNWSE || shape == CursorShape::ResizeNESW || shape == CursorShape::ResizeAll || shape == CursorShape::Grab || shape == CursorShape::Grabbing) id = GLFW_HAND_CURSOR;
	if (m_cursor) { glfwDestroyCursor(m_cursor); m_cursor = nullptr; }
	if (shape != CursorShape::Arrow) m_cursor = glfwCreateStandardCursor(id);
	if (m_activeWindow) {
		glfwSetInputMode(m_activeWindow, GLFW_CURSOR, shape == CursorShape::Hidden ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
		glfwSetCursor(m_activeWindow, shape == CursorShape::Hidden ? nullptr : m_cursor);
	}
}
void GlfwPlatform::setClipboardText(std::string_view s) { glfwSetClipboardString(nullptr, std::string(s).c_str()); }
std::string GlfwPlatform::clipboardText() const { const char* s = glfwGetClipboardString(nullptr); return s ? s : ""; }
void GlfwPlatform::requestRedraw(WindowHandle h) { if (auto* w = find(h)) glfwPostEmptyEvent(); }

bool GlfwPlatform::setDecorated(WindowHandle h, bool decorated) {
	Window* w = find(h);
	if (!w || !w->native) return false;
#ifdef _WIN32
	if (decorated) {
		// Drop the frame-stripping subclass first, then let GLFW restore its full styles.
		removeChrome(*w);
		glfwSetWindowAttrib(w->native, GLFW_DECORATED, GLFW_TRUE);
		return true;
	}
	glfwSetWindowAttrib(w->native, GLFW_DECORATED, GLFW_FALSE);
	w->customChrome = true;
	if (!w->chromeState) installChrome(*w);
	return true;
#else
	// Removing decorations without a native window-move API leaves an unmovable window,
	// so the framework keeps native chrome on this platform (documented fallback).
	if (decorated) { glfwSetWindowAttrib(w->native, GLFW_DECORATED, GLFW_TRUE); return true; }
	return false;
#endif
}

bool GlfwPlatform::beginSystemMove(WindowHandle h) {
	Window* w = find(h);
	if (!w || !w->native || !w->customChrome) return false;
#ifdef _WIN32
	HWND hwnd = glfwGetWin32Window(w->native);
	if (!hwnd) return false;
	ReleaseCapture();
	// Native modal move loop: the OS drags the window (including Aero Snap); the size
	// callback keeps rendering live if the drag ends in a snap-resize.
	POINT pt{};
	GetCursorPos(&pt);
	PostMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(pt.x, pt.y));
	return true;
#else
	return false;
#endif
}

bool GlfwPlatform::beginSystemResize(WindowHandle h, WindowEdge edge) {
	Window* w = find(h);
	if (!w || !w->native || !w->customChrome) return false;
#ifdef _WIN32
	HWND hwnd = glfwGetWin32Window(w->native);
	if (!hwnd) return false;
	LONG hit = HTCLIENT;
	switch (edge) {
	case WindowEdge::Left: hit = HTLEFT; break;
	case WindowEdge::Right: hit = HTRIGHT; break;
	case WindowEdge::Top: hit = HTTOP; break;
	case WindowEdge::Bottom: hit = HTBOTTOM; break;
	case WindowEdge::TopLeft: hit = HTTOPLEFT; break;
	case WindowEdge::TopRight: hit = HTTOPRIGHT; break;
	case WindowEdge::BottomLeft: hit = HTBOTTOMLEFT; break;
	case WindowEdge::BottomRight: hit = HTBOTTOMRIGHT; break;
	default: return false;
	}
	ReleaseCapture();
	POINT pt{};
	GetCursorPos(&pt);
	PostMessageW(hwnd, WM_NCLBUTTONDOWN, hit, MAKELPARAM(pt.x, pt.y));
	return true;
#else
	return false;
#endif
}

void GlfwPlatform::minimizeWindow(WindowHandle h) {
	if (Window* w = find(h); w && w->native) glfwIconifyWindow(w->native);
}

bool GlfwPlatform::toggleMaximize(WindowHandle h) {
	Window* w = find(h);
	if (!w || !w->native) return false;
	if (glfwGetWindowAttrib(w->native, GLFW_MAXIMIZED) == GLFW_TRUE) glfwRestoreWindow(w->native);
	else glfwMaximizeWindow(w->native);
<<<<<<< HEAD
#ifdef _WIN32
	// GLFW's maximize/restore calls should preserve GLFW_DECORATED, but reassert the
	// client-chrome style after the transition so a platform style refresh cannot add a caption.
	if (w->customChrome) applySnapStyles(glfwGetWin32Window(w->native));
#endif
=======
>>>>>>> d262d3498d1532fcaf2d920c57fbc4ddc03ae8e3
	return glfwGetWindowAttrib(w->native, GLFW_MAXIMIZED) == GLFW_TRUE;
}

bool GlfwPlatform::windowMaximized(WindowHandle h) const {
	const Window* w = find(h);
	return w && w->native && glfwGetWindowAttrib(w->native, GLFW_MAXIMIZED) == GLFW_TRUE;
}

void GlfwPlatform::closeWindow(WindowHandle h) {
	Window* w = find(h);
	if (!w || !w->native) return;
	// Mirror the native close button exactly: GLFW's close callback path queues the Quit event.
	onClose(w->native);
	glfwPostEmptyEvent();
}

<<<<<<< HEAD
void GlfwPlatform::setResizeInset(WindowHandle h, f32 pixels) {
	if (Window* w = find(h)) w->resizeInset = std::clamp(pixels, 0.f, 64.f);
}

=======
>>>>>>> d262d3498d1532fcaf2d920c57fbc4ddc03ae8e3
GlfwPlatform::Window* GlfwPlatform::find(WindowHandle h) { return h.index < m_windows.size() && m_windows[h.index].handle.gen == h.gen ? &m_windows[h.index] : nullptr; }
const GlfwPlatform::Window* GlfwPlatform::find(WindowHandle h) const { return const_cast<GlfwPlatform*>(this)->find(h); }
GlfwPlatform::Window* GlfwPlatform::record(GLFWwindow* w) { return static_cast<Window*>(glfwGetWindowUserPointer(w)); }
void GlfwPlatform::push(GLFWwindow* w, const Event& src) { if (Window* r = record(w)) { Event e = src; e.window = r->handle; r->owner->m_events.push_back(e); } }
// Interactive resize on Windows runs a modal loop inside glfwPollEvents/glfwWaitEvents; the app
// loop cannot regain control until the drag ends. The size callback therefore queues the Resize
// event AND drives one full frame per live step so widgets and layout resize continuously.
void GlfwPlatform::onSize(GLFWwindow* w, int width, int height) {
	if (Window* r = record(w)) {
		Event e{}; e.type = EventType::Resize; e.w = f32(width); e.h = f32(height);
		push(w, e);
		if (r->liveResizeFn) r->liveResizeFn(r->liveResizeContext);
	}
}
void GlfwPlatform::onCursor(GLFWwindow* w, double x, double y) { Event e{}; e.type = EventType::MouseMove; e.x = f32(x); e.y = f32(y); push(w, e); }
void GlfwPlatform::onCursorEnter(GLFWwindow* w, int entered) {
	Event e{};
	e.type = entered == GLFW_TRUE ? EventType::MouseEnter : EventType::MouseLeave;
	if (entered == GLFW_TRUE) {
		double x = 0, y = 0;
		glfwGetCursorPos(w, &x, &y);
		e.x = f32(x); e.y = f32(y);
	}
	push(w, e);
}
void GlfwPlatform::onScroll(GLFWwindow* w, double dx, double dy) {
	Event e{}; e.type = EventType::Wheel; e.dx = f32(dx); e.dy = f32(dy);
	double x = 0, y = 0; glfwGetCursorPos(w, &x, &y); e.x = f32(x); e.y = f32(y); push(w, e);
}
void GlfwPlatform::onMouse(GLFWwindow* w, int button, int action, int mods) {
	Event e{}; e.type = action == GLFW_PRESS ? EventType::MouseDown : EventType::MouseUp;
	e.button = button == GLFW_MOUSE_BUTTON_RIGHT ? MouseButton::Right : button == GLFW_MOUSE_BUTTON_MIDDLE ? MouseButton::Middle : MouseButton::Left;
	e.mods = (mods & GLFW_MOD_SHIFT ? static_cast<u8>(Mod::Shift) : 0) | (mods & GLFW_MOD_CONTROL ? static_cast<u8>(Mod::Ctrl) : 0) | (mods & GLFW_MOD_ALT ? static_cast<u8>(Mod::Alt) : 0);
	double x = 0, y = 0; glfwGetCursorPos(w, &x, &y); e.x = f32(x); e.y = f32(y); push(w, e);
}
Key GlfwPlatform::mapKey(int k) {
	if (k >= GLFW_KEY_A && k <= GLFW_KEY_Z) return static_cast<Key>('A' + k - GLFW_KEY_A);
	if (k >= GLFW_KEY_0 && k <= GLFW_KEY_9) return static_cast<Key>('0' + k - GLFW_KEY_0);
	switch (k) {
	case GLFW_KEY_TAB: return Key::Tab;
	case GLFW_KEY_ENTER: return Key::Enter;
	case GLFW_KEY_SPACE: return Key::Space;
	case GLFW_KEY_ESCAPE: return Key::Escape;
	case GLFW_KEY_BACKSPACE: return Key::Backspace;
	case GLFW_KEY_DELETE: return Key::Delete;
	case GLFW_KEY_INSERT: return Key::Insert;
	case GLFW_KEY_HOME: return Key::Home;
	case GLFW_KEY_END: return Key::End;
	case GLFW_KEY_LEFT: return Key::Left;
	case GLFW_KEY_RIGHT: return Key::Right;
	case GLFW_KEY_UP: return Key::Up;
	case GLFW_KEY_DOWN: return Key::Down;
	case GLFW_KEY_F3: return Key::F3;
	case GLFW_KEY_F4: return Key::F4;
	default: return Key::None;
	}
}
void GlfwPlatform::onKey(GLFWwindow* w, int key, int, int action, int mods) {
	if (action != GLFW_PRESS && action != GLFW_REPEAT && action != GLFW_RELEASE) return;
	Event e{}; e.type = action == GLFW_RELEASE ? EventType::KeyUp : EventType::KeyDown; e.key = mapKey(key); e.repeat = action == GLFW_REPEAT;
	e.mods = (mods & GLFW_MOD_SHIFT ? static_cast<u8>(Mod::Shift) : 0) | (mods & GLFW_MOD_CONTROL ? static_cast<u8>(Mod::Ctrl) : 0); push(w, e);
}
void GlfwPlatform::onChar(GLFWwindow* w, unsigned int codepoint) {
	Event e{}; e.type = EventType::TextInput; e.codepoint = codepoint; push(w, e);
}
void GlfwPlatform::onClose(GLFWwindow* w) { glfwSetWindowShouldClose(w, GLFW_TRUE); Event e{}; e.type = EventType::Quit; push(w, e); }

#ifdef _WIN32
// Client-drawn chrome. The subclass only intercepts WM_NCCALCSIZE (remove the native frame) and
// WM_NCHITTEST (resize borders); everything else — dragging, snapping, maximize — stays native.
// All Win32 state lives here in the .cpp so the backend header stays free of windows.h types.
namespace {
	struct ChromeState {
		GlfwPlatform::Window* window = nullptr;
		WNDPROC prevProc = nullptr;
	};
	constexpr wchar_t kChromeProp[] = L"XuiChromeProp";

	void applySnapStyles(HWND hwnd) {
<<<<<<< HEAD
		// GLFW's undecorated style is WS_POPUP without WS_THICKFRAME, which disables both Aero
		// Snap and the native sizing loop (HT* hits are ignored without a sizing border).
		// Re-add THICKFRAME and the snap boxes; WM_NCCALCSIZE keeps the frame invisible.
		const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
		SetWindowLongPtrW(hwnd, GWL_STYLE, (style & ~LONG_PTR(WS_CAPTION)) | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
=======
		// GLFW's undecorated style omits WS_MAXIMIZEBOX/WS_MINIMIZEBOX, which also disables
		// Aero Snap. Re-add both so drag-to-edge snap and Win+arrow tiling keep working.
		const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
		SetWindowLongPtrW(hwnd, GWL_STYLE, style | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
>>>>>>> d262d3498d1532fcaf2d920c57fbc4ddc03ae8e3
		SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	LRESULT CALLBACK chromeWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
		auto* state = static_cast<ChromeState*>(GetPropW(hwnd, kChromeProp));
		GlfwPlatform::Window* window = state ? state->window : nullptr;
<<<<<<< HEAD
		// Edge/corner resize margin in physical px: app-set inset or the native frame thickness.
		const LONG insetX = window && window->resizeInset > 0.f ? LONG(window->resizeInset + 0.5f)
			: GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
		const LONG insetY = window && window->resizeInset > 0.f ? LONG(window->resizeInset + 0.5f)
			: GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
		switch (msg) {
		case WM_NCCALCSIZE:
			// Handle both NCCALCSIZE forms. Letting the FALSE form reach DefWindowProc can
			// restore default non-client sizing during maximize/restore and expose native chrome.
			if (window && window->customChrome) {
				RECT* rect = wp
					? &reinterpret_cast<NCCALCSIZE_PARAMS*>(lp)->rgrc[0]
					: reinterpret_cast<RECT*>(lp);
				if (IsZoomed(hwnd)) {
					// A maximized borderless window should occupy the monitor work area, not cover
					// the taskbar or retain an invisible resize-frame strip.
					MONITORINFO monitor{ sizeof(MONITORINFO) };
					if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
						*rect = monitor.rcWork;
=======
		switch (msg) {
		case WM_NCCALCSIZE:
			// Returning 0 with wParam TRUE makes the whole window client area (no native frame).
			// When maximized, inset by the hidden frame so content stays inside the work area.
			if (window && window->customChrome && wp) {
				if (IsZoomed(hwnd)) {
					auto* rect = reinterpret_cast<RECT*>(lp);
					const LONG frameX = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
					const LONG frameY = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
					rect->left += frameX; rect->top += frameY;
					rect->right -= frameX; rect->bottom -= frameY;
>>>>>>> d262d3498d1532fcaf2d920c57fbc4ddc03ae8e3
				}
				return 0;
			}
			break;
<<<<<<< HEAD
		case WM_NCACTIVATE:
			// Native activation must not repaint a caption for our client-drawn frame.
			if (window && window->customChrome) return TRUE;
			break;
=======
>>>>>>> d262d3498d1532fcaf2d920c57fbc4ddc03ae8e3
		case WM_NCHITTEST:
			if (window && window->customChrome) {
				// Resize borders behave like a native frame; the rest stays client area so the
				// runtime keeps receiving mouse input for the custom TitleBar.
				if (!IsZoomed(hwnd) && window->native && glfwGetWindowAttrib(window->native, GLFW_RESIZABLE) == GLFW_TRUE) {
					const POINT pt{ static_cast<LONG>(static_cast<short>(LOWORD(lp))), static_cast<LONG>(static_cast<short>(HIWORD(lp))) };
					RECT rc{};
					GetWindowRect(hwnd, &rc);
					const LONG x = pt.x - rc.left, y = pt.y - rc.top;
					const LONG width = rc.right - rc.left, height = rc.bottom - rc.top;
<<<<<<< HEAD
					const bool left = x < insetX, right = x >= width - insetX, top = y < insetY, bottom = y >= height - insetY;
=======
					const LONG frameX = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
					const LONG frameY = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
					const bool left = x < frameX, right = x >= width - frameX, top = y < frameY, bottom = y >= height - frameY;
>>>>>>> d262d3498d1532fcaf2d920c57fbc4ddc03ae8e3
					if (top && left) return HTTOPLEFT;
					if (top && right) return HTTOPRIGHT;
					if (bottom && left) return HTBOTTOMLEFT;
					if (bottom && right) return HTBOTTOMRIGHT;
					if (left) return HTLEFT;
					if (right) return HTRIGHT;
					if (top) return HTTOP;
					if (bottom) return HTBOTTOM;
				}
				return HTCLIENT;
			}
			break;
		default: break;
		}
		return CallWindowProc(state && state->prevProc ? state->prevProc : DefWindowProcW, hwnd, msg, wp, lp);
	}
}

void GlfwPlatform::installChrome(Window& window) {
	HWND hwnd = glfwGetWin32Window(window.native);
	if (!hwnd || window.chromeState) return;
	auto* state = new ChromeState{ &window, nullptr };
	state->prevProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&chromeWndProc)));
	SetPropW(hwnd, kChromeProp, state);
	window.chromeState = state;
	window.customChrome = true;
	applySnapStyles(hwnd);
}

void GlfwPlatform::removeChrome(Window& window) {
	if (!window.chromeState) return;
	if (HWND hwnd = window.native ? glfwGetWin32Window(window.native) : nullptr) {
		if (auto* state = static_cast<ChromeState*>(GetPropW(hwnd, kChromeProp))) {
			SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(state->prevProc));
			RemovePropW(hwnd, kChromeProp);
			delete state;
		}
	}
	window.chromeState = nullptr;
	window.customChrome = false;
}
#endif
