#pragma once
/// @file drawlist.hpp  Backend-neutral draw data: flat POD arrays a Renderer consumes. Knows nothing about widgets.
///
/// Contract (every backend relies on it):
///  * Every primitive is an axis-aligned QUAD: 4 consecutive vertices TL,TR,BR,BL and indices (0,1,2, 0,2,3).
///  * Coordinates are PHYSICAL pixels, origin top-left, y down. Colours are straight (non-premultiplied) RGBA, 0xRRGGBBAA.
///  * One shader draws everything; the primitive kind selects a branch. Signed-distance maths (see sdf.hpp, the CPU
///    reference that the GLSL port mirrors) gives analytic anti-aliasing: no nine-patches, no extra geometry.
///  * A DrawCmd is one draw call: all its indices share a texture and a scissor rectangle.
#include <xui/core/handle.hpp>
#include <xui/core/types.hpp>

namespace xui {

	struct Recti { i32 x = 0, y = 0, w = 0, h = 0; friend constexpr bool operator==(const Recti&, const Recti&) = default; };

	enum class Prim : u8 {
		RoundRect = 0,   ///< filled/bordered/gradient rounded rectangle.  u,v = local position (px, relative to centre)
		Shadow = 1,   ///< blurred rounded rectangle (outer drop shadow, or inner shadow with FlagInner)
		Glyph = 2,   ///< A8 texture coverage * c0.  u,v = normalised texture coordinates
		Image = 3,   ///< RGBA texture * c0 tint, optionally masked by a rounded rect.  u,v = texcoords, aux = centre (px)
		Capsule = 4    ///< thick line segment with round caps (node-editor wires).  local = pos - midpoint, h = half-vector
	};
	enum : u8 {
		FlagGradient = 1,   ///< RoundRect/Capsule: blend c0 -> c1 (RoundRect: along direction (aux0,aux1); Capsule: along the segment)
		FlagDashed = 2,   ///< RoundRect border: dash period = aux0 px (50% duty)
		FlagInner = 4    ///< Shadow: inner shadow; hole is the shape shifted by (aux0,aux1)
	};

	/// 52 bytes. Per-primitive attributes are repeated on the 4 vertices (flat); only x,y,u,v differ per corner.
	struct Vertex {
		f32 x = 0, y = 0;          ///< position, physical px
		f32 u = 0, v = 0;          ///< local coords (SDF prims) or texcoords (Glyph/Image)
		u32 c0 = 0, c1 = 0;        ///< fill/tint/shadow colour; border colour or gradient end
		f32 hx = 0, hy = 0;        ///< half size of the shape (Capsule: half of the p1-p0 vector)
		u16 rad[4] = { 0, 0, 0, 0 }; ///< corner radii TL,TR,BR,BL in 1/4 px (Capsule: rad[0] = half thickness)
		f32 aux0 = 0, aux1 = 0;    ///< gradient direction / dash period / inner-shadow offset / image centre
		u16 border = 0;            ///< border width (RoundRect) or blur transition width (Shadow), in 1/4 px
		u8 prim = 0, flags = 0;
	};
	static_assert(sizeof(Vertex) == 52, "Vertex layout is part of the backend contract");

	struct DrawCmd {
		enum Kind : u8 { Triangles = 0, Custom = 1 } kind = Triangles;
		TextureHandle tex;         ///< invalid() = no texture
		Recti clip;                ///< scissor, physical px
		u32 idxOffset = 0, idxCount = 0;
		u32 customId = 0;          ///< Custom: id the application registered with Renderer::setCustomHandler
	};

	/// Non-owning view of one frame's draw data. Valid until the producer's next begin().
	struct DrawList {
		const Vertex* verts = nullptr;  u32 nVerts = 0;
		const u32* idx = nullptr;       u32 nIdx = 0;
		const DrawCmd* cmds = nullptr;  u32 nCmds = 0;
	};
}
