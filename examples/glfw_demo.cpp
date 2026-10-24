#include <opengl/gl_drawlist_renderer.hpp>
#include <glfw/glfw_platform.hpp>
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
		const auto selected = valueAs<xui::CheckListValue>(app, "setup-tasks");
		return "Tasks checked: " + std::to_string(selected ? selected->size() : 0u) + " / 4";
	}

	void onUiEvent(void* context, std::string_view name) {
		auto& action = *static_cast<ActionContext*>(context);
		xui::Xui& app = *action.app;
		if (name == "onAction") {
			setText(app, "action-readout", "Action count: " + std::to_string(++action.actions));
			app.setWindowTitle("Xui showcase - action " + std::to_string(action.actions));
		}
		else if (name == "onMeterOnce") app.playAnimation("event-meter", xui::AnimationMode::Once);
		else if (name == "onMeterLoop") app.playAnimation("event-meter", xui::AnimationMode::Loop);
		else if (name == "onMeterStop") app.cancelAnimation("event-meter");
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
		else if (name == "onTasksChange") {
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
		else if (name == "onMenuAction") {
			// Popup menu activation: the anchored menu reports its last selected item through the
			// value API; context-menu activations surface through the same handler.
			std::string picked;
			if (const auto value = valueAs<std::string>(app, "file-menu")) picked = *value;
			if (const auto value = valueAs<std::string>(app, "tab-context"); value && !value->empty()) picked = *value;
			setText(app, "action-readout", "Menu: " + (picked.empty() ? std::string("(none)") : picked));
			app.setWindowTitle("Xui showcase - " + (picked.empty() ? std::string("menu") : picked));
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

	bool loadBoldFont(xui::Xui& app) {
		std::vector<std::filesystem::path> paths;
		if (const char* env = std::getenv("XUI_DEMO_BOLD_FONT")) paths.emplace_back(env);
#ifdef _WIN32
		paths.emplace_back("C:/Windows/Fonts/segoeuib.ttf");
		paths.emplace_back("C:/Windows/Fonts/arialbd.ttf");
#elif defined(__APPLE__)
		paths.emplace_back("/System/Library/Fonts/Supplemental/Arial Bold.ttf");
		paths.emplace_back("/System/Library/Fonts/SFNS.ttf");
#else
		paths.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
		paths.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf");
#endif
		for (const auto& path : paths)
			if (app.loadFont(path.string(), "default", 700)) return true;
		return false; // unavailable weights resolve to the nearest registered face.
	}
}

int main(int argc, char** argv) {
	std::string profileCase;
	for (int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if (arg == "--profile") profileCase = "idle";
		else if (arg == "--profile-glow") profileCase = "glow";
		else if (arg == "--profile-animation") profileCase = "animation";
		else if (arg == "--profile-animation-timer") profileCase = "animation-timer";
		else if (arg == "--profile-transform") profileCase = "transform";
		else if (arg == "--profile-docking") profileCase = "docking";
		else if (arg == "--profile-menubar") profileCase = "menubar";
		else if (arg == "--profile-images") profileCase = "images";
	}
	const bool profile = !profileCase.empty();
	xui::XuiOptions options;
	options.appName = "xui-glfw-demo";
	options.windowTitle = "Xui GLFW/OpenGL widget showcase";
	options.width = 1080; options.height = 760;
	options.minWidth = 900; options.minHeight = 640;   // keeps the three-field input row unclipped
	// Profiles need a visible window so the runtime's minimize/hidden animation policy does not
	// suppress rendering and report a misleading zero-frame result.
	options.visible = true;
	options.glContext = true;
	options.vsync = false;

	xui::Xui app(std::move(options));
	std::filesystem::path directory = argc > 0 && argv[0] ? std::filesystem::absolute(argv[0]).parent_path()
		: std::filesystem::current_path();
	if (!loadFirstFont(app)) {
		std::cerr << "No usable demo font was found. Set XUI_DEMO_FONT to a TTF file.\n";
		return 1;
	}
	(void)loadBoldFont(app);
	if (!app.loadXui((directory / "showcase.xui").string()) ||
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
	if (profileCase == "glow" || profileCase == "animation" || profileCase == "animation-timer" || profileCase == "transform") {
		app.runtime().setValue("demo-sections", xui::u32{ 7 });
		app.runtime().setValue("motion-tabs", xui::u32{ profileCase == "glow" ? 0u : 1u });
		app.runtime().layout(1080.f, 760.f);
	}
	else if (profileCase == "docking") {
		app.runtime().setValue("demo-sections", xui::u32{ 6 });
		for (const auto [id, bounds] : { std::pair{ "dock-hierarchy", xui::Rect{ 70.f, 110.f, 300.f, 240.f } },
			std::pair{ "dock-inspector", xui::Rect{ 200.f, 170.f, 300.f, 240.f } } }) {
			if (auto value = app.runtime().value(id)) {
				if (auto panel = std::get_if<xui::DockPanelValue>(&*value)) {
					xui::DockPanelValue floating = *panel;
					floating.floating = true; floating.bounds = bounds;
					app.runtime().setValue(id, floating);
				}
			}
		}
		app.runtime().layout(1080.f, 760.f);
	}
	else if (profileCase == "menubar") {
		// Keep the normal top-level page selected; the client-area menu is always visible.
		app.runtime().layout(1080.f, 760.f);
	}
	else if (profileCase == "images") {
		// Select Display so both checked-in SVG Image widgets load through the real renderer.
		app.runtime().setValue("demo-sections", xui::u32{ 4 });
	}
	const bool continuousProfile = profile && profileCase != "animation-timer";
	const int result = profile ? app.run({ 5.0, continuousProfile }) : app.run();
	if (profile) {
		const auto& stats = app.lastRunStats();
		std::cout << "Xui " << profileCase << " profile: " << stats.frames << " frames in " << stats.elapsedSeconds
			<< " s (" << stats.framesPerSecond << " FPS)\n";
	}
	if (result != 0) std::cerr << app.lastError() << '\n';
	return result;
}
