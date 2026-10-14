#pragma once
/// @file image_io.hpp  Minimal dependency-free PNG (uncompressed deflate) writer/reader and image comparison, for golden tests.
/// The reader only understands what the writer produces (8-bit RGBA, filters 0/2, stored/fixed-Huffman blocks); it is not a general PNG decoder.
#include <xui/core/types.hpp>
#include <string>
#include <vector>

namespace xui {
struct Image {
    u32 width = 0, height = 0;
    std::vector<u8> rgba;                       ///< straight (non-premultiplied) RGBA8, row-major
    u8* px(u32 x, u32 y) { return &rgba[(std::size_t(y) * width + x) * 4]; }
    const u8* px(u32 x, u32 y) const { return &rgba[(std::size_t(y) * width + x) * 4]; }
};
bool writePng(const std::string& path, const Image& img);
bool readPng(const std::string& path, Image& out);

struct ImageDiff {
    bool sizeMismatch = false;
    u32 badPixels = 0;        ///< pixels where any channel differs by more than the tolerance
    u32 maxDelta = 0;         ///< largest channel difference anywhere
    u32 firstX = 0, firstY = 0;
};
ImageDiff compareImages(const Image& a, const Image& b, u32 tolerance);
/// Visualise differences: matching pixels dimmed, differing pixels red.
Image diffImage(const Image& a, const Image& b, u32 tolerance);
}
