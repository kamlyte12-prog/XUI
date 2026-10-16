#include <opengl/gl_drawlist_renderer.hpp>
#include <glfw/glfw_platform.hpp>
#include <xui/render/image_io.hpp>
#include <xui/xui.hpp>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
	struct ActionContext { xui::Xui* app = nullptr; xui::u32 actions = 0; };

	template <typename T>
	std::optional<T> valueAs(xui::Xui& app, std::string_view id) {
		const auto value = app.runtime().value(id);
		if (!value) return std::nullopt;
		if (const T* typed = std::get_if<T>(&*value)) return *typed;
		return std::nullopt;
	}

	void setText(xui::Xui& app, std::string_view id, std::string value) {
		app.runtime().setValue(id, std::move(value));
	}

	std::string checklistSummary(xui::Xui& app) {
		xui::u32 checked = 0, total = 0;
		for (const std::string_view id : { "task-layout", "task-styles", "task-backend", "task-resize" }) {
			if (const auto value = valueAs<bool>(app, id)) { ++total; checked += *value ? 1u : 0u; }
		}
		return "Tasks checked: " + std::to_string(checked) + " / " + std::to_string(total);
	}

	void onUiEvent(void* context, std::string_view name) {
		auto& action = *static_cast<ActionContext*>(context);
		xui::Xui& app = *action.app;
		if (name == "onAction") {
			setText(app, "action-readout", "Action count: " + std::to_string(++action.actions));
			app.setWindowTitle("Xui showcase - action " + std::to_string(action.actions));
		}
		else if (name == "onNameChange") {
			if (const auto value = valueAs<std::string>(app, "display-name")) setText(app, "name-readout", value->empty() ? "(empty)" : *value);
		}
		else if (name == "onNotesChange") {
			if (const auto value = valueAs<std::string>(app, "notes")) setText(app, "notes-readout", value->empty() ? "(empty)" : *value);
		}
		else if (name == "onCheckChange") {
			if (const auto value = valueAs<bool>(app, "updates")) setText(app, "check-readout", *value ? "Updates: on" : "Updates: off");
		}
		else if (name == "onSliderChange") {
			if (const auto value = valueAs<xui::f64>(app, "volume")) {
				const int percent = static_cast<int>(*value * 100.0 + 0.5);
				setText(app, "volume-readout", "Volume: " + std::to_string(percent) + "%");
				app.runtime().setValue("progress-demo", *value);
				setText(app, "progress-readout", "Progress: " + std::to_string(percent) + "%");
			}
		}
		else if (name == "onDensityChange") {
			if (const auto value = valueAs<std::string>(app, "density")) setText(app, "density-readout", "Density: " + *value);
		}
		else if (name == "onColorChange") {
			if (const auto value = valueAs<xui::Color>(app, "accent-color")) {
				char rgba[9]; std::snprintf(rgba, sizeof(rgba), "%08X", value->toRGBA8());
				setText(app, "color-readout", "Accent: #" + std::string(rgba));
			}
		}
		else if (name == "onTabChange") {
			if (const auto value = valueAs<xui::u32>(app, "demo-sections")) {
				setText(app, "tab-readout", "Section " + std::to_string(*value + 1));
				app.setWindowTitle("Showcase section " + std::to_string(*value + 1));
			}
		}
		else if (name == "onTaskLayout" || name == "onTaskStyles" || name == "onTaskBackend" || name == "onTaskResize") {
			setText(app, "tasks-readout", checklistSummary(app));
		}
		else if (name == "onSearchChange") {
			// SearchField: string value by id; the clear glyph and Escape both fire change with "".
			if (const auto value = valueAs<std::string>(app, "query")) {
				app.runtime().setValue("search-readout", value->empty() ? std::string("(empty)") : *value);
				app.setWindowTitle(value->empty() ? "Xui showcase" : "Search: " + *value);
			}
		}
		else if (name == "onPassphraseChange") {
			// Password field: the value API reads the stored text; the demo only shows its length.
			if (const auto value = valueAs<std::string>(app, "passphrase"))
				setText(app, "password-readout", "Password length: " + std::to_string(value->size()));
		}
		else if (name == "onNumberChange") {
			// NumberField: committed f64 by id, clamped to min/max and stepped by Up/Down or the steppers.
			if (const auto value = valueAs<xui::f64>(app, "delay"))
				setText(app, "number-readout", "Delay: " + std::to_string(static_cast<int>(*value)) + " ms");
		}
		else if (name == "onSwitchChange") {
			if (const auto value = valueAs<bool>(app, "live-sync"))
				setText(app, "switch-readout", std::string("Live sync: ") + (*value ? "on" : "off"));
		}
		else if (name == "onToggleChange") {
			if (const auto value = valueAs<bool>(app, "mute"))
				setText(app, "toggle-readout", std::string("Mute: ") + (*value ? "on" : "off"));
		}
		else if (name == "onRadioChange") {
			if (const auto value = valueAs<std::string>(app, "density-choice"))
				setText(app, "radio-readout", "Density: " + *value);
		}
		else if (name == "onWhitespaceToggle") {
			// Debug aid: spaces render as a middle dot, tabs as a right arrow inside editable fields.
			if (const auto value = valueAs<bool>(app, "whitespace-toggle")) app.runtime().setWhitespaceVisible(*value);
		}
		else if (name == "onShadingChange") {
			if (const auto value = valueAs<std::string>(app, "shading")) setText(app, "shading-readout", "Shading: " + *value);
		}
		else if (name == "onRatingChange") {
			if (const auto value = valueAs<xui::f64>(app, "priority"))
				setText(app, "rating-readout", "Priority: " + std::to_string(static_cast<int>(*value)) + " / 5");
		}
		else if (name == "onSubdivChange") {
			if (const auto value = valueAs<xui::f64>(app, "subdivisions"))
				setText(app, "subdiv-readout", "Subdivisions: " + std::to_string(static_cast<int>(*value)));
		}
		else if (name == "onDateChange") {
			if (const auto value = valueAs<std::string>(app, "due-date")) setText(app, "date-readout", "Date: " + *value);
		}
		else if (name == "onKeybindChange") {
			if (const auto value = valueAs<std::string>(app, "save-key"))
				setText(app, "keybind-readout", "Save key: " + (value->empty() ? std::string("(none)") : *value));
		}
		else if (name == "onTrailChange") {
			if (const auto value = valueAs<std::string>(app, "trail")) {
				// Rebuild the visible path up to the selected segment from the option list.
				static const char* options[] = { "Project", "Assets", "Textures", "Wood" };
				std::string path;
				for (const char* option : options) { path += path.empty() ? option : std::string(" > ") + option; if (std::string_view(option) == *value) break; }
				setText(app, "trail-readout", "Path: " + path);
			}
		}
		else if (name == "onScrollChanged") {
			if (const auto offset = valueAs<xui::Vec2>(app, "vertical-scroll")) {
				app.runtime().setValue("scroll-position-readout", "Vertical offset: " +
					std::to_string(static_cast<int>(offset->x)) + ", " + std::to_string(static_cast<int>(offset->y)));
			}
		}
		else if (name == "onScrollToTarget") {
			app.runtime().scrollIntoView("scroll-target-last", true);
		}
		else if (name == "onDockChange") {
			std::string summary = "Dock state: ";
			for (const std::string_view id : { "dock-editor", "dock-hierarchy", "dock-inspector", "dock-console", "dock-preview" }) {
				const auto value = app.runtime().value(id);
				if (!value) continue;
				const auto* panel = std::get_if<xui::DockPanelValue>(&*value);
				if (!panel) continue;
				std::string_view region = "center";
				if (panel->floating) region = "floating";
				else if (panel->region == xui::DockZone::Left) region = "left";
				else if (panel->region == xui::DockZone::Right) region = "right";
				else if (panel->region == xui::DockZone::Top) region = "top";
				else if (panel->region == xui::DockZone::Bottom) region = "bottom";
				if (id == "dock-editor") summary += "Editor ";
				else if (id == "dock-hierarchy") summary += "Hierarchy ";
				else if (id == "dock-inspector") summary += "Inspector ";
				else if (id == "dock-console") summary += "Console ";
				else summary += "Preview ";
				summary += region;
				if (panel->active) summary += " (active)";
				summary += "; ";
			}
			setText(app, "dock-readout", summary);
		}
	}

	std::vector<std::filesystem::path> fontCandidates() {
		std::vector<std::filesystem::path> paths;
		if (const char* env = std::getenv("XUI_DEMO_FONT")) paths.emplace_back(env);
		paths.emplace_back("assets/DejaVuSans.ttf");
#ifdef _WIN32
		paths.emplace_back("C:/Windows/Fonts/segoeui.ttf");
		paths.emplace_back("C:/Windows/Fonts/arial.ttf");
#elif defined(__APPLE__)
		paths.emplace_back("/System/Library/Fonts/Supplemental/Arial.ttf");
		paths.emplace_back("/System/Library/Fonts/SFNS.ttf");
#else
		paths.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
		paths.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf");
#endif
		return paths;
	}

	bool loadFirstFont(xui::Xui& app) {
		for (const auto& path : fontCandidates())
			if (app.loadFont(path.string())) return true;
		return false;
	}
}

int main(int argc, char** argv) {
	bool profile = false;
	for (int i = 1; i < argc; ++i) {
		profile |= std::string_view(argv[i]) == "--profile";
	}
	xui::XuiOptions options;
	options.appName = "xui-glfw-demo";
	options.windowTitle = "Xui GLFW/OpenGL widget showcase";
	options.width = 1080; options.height = 760;
	options.minWidth = 900; options.minHeight = 640;   // keeps the three-field input row unclipped
	options.visible = !profile;
	options.glContext = true;
	options.vsync = false;

	xui::Xui app(std::move(options));
	std::filesystem::path directory = argc > 0 && argv[0] ? std::filesystem::absolute(argv[0]).parent_path()
		: std::filesystem::current_path();
	// The Image widget resolves its source against the process working directory, so generate the
	// demo asset there once. It uses the project's own PNG writer (see doc/widgets.md for the
	// decoder limits).
	if (!std::filesystem::exists("xui-demo-image.png")) {
		xui::Image image;
		image.width = 96;
		image.height = 64;
		image.rgba.assign(std::size_t(image.width) * image.height * 4, 0);
		for (xui::u32 y = 0; y < image.height; ++y)
			for (xui::u32 x = 0; x < image.width; ++x) {
				xui::u8* pixel = image.px(x, y);
				pixel[0] = static_cast<xui::u8>(40 + x * 2);
				pixel[1] = static_cast<xui::u8>(60 + y * 3);
				pixel[2] = static_cast<xui::u8>(160 + x);
				pixel[3] = 255;
			}
		xui::writePng("xui-demo-image.png", image);
	}
	if (!loadFirstFont(app) ||
		!app.loadXui((directory / "showcase.xui").string()) ||
		!app.loadXss((directory / "showcase.xss").string())) {
		std::cerr << app.lastError() << '\n';
		return 1;
	}

	GlDrawListRenderer renderer;
	GlfwPlatform platform;
	ActionContext action{ &app, 0 };
	app.setEventHandler(onUiEvent, &action);
	if (!app.attach(renderer, platform)) {
		std::cerr << app.lastError() << '\n';
		return 1;
	}

	// The showcase keeps the FPS counter visible from its first frame; F3 still toggles it.
	app.runtime().keyDown(xui::Key::F3);

	const int result = profile ? app.run({ 5.0, true }) : app.run();
	if (profile) {
		const auto& stats = app.lastRunStats();
		std::cout << "Xui profile: " << stats.frames << " frames in " << stats.elapsedSeconds
			<< " s (" << stats.framesPerSecond << " FPS)\n";
	}
	if (result != 0) std::cerr << app.lastError() << '\n';
	return result;
}
