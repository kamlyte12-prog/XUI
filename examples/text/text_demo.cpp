#include "opengl/gl_drawlist_renderer.hpp"
#include "glfw/glfw_platform.hpp"
#include <xui/xui.hpp>
#include <print>

int main() {
	GlDrawListRenderer renderer;
	GlfwPlatform platform;
	xui::Xui ui;

	if (!ui.loadFont("C:/Windows/Fonts/comic.ttf", "comic") ||
		!ui.loadXui("text.xui") ||
		!ui.loadXss("text.xss")) {
		std::print("{}\n", ui.lastError());
		return 1;
	}

	if (!ui.attach(renderer, platform)) {
		std::print("{}\n", ui.lastError());
		return 1;
	}

	return ui.run();
}