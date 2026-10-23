#include "opengl/gl_drawlist_renderer.hpp"
#include "glfw/glfw_platform.hpp"
#include <xui/xui.hpp>
#include <print>

int main() {
	GlDrawListRenderer renderer;
	GlfwPlatform platform;
	xui::Xui ui;

	if (!ui.loadFont("Roboto.ttf", "Roboto") ||
		!ui.loadXui("layout.xui") ||
		!ui.loadXss("layout.xss")) {
		std::print("{}\n", ui.lastError());
		return 1;
	}

	if (!ui.attach(renderer, platform)) {
		std::print("{}\n", ui.lastError());
		return 1;
	}

	return ui.run();
}