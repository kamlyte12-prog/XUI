#pragma once
/// @file rasterizer.hpp  Scanline polygon rasteriser for glyph coverage (A8).
///
/// Input: flattened contours (line segments only — font.cpp already resolves quadratics). Output:
/// per-pixel 8-bit coverage with 4x vertical supersampling for smooth edges. Nonzero winding.
/// Deterministic: identical input always produces identical bytes on every platform.
#include <xui/core/math.hpp>
#include <xui/core/types.hpp>
#include <vector>

namespace xui {

	/// Rasterise closed contours into an 8-bit coverage bitmap (row-major, w*h bytes, 0..255).
	/// @p pts are contour points in PIXEL space with y DOWN (flip before calling: py = -py + baseline).
	/// @p contourStarts holds the first point index of each contour; the last contour ends at pts.size().
	/// @p scratch must be provided by the caller (reused across calls to avoid per-glyph allocation).
	void rasterizeContours(const Vec2* pts, u32 n, const u32* contourStarts, u32 contourCount,
		u8* bits, u32 w, u32 h, std::vector<f32>& scratch);

} // namespace xui
