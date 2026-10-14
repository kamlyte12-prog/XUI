#include "gl_drawlist_renderer.hpp"
#include "glfw_platform.hpp"
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

void onUiEvent(void* context, std::string_view name) {
    auto& action = *static_cast<ActionContext*>(context);
    xui::Xui& app = *action.app;
    if (name == "onAction") app.setWindowTitle("Xui showcase - action " + std::to_string(++action.actions));
    else if (name == "onNameChange") {
        if (const auto value = valueAs<std::string>(app, "display-name")) app.setWindowTitle("Name: " + *value);
    } else if (name == "onNotesChange") {
        if (const auto value = valueAs<std::string>(app, "notes")) app.setWindowTitle("Notes updated (" + std::to_string(value->size()) + " UTF-8 bytes)");
    } else if (name == "onCheckChange") {
        if (const auto value = valueAs<bool>(app, "updates")) app.setWindowTitle(*value ? "Updates enabled" : "Updates disabled");
    } else if (name == "onSliderChange") {
        if (const auto value = valueAs<xui::f64>(app, "volume")) {
            const std::string label = "Volume: " + std::to_string(static_cast<int>(*value * 100.0 + 0.5)) + "%";
            app.runtime().setValue("volume-readout", label);
            app.setWindowTitle(label);
        }
    } else if (name == "onDensityChange") {
        if (const auto value = valueAs<std::string>(app, "density")) app.setWindowTitle("Density: " + *value);
    } else if (name == "onColorChange") {
        if (const auto value = valueAs<xui::Color>(app, "accent-color")) {
            char rgba[9]; std::snprintf(rgba, sizeof(rgba), "%08x", value->toRGBA8());
            app.setWindowTitle("Accent color: #" + std::string(rgba));
        }
    } else if (name == "onTabChange") {
        if (const auto value = valueAs<xui::u32>(app, "demo-sections")) app.setWindowTitle("Showcase section " + std::to_string(*value + 1));
    } else if (name == "onTaskLayout" || name == "onTaskStyles" || name == "onTaskBackend" || name == "onTaskResize") {
        const std::string_view id = name == "onTaskLayout" ? "task-layout" : name == "onTaskStyles" ? "task-styles" :
                                    name == "onTaskBackend" ? "task-backend" : "task-resize";
        if (const auto value = valueAs<bool>(app, id)) app.setWindowTitle(std::string(id) + (*value ? " completed" : " unchecked"));
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
    options.minWidth = 760; options.minHeight = 560;
    options.visible = !profile;
    options.glContext = true;
    options.vsync = false;

    xui::Xui app(std::move(options));
    std::filesystem::path directory = argc > 0 && argv[0] ? std::filesystem::absolute(argv[0]).parent_path()
                                                            : std::filesystem::current_path();
    if (!loadFirstFont(app) ||
        !app.loadXui((directory / "showcase.xui").string()) ||
        !app.loadXss((directory / "showcase.xss").string())) {
        std::cerr << app.lastError() << '\n';
        return 1;
    }

    GlDrawListRenderer renderer;
    GlfwPlatform platform;
    ActionContext action{&app, 0};
    app.setEventHandler(onUiEvent, &action);
    if (!app.attach(renderer, platform)) {
        std::cerr << app.lastError() << '\n';
        return 1;
    }

    // The showcase keeps the FPS counter visible from its first frame; F3 still toggles it.
    app.runtime().keyDown(xui::Key::F3);

    const int result = profile ? app.run({5.0, true}) : app.run();
    if (profile) {
        const auto& stats = app.lastRunStats();
        std::cout << "Xui profile: " << stats.frames << " frames in " << stats.elapsedSeconds
                  << " s (" << stats.framesPerSecond << " FPS)\n";
    }
    if (result != 0) std::cerr << app.lastError() << '\n';
    return result;
}
