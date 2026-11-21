#include <xui/xui.hpp>

#include <cstdio>

int main() {
	xui::Xui ui;
	if (!ui.lastError().empty()) {
		std::printf("Xui package consumer: %s\n", ui.lastError().c_str());
		return 1;
	}
	std::printf("Xui package consumer: ok\n");
	return 0;
}
