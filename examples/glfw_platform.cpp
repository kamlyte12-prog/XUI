#include "glfw_platform.hpp"
#include <algorithm>
#include <cmath>

using namespace xui;

bool GlfwPlatform::init(const PlatformDesc&) { return glfwInit() == GLFW_TRUE; }
void GlfwPlatform::shutdown() {
	for (Window& w : m_windows) if (w.native) glfwDestroyWindow(w.native);
	m_windows.clear();
	if (m_cursor) glfwDestroyCursor(m_cursor);
	m_cursor = nullptr;
	m_cursorShapeSet = false;
	m_activeWindow = nullptr;
	glfwTerminate();
}

WindowHandle GlfwPlatform::createWindow(const WindowDesc& d) {
	glfwWindowHint(GLFW_RESIZABLE, d.resizable ? GLFW_TRUE : GLFW_FALSE);
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
	glfwSetScrollCallback(native, onScroll);
	glfwSetMouseButtonCallback(native, onMouse);
	glfwSetKeyCallback(native, onKey);
	glfwSetCharCallback(native, onChar);
	glfwSetWindowCloseCallback(native, onClose);
	if (!d.visible) glfwHideWindow(native);
	return handle;
}

void GlfwPlatform::destroyWindow(WindowHandle h) {
	Window* w = find(h); if (!w) return;
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
	if (m_cursor) { glfwDestroyCursor(m_cursor); m_cursor = nullptr; }
	if (shape != CursorShape::Arrow) m_cursor = glfwCreateStandardCursor(id);
	if (m_activeWindow) glfwSetCursor(m_activeWindow, m_cursor);
}
void GlfwPlatform::setClipboardText(std::string_view s) { glfwSetClipboardString(nullptr, std::string(s).c_str()); }
std::string GlfwPlatform::clipboardText() const { const char* s = glfwGetClipboardString(nullptr); return s ? s : ""; }
void GlfwPlatform::requestRedraw(WindowHandle h) { if (auto* w = find(h)) glfwPostEmptyEvent(); }

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
