#pragma once
#include <xui/platform/platform.hpp>
#include <GLFW/glfw3.h>
#include <deque>
#include <unordered_map>

/// GLFW implementation of Xui's platform boundary. Application code consumes Platform events and
/// never needs to call GLFW or a native OS API.
class GlfwPlatform final : public xui::Platform {
public:
	bool init(const xui::PlatformDesc&) override;
	void shutdown() override;
	xui::WindowHandle createWindow(const xui::WindowDesc&) override;
	void destroyWindow(xui::WindowHandle) override;
	void setWindowTitle(xui::WindowHandle, std::string_view) override;
	xui::Vec2 windowSizeLogical(xui::WindowHandle) const override;
	xui::Vec2i windowSizePixels(xui::WindowHandle) const override;
	xui::f32 dpiScale(xui::WindowHandle) const override;
	bool pumpEvents(xui::Event&) override;
	void waitEvents(xui::f64 timeoutSec) override;
	xui::f64 time() const override;
	void* nativeWindowHandle(xui::WindowHandle) override;
	void* nativeGlContext(xui::WindowHandle) override;
	void swapBuffers(xui::WindowHandle) override;
	void makeGlCurrent(xui::WindowHandle) override;
	void setCursor(xui::CursorShape) override;
	void setClipboardText(std::string_view) override;
	std::string clipboardText() const override;
	void requestRedraw(xui::WindowHandle) override;
	void setLiveResizeHook(xui::WindowHandle, void (*fn)(void*), void*) override;
private:
	struct Window { GLFWwindow* native = nullptr; xui::WindowHandle handle; GlfwPlatform* owner = nullptr;
	                void (*liveResizeFn)(void*) = nullptr; void* liveResizeContext = nullptr; };
	static Window* record(GLFWwindow* window);
	static void push(GLFWwindow* window, const xui::Event& event);
	static void onSize(GLFWwindow*, int, int);
	static void onCursor(GLFWwindow*, double, double);
	static void onScroll(GLFWwindow*, double, double);
	static void onMouse(GLFWwindow*, int, int, int);
	static void onKey(GLFWwindow*, int, int, int, int);
	static void onChar(GLFWwindow*, unsigned int);
	static void onClose(GLFWwindow*);
	static xui::Key mapKey(int key);
	Window* find(xui::WindowHandle);
	const Window* find(xui::WindowHandle) const;
	std::deque<Window> m_windows;
	std::deque<xui::Event> m_events;
	GLFWcursor* m_cursor = nullptr;
	xui::CursorShape m_cursorShape = xui::CursorShape::Arrow;
	bool m_cursorShapeSet = false;
	GLFWwindow* m_activeWindow = nullptr;
	// glfwPollEvents() invokes callbacks synchronously. Poll at most once while the caller
	// drains this batch so continuous native resize events cannot starve the next redraw.
	bool m_polledForCurrentDrain = false;
};
