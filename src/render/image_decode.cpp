#include <xui/render/image_io.hpp>

#ifndef XUI_ENABLE_IMAGE_SUPPORT
#define XUI_ENABLE_IMAGE_SUPPORT 0
#endif

#if XUI_ENABLE_IMAGE_SUPPORT
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <lunasvg.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>

namespace xui {
namespace {
constexpr u32 kMaxDimension = 16384;
constexpr u64 kMaxPixels = 64ull * 1024ull * 1024ull;

bool acceptableSize(u32 width, u32 height) {
	return width > 0 && height > 0 && width <= kMaxDimension && height <= kMaxDimension &&
		static_cast<u64>(width) * height <= kMaxPixels;
}

std::string lowerExtension(std::string_view path) {
	const std::size_t dot = path.find_last_of('.');
	if (dot == std::string_view::npos) return {};
	std::string extension(path.substr(dot));
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return extension;
}
}

bool imageSupportAvailable() {
#if XUI_ENABLE_IMAGE_SUPPORT
	return true;
#else
	return false;
#endif
}

bool isSvgImagePath(std::string_view path) { return lowerExtension(path) == ".svg"; }

bool readImage(const std::string& path, Image& out, std::string* error) {
	out = {};
#if XUI_ENABLE_IMAGE_SUPPORT
	if (isSvgImagePath(path)) {
		if (error) *error = "SVG images must be loaded with SvgImage.";
		return false;
	}
	int width = 0, height = 0, sourceChannels = 0;
	if (!stbi_info(path.c_str(), &width, &height, &sourceChannels) || width <= 0 || height <= 0 ||
		!acceptableSize(static_cast<u32>(std::max(width, 0)), static_cast<u32>(std::max(height, 0)))) {
		if (error) *error = stbi_failure_reason() ? stbi_failure_reason() : "Unsupported or oversized image.";
		return false;
	}
	int decodedWidth = 0, decodedHeight = 0, decodedChannels = 0;
	stbi_uc* pixels = stbi_load(path.c_str(), &decodedWidth, &decodedHeight, &decodedChannels, STBI_rgb_alpha);
	if (!pixels) {
		if (error) *error = stbi_failure_reason() ? stbi_failure_reason() : "Image decoding failed.";
		return false;
	}
	if (decodedWidth != width || decodedHeight != height || decodedWidth <= 0 || decodedHeight <= 0 ||
		!acceptableSize(static_cast<u32>(decodedWidth), static_cast<u32>(decodedHeight))) {
		stbi_image_free(pixels);
		if (error) *error = "Decoded image dimensions are invalid or too large.";
		return false;
	}
	out.width = static_cast<u32>(decodedWidth);
	out.height = static_cast<u32>(decodedHeight);
	out.rgba.assign(pixels, pixels + static_cast<std::size_t>(out.width) * out.height * 4u);
	stbi_image_free(pixels);
	if (error) error->clear();
	return true;
#else
	(void)path;
	if (error) *error = "Image support is disabled; configure with XUI_ENABLE_IMAGE_SUPPORT=ON.";
	return false;
#endif
}

struct SvgImage::Impl {
#if XUI_ENABLE_IMAGE_SUPPORT
	std::unique_ptr<lunasvg::Document> document;
#endif
	f32 width = 0, height = 0;
};

SvgImage::SvgImage() : m_impl(std::make_unique<Impl>()) {}
SvgImage::~SvgImage() = default;
SvgImage::SvgImage(SvgImage&&) noexcept = default;
SvgImage& SvgImage::operator=(SvgImage&&) noexcept = default;

bool SvgImage::load(const std::string& path, std::string* error) {
	if (!m_impl) m_impl = std::make_unique<Impl>();
	m_impl->width = m_impl->height = 0;
#if XUI_ENABLE_IMAGE_SUPPORT
	auto document = lunasvg::Document::loadFromFile(path);
	if (!document) {
		if (error) *error = "LunaSVG could not parse the SVG document.";
		return false;
	}
	const f32 width = document->width(), height = document->height();
	if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.f || height <= 0.f) {
		if (error) *error = "SVG has no finite, positive intrinsic dimensions.";
		return false;
	}
	m_impl->width = width;
	m_impl->height = height;
	m_impl->document = std::move(document);
	if (error) error->clear();
	return true;
#else
	(void)path;
	if (error) *error = "SVG support is disabled; configure with XUI_ENABLE_IMAGE_SUPPORT=ON.";
	return false;
#endif
}

bool SvgImage::rasterize(u32 targetWidthPx, u32 targetHeightPx, bool cover, Image& out, std::string* error) const {
	out = {};
#if XUI_ENABLE_IMAGE_SUPPORT
	if (!valid() || targetWidthPx == 0 || targetHeightPx == 0) {
		if (error) *error = "SVG is not loaded or the raster size is empty.";
		return false;
	}
	const f64 sx = static_cast<f64>(targetWidthPx) / m_impl->width;
	const f64 sy = static_cast<f64>(targetHeightPx) / m_impl->height;
	const f64 scale = cover ? std::max(sx, sy) : std::min(sx, sy);
	const u32 width = static_cast<u32>(std::clamp(std::ceil(m_impl->width * scale), 1.0, static_cast<f64>(kMaxDimension)));
	const u32 height = static_cast<u32>(std::clamp(std::ceil(m_impl->height * scale), 1.0, static_cast<f64>(kMaxDimension)));
	if (!acceptableSize(width, height)) {
		if (error) *error = "Requested SVG raster exceeds the image size limit.";
		return false;
	}
	auto bitmap = m_impl->document->renderToBitmap(static_cast<int>(width), static_cast<int>(height), 0x00000000u);
	if (bitmap.isNull() || bitmap.width() <= 0 || bitmap.height() <= 0 || bitmap.stride() < bitmap.width() * 4) {
		if (error) *error = "LunaSVG could not rasterize the requested image size.";
		return false;
	}
	bitmap.convertToRGBA();
	out.width = static_cast<u32>(bitmap.width());
	out.height = static_cast<u32>(bitmap.height());
	out.rgba.resize(static_cast<std::size_t>(out.width) * out.height * 4u);
	for (u32 y = 0; y < out.height; ++y)
		std::memcpy(out.rgba.data() + static_cast<std::size_t>(y) * out.width * 4u,
			bitmap.data() + static_cast<std::size_t>(y) * bitmap.stride(), static_cast<std::size_t>(out.width) * 4u);
	if (error) error->clear();
	return true;
#else
	(void)targetWidthPx; (void)targetHeightPx; (void)cover;
	if (error) *error = "SVG support is disabled; configure with XUI_ENABLE_IMAGE_SUPPORT=ON.";
	return false;
#endif
}

f32 SvgImage::intrinsicWidth() const { return m_impl ? m_impl->width : 0.f; }
f32 SvgImage::intrinsicHeight() const { return m_impl ? m_impl->height : 0.f; }
bool SvgImage::valid() const {
#if XUI_ENABLE_IMAGE_SUPPORT
	return m_impl && m_impl->document != nullptr;
#else
	return false;
#endif
}
}
