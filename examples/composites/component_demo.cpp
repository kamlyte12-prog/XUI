#include <glfw/glfw_platform.hpp>
#include <opengl/gl_drawlist_renderer.hpp>
#include <xui/xui.hpp>
#include <cstdio>

namespace {
	void onUiEvent(void*, std::string_view name) {
		if (name == "onContinue") std::puts("Card footer action: onContinue");
	}
}

int main() {
	GlDrawListRenderer renderer;
	GlfwPlatform platform;
	xui::Xui ui;
	ui.setEventHandler(onUiEvent, nullptr);
	if (!ui.loadXui("main.xui") || !ui.loadXss("main.xss")) {
		std::fprintf(stderr, "%s\n", ui.lastError().c_str());
		return 1;
	}
	if (!ui.attach(renderer, platform)) {
		std::fprintf(stderr, "%s\n", ui.lastError().c_str());
		return 1;
	}
	return ui.run();
}
