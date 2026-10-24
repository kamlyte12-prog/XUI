#pragma once
/// @file image_io.hpp  Image pixel storage, fixture/golden PNG I/O, and optional image decoding.
#include <xui/core/types.hpp>
#include <string>
#include <string_view>
#include <memory>
#include <vector>

namespace xui {
	struct Image {
		u32 width = 0, height = 0;
		std::vector<u8> rgba;                       ///< straight (non-premultiplied) RGBA8, row-major
		u8* px(u32 x, u32 y) { return &rgba[(std::size_t(y) * width + x) * 4]; }
		const u8* px(u32 x, u32 y) const { return &rgba[(std::size_t(y) * width + x) * 4]; }
	};

	/// True when the optional bundled stb_image/LunaSVG resource loaders were enabled at build time.
	bool imageSupportAvailable();
	bool isSvgImagePath(std::string_view path);
	/// Decode a static PNG, JPEG, BMP, or TGA into straight-alpha, top-left RGBA8 pixels.
	bool readImage(const std::string& path, Image& out, std::string* error = nullptr);

	/// Parsed SVG document. Keep one per authored Image and only rasterize again when its display
	/// size/DPI bucket changes; decoded pixels are uploaded using the ordinary Renderer texture API.
	class SvgImage {
	public:
		SvgImage();
		~SvgImage();
		SvgImage(SvgImage&&) noexcept;
		SvgImage& operator=(SvgImage&&) noexcept;
		SvgImage(const SvgImage&) = delete;
		SvgImage& operator=(const SvgImage&) = delete;

		bool load(const std::string& path, std::string* error = nullptr);
		bool rasterize(u32 targetWidthPx, u32 targetHeightPx, bool cover, Image& out,
			std::string* error = nullptr) const;
		f32 intrinsicWidth() const;
		f32 intrinsicHeight() const;
		bool valid() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
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
