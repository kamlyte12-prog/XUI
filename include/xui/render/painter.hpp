#pragma once
/// @file painter.hpp  Immediate-mode drawing API that produces a batched, backend-neutral DrawList.
///
/// Widgets call roundRect()/glyphRun()/... in LOGICAL pixels (dp); the painter scales by the DPI, snaps edges to the
/// physical pixel grid, culls what the clip rejects, and batches by (texture, scissor).
///
/// Batching without breaking painter's-algorithm semantics: a new primitive may join any EARLIER batch with the same
/// state, provided that batch is not older than the newest batch holding a primitive that truly OVERLAPS it - i.e. it still
/// lands above everything it overlaps. Non-overlapping primitives never constrain each other, so
/// `rect,text,rect,text,...` collapses to one rect batch + one text batch. Overlap is tested on exact rectangles; a 32px
/// grid only narrows down which rectangles to test.
///
/// Zero steady-state allocation: all buffers are cleared, never freed, by begin().
#include <xui/core/math.hpp>
#include <xui/render/drawlist.hpp>
#include <xui/render/renderer.hpp>
#include <vector>

namespace xui {

	struct CornerRadii {
		f32 tl = 0, tr = 0, br = 0, bl = 0;
		static constexpr CornerRadii all(f32 r) { return { r, r, r, r }; }
		constexpr bool any() const { return tl > 0 || tr > 0 || br > 0 || bl > 0; }
	};
	struct RectStyle {
		Color fill{ 0, 0, 0, 0 };
		Color border{ 0, 0, 0, 0 }; f32 borderWidth = 0;   ///< dp; drawn inside the rect
		CornerRadii radius;
		bool dashed = false; f32 dashLength = 6;         ///< dp period (50% duty)
		bool gradient = false; Color gradientEnd{ 0, 0, 0, 0 }; f32 gradientAngle = 0;   ///< 0 = left->right, pi/2 = top->bottom
	};
	struct ShadowStyle {
		Color color{ 0, 0, 0, 0.5f };
		Vec2 offset{ 0, 2 }; f32 blur = 8; f32 spread = 0; ///< blur = full transition width (dp). spread ignored for inner shadows
		CornerRadii radius;
		bool inner = false;
	};
	struct GlyphQuad { Rect dst; Rect uv; };             ///< dst relative to the run origin (dp); uv normalised

	class Painter {
	public:
		struct Options {
			bool reorder = true;      ///< tile-dependency batching (false = merge only with the immediately preceding batch)
			bool snap = true;         ///< snap rect edges / glyph origins to whole physical pixels
		};
		struct Stats { u32 prims = 0, culled = 0, batches = 0, drawCalls = 0, vertices = 0, indices = 0; };
		static constexpr u32 kMaxLayers = 8;

		Painter() : Painter(Options{}) {}
		explicit Painter(Options o) : m_opt(o) {}

		/// Start a frame. @p widthPx/@p heightPx are physical pixels.
		void begin(u32 widthPx, u32 heightPx, f32 dpiScale);
		/// Overlays (menus, tooltips, drag previews) draw in a higher layer: always above lower layers, regardless of paint order.
		void setLayer(u32 layer);
		void pushClip(Rect logical);           ///< intersects with the current clip; rounded OUT to whole physical pixels
		void popClip();
		void pushOpacity(f32 alpha);           ///< multiplies the alpha of everything drawn until popOpacity (per primitive)
		void popOpacity();

		void fillRect(Rect r, Color c, CornerRadii radius = {});
		void roundRect(Rect r, const RectStyle& s);
		void shadow(Rect r, const ShadowStyle& s);
		/// A run of glyph quads sharing one atlas and colour; `origin` is added to every quad. A8 atlas.
		void glyphRun(TextureHandle atlas, const GlyphQuad* quads, u32 count, Vec2 origin, Color c);
		void image(TextureHandle tex, Rect dst, Rect uv, Color tint = { 1, 1, 1, 1 }, CornerRadii radius = {});
		/// Thick segment with round caps. With `gradient`, colour blends c -> c2 along the segment.
		void line(Vec2 a, Vec2 b, f32 width, Color c);
		void line(Vec2 a, Vec2 b, f32 width, Color c, Color c2);
		void polyline(const Vec2* pts, u32 n, f32 width, Color c);
		/// Cubic Bezier as a chain of capsules (node-editor wires). Segment count adapts to the curve length.
		void bezier(Vec2 p0, Vec2 c0, Vec2 c1, Vec2 p1, f32 width, Color c);
		/// Reserve a rectangle for application drawing (e.g. a 3D viewport). Emits a DrawCmd::Custom in stream order.
		void custom(Rect r, u32 id);

		/// Finish the frame: build indices and commands. The returned views are valid until the next begin().
		DrawList finish();
		const Stats& stats() const { return m_stats; }

	private:
		struct ClipI {
			i32 x0 = 0, y0 = 0, x1 = 0, y1 = 0; bool empty() const { return x1 <= x0 || y1 <= y0; }
			friend bool operator==(const ClipI&, const ClipI&) = default;
		};
		struct Key { TextureHandle tex; ClipI clip; u32 custom = 0; friend bool operator==(const Key&, const Key&) = default; };
		struct Prim { u32 firstVert = 0, quads = 0, next = kNil; };
		struct Batch { Key key; u32 head = kNil, tail = kNil; };
		struct Entry { i32 x0, y0, x1, y1; u32 batch, next; };      // a placed primitive's bounds, linked per grid cell
		struct Layer { std::vector<Batch> batches; std::vector<Entry> entries; std::vector<u32> cellHead; bool used = false; };
		static constexpr u32 kNil = 0xFFFFFFFFu;
		static constexpr u32 kCellShift = 5;

		Vertex makeVertex(f32 x, f32 y) const;
		bool admit(f32 x0, f32 y0, f32 x1, f32 y1, ClipI& bounds) const;   // bounds ∩ clip; false => cull
		void commit(const Key& key, const ClipI& bounds, u32 firstVert, u32 quads);
		u32 packAlpha(Color c) const;
		void quad(const Vertex& tmpl, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1);
		void rectPrim(Rect r, const RectStyle& s, bool ringOnly);
		void capsule(Vec2 a, Vec2 b, f32 width, Color c, Color c2, bool grad);
		f32 snapv(f32 v) const { return m_opt.snap ? std::round(v) : v; }

		Options m_opt;
		f32 m_dpi = 1.f; u32 m_w = 0, m_h = 0, m_cellsX = 0, m_cellsY = 0;
		u32 m_layer = 0, m_nextCustom = 1;
		std::vector<ClipI> m_clips; std::vector<f32> m_opacity;
		std::vector<Vertex> m_verts; std::vector<Prim> m_prims; std::vector<u32> m_indices; std::vector<DrawCmd> m_cmds;
		Layer m_layers[kMaxLayers];
		Stats m_stats;
	};
}
