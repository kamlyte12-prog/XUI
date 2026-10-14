#pragma once
// Golden-image checking. Missing goldens FAIL (so CI cannot pass vacuously). XUI_UPDATE_GOLDEN=1 (re)generates them.
// On mismatch the actual and a diff image are written to $XUI_GOLDEN_OUT (default /tmp/xui_golden_out) for review.
#include "xtest.hpp"
#include <xui/render/image_io.hpp>
#include <cstdlib>
#include <filesystem>
#include <string>
#ifndef XUI_GOLDEN_DIR
#define XUI_GOLDEN_DIR "tests/golden"
#endif
inline void checkGolden(const char* name, const xui::Image& img, xui::u32 tolerance = 3, double maxBadFraction = 0.001) {
    const std::string path = std::string(XUI_GOLDEN_DIR) + "/" + name + ".png";
    if (std::getenv("XUI_UPDATE_GOLDEN")) { xui::writePng(path, img); std::printf("  updated golden %s\n", name); return; }
    xui::Image g;
    if (!xui::readPng(path, g)) {
        ++xtest::failures();
        const char* out = std::getenv("XUI_GOLDEN_OUT"); const std::string dir = out ? out : "/tmp/xui_golden_out";
        std::error_code ec; std::filesystem::create_directories(dir, ec);
        const bool wrote = xui::writePng(dir + "/" + name + ".actual.png", img);
        std::printf("  golden '%s' is missing or unreadable (%s)\n    review %s%s, then regenerate with XUI_UPDATE_GOLDEN=1\n", name, path.c_str(), wrote ? dir.c_str() : "(could not write)", wrote ? (std::string("/") + name + ".actual.png").c_str() : "");
        return;
    }
    const xui::ImageDiff d = xui::compareImages(img, g, tolerance);
    const double frac = d.sizeMismatch ? 1.0 : double(d.badPixels) / (double(img.width) * img.height);
    if (d.sizeMismatch || frac > maxBadFraction) {
        ++xtest::failures();
        const char* out = std::getenv("XUI_GOLDEN_OUT"); const std::string dir = out ? out : "/tmp/xui_golden_out";
        std::error_code ec; std::filesystem::create_directories(dir, ec);
        const bool wroteActual = xui::writePng(dir + "/" + name + ".actual.png", img);
        const bool wroteDiff = !d.sizeMismatch && xui::writePng(dir + "/" + name + ".diff.png", xui::diffImage(img, g, tolerance));
        std::printf("  golden '%s' differs: %u bad pixels (first at %u,%u), max channel delta %u%s\n", name, d.badPixels, d.firstX, d.firstY, d.maxDelta, d.sizeMismatch ? ", SIZE MISMATCH" : "");
        if (wroteActual) std::printf("    actual: %s/%s.actual.png%s\n", dir.c_str(), name, wroteDiff ? " (and .diff.png)" : "");
        else std::printf("    (could not write review images to %s)\n", dir.c_str());
    }
}
#define CHECK_GOLDEN(name, img) checkGolden(name, img)
