#include "golden.hpp"
#include "xtest.hpp"
#include "../benchmarks/paint_tree.hpp"
#include "paint_fuzz.hpp"
#include <xui/render/null_renderer.hpp>
#include <xui/render/painter.hpp>
#include <xui/render/sdf.hpp>
#include <xui/render/soft_renderer.hpp>
#include <cmath>
#include <cstdio>
#include <filesystem>
namespace xtest { extern std::size_t allocCount; }
using namespace xui;

using namespace paintfuzz;
namespace {
	double alphaSum(const Image& im) { double s = 0; for (u32 y = 0; y < im.height; ++y) for (u32 x = 0; x < im.width; ++x) s += im.px(x, y)[3] / 255.0; return s; }
	bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }
	constexpr double kPi = 3.14159265358979323846;
}

// ---- SDF maths (independent of any rendering) ---------------------------------------------------------
TEST(sdf_round_rect_distance_basic_properties) {
	const f32 r0[4] = { 0, 0, 0, 0 }, r10[4] = { 10, 10, 10, 10 };
	CHECK(near(sdf::roundRectDist(0, 0, 20, 10, r0), -10, 1e-4));           // centre: distance to nearest edge
	CHECK(near(sdf::roundRectDist(20, 0, 20, 10, r0), 0, 1e-4));            // on the right edge
	CHECK(near(sdf::roundRectDist(25, 0, 20, 10, r0), 5, 1e-4));            // 5 outside
	CHECK(near(sdf::roundRectDist(20, 10, 20, 10, r0), 0, 1e-4));           // sharp corner is on the boundary
	CHECK(sdf::roundRectDist(20, 10, 20, 10, r10) > 4);                     // ...but a rounded corner cuts it off
	const f32 k = 10 * std::sqrt(0.5f);
	CHECK(near(sdf::roundRectDist(10 + k, 0 + k - 0, 20, 10, r10), sdf::roundRectDist(10 + k, 0 + k, 20, 10, r10), 1e-5));
	CHECK(near(sdf::roundRectDist(20 - 10 + k, 10 - 10 + k, 20, 10, r10), 0, 1e-3));   // a point on the corner arc
	CHECK(near(sdf::roundRectDist(7, -3, 20, 10, r10), sdf::roundRectDist(-7, 3, 20, 10, r10), 1e-5));   // symmetric
}
TEST(sdf_coverage_and_falloff) {
	CHECK(near(sdf::coverage(0), 0.5, 1e-6)); CHECK(near(sdf::coverage(-1), 1, 1e-6)); CHECK(near(sdf::coverage(1), 0, 1e-6));
	for (f32 d = -4; d <= 4; d += 0.5f) CHECK(near(sdf::falloff(d, 8) + sdf::falloff(-d, 8), 1.0, 1e-5));   // point-symmetric around the edge
	f32 prev = 2; for (f32 d = -6; d <= 6; d += 0.25f) { const f32 v = sdf::falloff(d, 8); CHECK(v <= prev + 1e-6f); prev = v; }
	CHECK(near(sdf::falloff(10, 8), 0, 1e-6)); CHECK(near(sdf::falloff(-10, 8), 1, 1e-6));
}
TEST(sdf_capsule_distance) {
	f32 t; CHECK(near(sdf::capsuleDist(0, 0, 10, 0, 2, t), -2, 1e-5) && near(t, 0.5, 1e-5));
	CHECK(near(sdf::capsuleDist(14, 0, 10, 0, 2, t), 2, 1e-5) && t == 1.f);    // beyond the end cap
	CHECK(near(sdf::capsuleDist(0, 5, 10, 0, 2, t), 3, 1e-5));
}

// ---- rasterised areas / exact coverage ---------------------------------------------------------------
TEST(render_circle_area_matches_pi_r_squared) {
	Env e(80, 80); Painter p; p.begin(80, 80, 1);
	p.fillRect({ 10, 10, 60, 60 }, { 1, 1, 1, 1 }, CornerRadii::all(30));
	CHECK(near(alphaSum(e.draw(p)), kPi * 900, kPi * 900 * 0.005));
}
TEST(render_rounded_rect_area) {
	Env e(100, 80); Painter p; p.begin(100, 80, 1);
	p.fillRect({ 10, 10, 80, 50 }, { 1, 1, 1, 1 }, CornerRadii::all(10));
	CHECK(near(alphaSum(e.draw(p)), 80.0 * 50 - (4 - kPi) * 100, 4000 * 0.003));
}
TEST(render_border_ring_area) {
	Env e(100, 80); Painter p; p.begin(100, 80, 1);
	RectStyle s; s.border = { 1, 1, 1, 1 }; s.borderWidth = 4; s.radius = CornerRadii::all(10);
	p.roundRect({ 10, 10, 80, 50 }, s);
	const double outer = 4000 - (4 - kPi) * 100, inner = 72.0 * 42 - (4 - kPi) * 36;     // inner rect radius = 10 - 4
	CHECK(near(alphaSum(e.draw(p)), outer - inner, (outer - inner) * 0.01));
}
TEST(render_pixel_aligned_rect_is_crisp) {
	Env e(20, 12); Painter p; p.begin(20, 12, 1);
	p.fillRect({ 5, 3, 10, 6 }, { 1, 0, 0, 1 });
	Image im = e.draw(p);
	for (u32 y = 0; y < 12; ++y) for (u32 x = 0; x < 20; ++x) {
		const bool inside = x >= 5 && x < 15 && y >= 3 && y < 9;
		CHECK_EQ(int(im.px(x, y)[3]), inside ? 255 : 0);                 // no blur, no bleed, edges fully on/off
	}
	CHECK_EQ(int(im.px(7, 5)[0]), 255); CHECK_EQ(int(im.px(7, 5)[1]), 0);
}
TEST(render_half_pixel_edges_get_half_coverage_when_snapping_is_off) {
	Env e(20, 12); Painter p(Painter::Options{ true, false }); p.begin(20, 12, 1);
	p.fillRect({ 5.5f, 3, 10, 6 }, { 1, 1, 1, 1 });
	Image im = e.draw(p);
	CHECK(std::abs(int(im.px(5, 5)[3]) - 128) <= 1); CHECK(std::abs(int(im.px(15, 5)[3]) - 128) <= 1);
	CHECK_EQ(int(im.px(8, 5)[3]), 255); CHECK_EQ(int(im.px(4, 5)[3]), 0); CHECK_EQ(int(im.px(16, 5)[3]), 0);
}
TEST(render_per_corner_radii_land_on_the_right_corners) {
	// Every subset of {TL,TR,BR,BL}: exactly the rounded corners lose their corner pixel, the others keep it.
	Env e(50, 50);
	for (int mask = 0; mask < 16; ++mask) {
		const CornerRadii r{ (mask & 1) ? 20.f : 0.f, (mask & 2) ? 20.f : 0.f, (mask & 4) ? 20.f : 0.f, (mask & 8) ? 20.f : 0.f };
		Painter p; p.begin(50, 50, 1); p.fillRect({ 5, 5, 40, 40 }, { 1, 1, 1, 1 }, r);
		Image im = e.draw(p);
		const int a[4] = { int(im.px(5, 5)[3]), int(im.px(44, 5)[3]), int(im.px(44, 44)[3]), int(im.px(5, 44)[3]) };   // TL, TR, BR, BL corner pixels
		for (int c = 0; c < 4; ++c) {
			const bool rounded = (mask >> c) & 1;
			if (rounded ? a[c] != 0 : a[c] != 255) { ++xtest::failures(); std::printf("    mask %d corner %d: alpha %d (expected %s)\n", mask, c, a[c], rounded ? "0" : "255"); }
		}
	}
}
TEST(render_gradient_runs_from_start_to_end_colour) {
	Env e(100, 10); Painter p; p.begin(100, 10, 1);
	RectStyle s; s.gradient = true; s.fill = { 1, 0, 0, 1 }; s.gradientEnd = { 0, 0, 1, 1 }; s.gradientAngle = 0;
	p.roundRect({ 0, 0, 100, 10 }, s);
	Image im = e.draw(p);
	CHECK(im.px(0, 5)[0] > 250 && im.px(0, 5)[2] < 5); CHECK(im.px(99, 5)[2] > 250 && im.px(99, 5)[0] < 5);
	for (u32 x = 1; x < 100; ++x) CHECK(im.px(x, 5)[0] <= im.px(x - 1, 5)[0]);       // red falls monotonically
	CHECK(std::abs(int(im.px(49, 5)[0]) - int(im.px(50, 5)[2])) <= 3);               // symmetric about the middle
}
TEST(render_vertical_gradient_angle) {
	Env e(10, 100); Painter p; p.begin(10, 100, 1);
	RectStyle s; s.gradient = true; s.fill = { 1, 1, 1, 1 }; s.gradientEnd = { 0, 0, 0, 1 }; s.gradientAngle = f32(kPi / 2);
	p.roundRect({ 0, 0, 10, 100 }, s);
	Image im = e.draw(p);
	CHECK(im.px(5, 0)[0] > 250 && im.px(5, 99)[0] < 5);
	CHECK(std::abs(int(im.px(0, 50)[0]) - int(im.px(9, 50)[0])) <= 1);               // constant across x
}
TEST(render_dashed_border_alternates_with_the_requested_period) {
	Env e(120, 40); Painter p; p.begin(120, 40, 1);
	RectStyle s; s.border = { 1, 1, 1, 1 }; s.borderWidth = 2; s.dashed = true; s.dashLength = 8; s.radius = CornerRadii::all(0);
	p.roundRect({ 10, 5, 100, 30 }, s);
	Image im = e.draw(p);
	// along the top border row: x = 10 + k; pattern is (k + 0.5)/8 -> on for k mod 8 in [0,4), off for [4,8)
	for (u32 k = 0; k < 64; ++k) CHECK_EQ(int(im.px(10 + k, 5)[3]), (k % 8) < 4 ? 255 : 0);
	CHECK_EQ(int(im.px(60, 20)[3]), 0);                                              // interior stays empty
}
TEST(render_drop_shadow_is_symmetric_monotonic_and_bounded) {
	Env e(100, 100); Painter p; p.begin(100, 100, 1);
	p.shadow({ 30, 30, 40, 40 }, { {0, 0, 0, 1}, {0, 0}, 10, 0, {} });
	Image im = e.draw(p);
	for (u32 x = 25; x < 50; ++x) CHECK(im.px(x, 50)[3] >= im.px(x - 1, 50)[3]);     // rises towards the shape
	CHECK(std::abs(int(im.px(29, 50)[3]) + int(im.px(30, 50)[3]) - 255) <= 2);       // half intensity straddles the edge
	CHECK_EQ(int(im.px(23, 50)[3]), 0); CHECK_EQ(int(im.px(5, 5)[3]), 0);            // nothing beyond blur/2 from the edge
	CHECK(im.px(50, 50)[3] == 255);                                                  // fully opaque deep inside
}
TEST(render_inner_shadow_only_darkens_inside_the_shape) {
	Env e(100, 100); Painter p; p.begin(100, 100, 1);
	p.shadow({ 30, 30, 40, 40 }, { {0, 0, 0, 1}, {0, 8}, 10, 0, {}, true });
	Image im = e.draw(p);
	CHECK_EQ(int(im.px(20, 50)[3]), 0); CHECK_EQ(int(im.px(80, 50)[3]), 0); CHECK_EQ(int(im.px(50, 20)[3]), 0);   // never outside the shape
	CHECK(im.px(50, 31)[3] > im.px(50, 55)[3]);                                      // darkest near the edge the shadow comes from
	CHECK(im.px(50, 50)[3] < 20);                                                    // light in the middle
}
TEST(render_capsule_area_and_caps) {
	Env e(100, 40); Painter p; p.begin(100, 40, 1);
	p.line({ 20, 20 }, { 80, 20 }, 8, { 1, 1, 1, 1 });
	Image im = e.draw(p);
	CHECK(near(alphaSum(im), 60.0 * 8 + kPi * 16, (60.0 * 8 + kPi * 16) * 0.01));
	CHECK(im.px(18, 20)[3] > 200);                                                   // the round cap extends past the endpoint
	CHECK_EQ(int(im.px(14, 20)[3]), 0);
}
TEST(render_glyph_and_image_use_texture_alpha_and_tint) {
	Env e(40, 20); Painter p; p.begin(40, 20, 1);
	GlyphQuad q{ {0, 0, 8, 8}, {0, 0, 1, 1} };
	p.glyphRun(e.half, &q, 1, { 2, 2 }, { 1, 1, 1, 1 });
	p.image(e.red, { 20, 2, 8, 8 }, { 0, 0, 1, 1 }, { 1, 1, 1, 0.5f });
	Image im = e.draw(p);
	CHECK(std::abs(int(im.px(5, 5)[3]) - 128) <= 1); CHECK_EQ(int(im.px(5, 5)[0]), 255);          // 50% white glyph
	CHECK(std::abs(int(im.px(23, 5)[3]) - 128) <= 1); CHECK_EQ(int(im.px(23, 5)[0]), 255); CHECK_EQ(int(im.px(23, 5)[1]), 0);   // half-transparent red
}
TEST(render_rounded_image_is_masked_by_its_corners) {
	Env e(40, 40); Painter p; p.begin(40, 40, 1);
	p.image(e.red, { 4, 4, 32, 32 }, { 0, 0, 1, 1 }, { 1, 1, 1, 1 }, CornerRadii::all(16));
	Image im = e.draw(p);
	CHECK_EQ(int(im.px(4, 4)[3]), 0); CHECK_EQ(int(im.px(35, 35)[3]), 0); CHECK_EQ(int(im.px(20, 20)[3]), 255);
	CHECK(near(alphaSum(im), kPi * 256, kPi * 256 * 0.01));
}
TEST(render_stale_or_missing_texture_does_not_crash) {
	Env e(20, 20); TextureHandle dead = e.r.createTexture({ 4, 4, TextureFormat::RGBA8, nullptr, true }); e.r.destroyTexture(dead);
	Painter p; p.begin(20, 20, 1); GlyphQuad q{ {0, 0, 8, 8}, {0, 0, 1, 1} };
	p.glyphRun(dead, &q, 1, { 2, 2 }, { 1, 1, 1, 1 }); p.image(TextureHandle{}, { 10, 10, 5, 5 }, { 0, 0, 1, 1 });
	Image im = e.draw(p);
	CHECK_EQ(int(im.px(5, 5)[3]), 255);                                              // behaves as a white texture
}

// ---- Painter semantics --------------------------------------------------------------------------------
TEST(painter_scales_by_dpi_and_snaps) {
	Painter p; p.begin(200, 200, 2.f); p.fillRect({ 10.2f, 10.2f, 20, 20 }, { 1, 1, 1, 1 });
	DrawList dl = p.finish();
	CHECK_EQ(dl.nVerts, 4u);
	// 10.2 * 2 = 20.4 snaps to 20; snapped edges have no coverage outside them, so the quad is exactly the shape (no AA margin)
	CHECK(near(dl.verts[0].x, 20, 1e-4) && near(dl.verts[0].y, 20, 1e-4) && near(dl.verts[2].x, 60, 1e-4));
	CHECK(near(dl.verts[0].hx, 20, 1e-4));
	CHECK(near(dl.verts[0].u, -20, 1e-4));                                            // local coords are relative to the shape centre
	Painter q(Painter::Options{ true, false }); q.begin(200, 200, 2.f); q.fillRect({ 10.2f, 10.2f, 20, 20 }, { 1, 1, 1, 1 });
	DrawList d2 = q.finish();
	CHECK(near(d2.verts[0].x, 20.4f - 1, 1e-3));                                      // unsnapped edges can be partially covered: 1px margin
	CHECK(near(d2.verts[0].hx, 20, 1e-3));
}
TEST(painter_clip_rounds_out_nests_and_culls) {
	Painter p; p.begin(100, 100, 1);
	p.pushClip({ 10.2f, 10.2f, 10, 10 });
	p.fillRect({ 0, 0, 100, 100 }, { 1, 1, 1, 1 });                                       // clipped to the pushed rect
	p.pushClip({ 15, 15, 50, 50 });                                                     // nested: intersection
	p.fillRect({ 0, 0, 100, 100 }, { 1, 0, 0, 1 });
	p.fillRect({ 60, 60, 5, 5 }, { 1, 0, 0, 1 });                                         // outside the intersection -> culled
	p.popClip(); p.popClip();
	DrawList dl = p.finish();
	CHECK_EQ(dl.nCmds, 2u);
	CHECK(dl.cmds[0].clip == (Recti{ 10, 10, 11, 11 }));                                // floor(10.2)..ceil(20.2)
	CHECK(dl.cmds[1].clip == (Recti{ 15, 15, 6, 6 }));                                  // [15,21) from the intersection
	CHECK_EQ(p.stats().culled, 1u);
}
TEST(painter_culls_degenerate_and_offscreen_primitives) {
	Painter p; p.begin(100, 100, 1);
	p.fillRect({ 10, 10, 0, 10 }, { 1, 1, 1, 1 });               // zero width
	p.fillRect({ 10, 10, 10, 10 }, { 1, 1, 1, 0 });              // invisible
	p.fillRect({ 200, 10, 10, 10 }, { 1, 1, 1, 1 });             // off-screen
	p.fillRect({ -50, 10, 10, 10 }, { 1, 1, 1, 1 });             // off-screen left
	ShadowStyle invisible; invisible.color.a = 0; p.shadow({ 10, 10, 10, 10 }, invisible);   // invisible shadow
	p.glyphRun(TextureHandle{}, nullptr, 0, { 0, 0 }, { 1, 1, 1, 1 });
	DrawList dl = p.finish();
	CHECK_EQ(p.stats().culled, 6u); CHECK_EQ(dl.nVerts, 0u); CHECK_EQ(dl.nCmds, 0u);
}
TEST(painter_opacity_multiplies_alpha) {
	Painter p; p.begin(50, 50, 1);
	p.pushOpacity(0.5f); p.pushOpacity(0.5f); p.fillRect({ 0, 0, 10, 10 }, { 1, 1, 1, 1 }); p.popOpacity(); p.popOpacity();
	p.fillRect({ 20, 0, 10, 10 }, { 1, 1, 1, 1 });
	DrawList dl = p.finish();
	CHECK_EQ(dl.verts[0].c0 & 0xFF, 64u);                    // 0.25 * 255 = 63.75 -> 64
	CHECK_EQ(dl.verts[4].c0 & 0xFF, 255u);
}
TEST(painter_layers_order_overlays_above_regardless_of_paint_order) {
	Painter p; p.begin(100, 100, 1);
	p.setLayer(1); p.fillRect({ 0, 0, 50, 50 }, { 1, 0, 0, 1 });                          // overlay painted FIRST
	p.setLayer(0); p.fillRect({ 60, 60, 20, 20 }, { 0, 1, 0, 1 });                        // base painted second
	DrawList dl = p.finish();
	CHECK_EQ(dl.nCmds, 2u);
	CHECK_EQ(dl.idx[dl.cmds[0].idxOffset], 4u);                                       // first command draws the base (vertices 4..7)
	CHECK_EQ(dl.idx[dl.cmds[1].idxOffset], 0u);
}
TEST(painter_batching_collapses_alternating_rect_text_into_two_draw_calls) {
	Env e(400, 400); GlyphQuad q{ {0, 0, 6, 8}, {0, 0, .25f, 1} };
	for (int mode = 0; mode < 2; ++mode) {
		Painter p(Painter::Options{ mode == 0, true }); p.begin(400, 400, 1);
		for (int i = 0; i < 100; ++i) {                                               // 100 widgets: background, then text, none overlapping each other
			const f32 x = f32(i % 10) * 40.f, y = f32(i / 10) * 40.f;
			p.fillRect({ x + 2, y + 2, 36, 20 }, { 0.3f, 0.3f, 0.4f, 1 }, CornerRadii::all(3));
			p.glyphRun(e.blobs, &q, 1, { x + 6, y + 6 }, { 1, 1, 1, 1 });
		}
		DrawList dl = p.finish();
		if (mode == 0) CHECK_EQ(dl.nCmds, 2u); else CHECK_EQ(dl.nCmds, 200u);        // naive order needs a draw call per state change
	}
}
TEST(painter_never_reorders_across_an_overlap) {
	Env e(100, 100); GlyphQuad q{ {0, 0, 30, 30}, {0, 0, .25f, 1} };
	Painter p; p.begin(100, 100, 1);
	p.fillRect({ 10, 10, 60, 60 }, { 1, 0, 0, 1 });                                       // A: untextured
	p.glyphRun(e.blobs, &q, 1, { 20, 20 }, { 1, 1, 1, 1 });                               // T: text over A
	p.fillRect({ 25, 25, 20, 20 }, { 0, 0, 1, 1 });                                       // B: untextured, overlaps T -> must stay above T
	DrawList dl = p.finish();
	CHECK_EQ(dl.nCmds, 3u);                                                           // B cannot hop down into A's batch
	Image im = e.draw(p);   // (draw() calls finish again on an already-finished painter: rebuilt from the same prims)
	(void)im;
}
TEST(painter_custom_command_keeps_stream_order_and_invokes_the_handler) {
	NullRenderer nr; nr.init({ 100, 100, 1 });
	struct Got { u32 id = 0; Recti clip; int calls = 0; } got;
	nr.setCustomHandler([](void* c, u32 id, Recti clip) { auto* g = static_cast<Got*>(c); g->id = id; g->clip = clip; ++g->calls; }, &got);
	Painter p; p.begin(100, 100, 1);
	p.fillRect({ 0, 0, 50, 50 }, { 1, 0, 0, 1 });
	p.custom({ 10, 10, 30, 30 }, 7);                                                    // e.g. a 3D viewport
	p.fillRect({ 20, 20, 20, 20 }, { 0, 1, 0, 1 });                                       // overlaps the custom rect -> after it
	DrawList dl = p.finish();
	CHECK_EQ(dl.nCmds, 3u); CHECK(dl.cmds[1].kind == DrawCmd::Custom && dl.cmds[1].customId == 7u);
	nr.beginFrame({ 100, 100, 1, {}, 0 }); nr.submit(dl); nr.endFrame();
	CHECK_EQ(got.calls, 1); CHECK_EQ(got.id, 7u); CHECK(got.clip == (Recti{ 10, 10, 30, 30 }));
}
TEST(painter_output_is_a_well_formed_drawlist) {
	Env e(200, 200); Painter p; p.begin(200, 200, 1);
	GlyphQuad q[3] = { {{0, 0, 8, 8}, {0, 0, .25f, 1}}, {{9, 0, 8, 8}, {.25f, 0, .25f, 1}}, {{18, 0, 8, 8}, {.5f, 0, .25f, 1}} };
	for (int i = 0; i < 20; ++i) { p.fillRect({ f32(i * 9), 5, 8, 8 }, { 1, 1, 1, 1 }); p.glyphRun(e.blobs, q, 3, { f32(i * 9), 30 }, { 1, 1, 1, 1 }); p.bezier({ 0, 100 }, { 50, 80 }, { 100, 150 }, { 190, 120 }, 2, { 1, 1, 0, 1 }); }
	DrawList dl = p.finish();
	CHECK(dl.nIdx % 6 == 0 && dl.nVerts % 4 == 0);
	u32 expect = 0; bool contiguous = true, inRange = true;
	for (u32 c = 0; c < dl.nCmds; ++c) { contiguous = contiguous && dl.cmds[c].idxOffset == expect; expect += dl.cmds[c].idxCount; }
	for (u32 i = 0; i < dl.nIdx; ++i) inRange = inRange && dl.idx[i] < dl.nVerts;
	CHECK(contiguous && expect == dl.nIdx && inRange);
	for (u32 q4i = 0; q4i + 6 <= dl.nIdx; q4i += 6) CHECK(dl.idx[q4i] == dl.idx[q4i + 3] && dl.idx[q4i + 2] == dl.idx[q4i + 4] && dl.idx[q4i + 1] == dl.idx[q4i] + 1);   // 0,1,2,0,2,3
}
TEST(painter_steady_state_frames_allocate_nothing) {
	Env e(300, 200); Painter p; GlyphQuad q[4] = { {{0, 0, 8, 8}, {0, 0, .25f, 1}}, {{9, 0, 8, 8}, {.25f, 0, .25f, 1}}, {{18, 0, 8, 8}, {.5f, 0, .25f, 1}}, {{27, 0, 8, 8}, {.75f, 0, .25f, 1}} };
	auto frame = [&](int k) {
		p.begin(300, 200, 1); p.pushClip({ 0, 0, 280, 180 });
		for (int i = 0; i < 60; ++i) { const f32 x = f32((i * 37 + k) % 250), y = f32((i * 53) % 150); p.fillRect({ x, y, 30, 16 }, { 0.3f, 0.3f, 0.4f, 1 }, CornerRadii::all(3)); p.glyphRun(e.blobs, q, 4, { x + 2, y + 2 }, { 1, 1, 1, 1 }); }
		p.shadow({ 20, 20, 80, 40 }, {}); p.bezier({ 0, 0 }, { 100, 0 }, { 100, 100 }, { 200, 100 }, 2, { 1, 1, 1, 1 }); p.setLayer(1); p.fillRect({ 5, 5, 50, 20 }, { 1, 0, 0, 1 }); p.setLayer(0); p.popClip();
		(void)p.finish();
		};
	for (int k = 0; k < 4; ++k) frame(k);                                             // warm the buffers
	const std::size_t before = xtest::allocCount;
	for (int k = 0; k < 100; ++k) frame(k);
	CHECK_EQ(xtest::allocCount - before, 0u);
}

// ---- the property that makes batching legal: reordering never changes a single pixel ------------------------
TEST(painter_reordering_is_pixel_identical_to_naive_order) {
	Env e(200, 150);
	u64 callsOn = 0, callsOff = 0; int failures = 0;
	for (u64 seed = 1; seed <= 400; ++seed) {
		u32 on = 0, off = 0; const std::string err = reorderCase(e, seed, 70, &on, &off);
		if (!err.empty() && failures++ < 3) std::printf("    seed %llu: %s\n", (unsigned long long)seed, err.c_str());
		callsOn += on; callsOff += off;
	}
	CHECK_EQ(failures, 0);
	std::printf("    draw calls over 400 random scenes: %llu reordered vs %llu naive\n", (unsigned long long)callsOn, (unsigned long long)callsOff);
}

// ---- renderers & IO -----------------------------------------------------------------------------------
TEST(null_renderer_records_frames_and_tracks_textures) {
	NullRenderer nr; CHECK(nr.init({ 64, 64, 1 }));
	TextureHandle t = nr.createTexture({ 4, 4, TextureFormat::A8, nullptr, true }), t2 = nr.createTexture({ 8, 8, TextureFormat::RGBA8, nullptr, true });
	CHECK(t.valid() && t2.valid() && t != t2); CHECK_EQ(nr.liveTextures(), 2u);
	nr.destroyTexture(t); CHECK_EQ(nr.liveTextures(), 1u); nr.destroyTexture(t); CHECK_EQ(nr.liveTextures(), 1u);   // double destroy is harmless
	TextureHandle t3 = nr.createTexture({ 2, 2, TextureFormat::A8, nullptr, true });
	CHECK(t3.index == t.index && t3.gen != t.gen);                                                              // slot reused, old handle stale
	Painter p; p.begin(64, 64, 1); p.fillRect({ 0, 0, 10, 10 }, { 1, 1, 1, 1 }); p.setLayer(1); p.fillRect({ 5, 5, 10, 10 }, { 1, 0, 0, 1 });
	nr.beginFrame({ 64, 64, 1, {}, 0 }); nr.submit(p.finish()); nr.endFrame();
	CHECK_EQ(nr.drawCallsLastFrame(), 2u); CHECK_EQ(nr.last().verts.size(), 8u); CHECK_EQ(nr.last().cmds.size(), 2u); CHECK_EQ(nr.frames(), 1u);
}
TEST(png_roundtrip_and_comparison) {
	Image a; a.width = 70; a.height = 50; a.rgba.resize(70 * 50 * 4);
	for (std::size_t i = 0; i < a.rgba.size(); ++i) a.rgba[i] = u8((i * 7 + i / 13) & 255);
	const std::string tmp = std::filesystem::temp_directory_path().string();   // portable across Windows/POSIX
	CHECK(writePng(tmp + "/xui_rt.png", a));
	Image b; CHECK(readPng((tmp + "/xui_rt.png").c_str(), b)); CHECK(b.width == 70 && b.height == 50 && b.rgba == a.rgba);
	Image c = a; c.rgba[8] ^= 5;
	CHECK_EQ(compareImages(a, c, 4).badPixels, 1u); CHECK_EQ(compareImages(a, c, 5).badPixels, 0u); CHECK_EQ(compareImages(a, c, 5).maxDelta, 5u);
	Image d = a; d.width = 10; CHECK(compareImages(a, d, 0).sizeMismatch);
	Image big; big.width = 300; big.height = 300; big.rgba.assign(300 * 300 * 4, 77);           // > 64 KiB: exercises multiple stored deflate blocks
	CHECK(writePng(tmp + "/xui_rt2.png", big)); Image big2; CHECK(readPng((tmp + "/xui_rt2.png").c_str(), big2) && big2.rgba == big.rgba);
	CHECK(!readPng((tmp + "/definitely_missing.png").c_str(), b));
}
TEST(soft_renderer_texture_update_and_resize) {
	Env e(16, 16); TextureHandle t = e.r.createTexture({ 2, 1, TextureFormat::A8, nullptr, true });
	const u8 px[2] = { 0, 255 }; e.r.updateTexture(t, px, { 0, 0, 2, 1 });
	Painter p; p.begin(16, 16, 1); GlyphQuad q{ {0, 0, 16, 4}, {0, 0, 1, 1} }; p.glyphRun(t, &q, 1, { 0, 0 }, { 1, 1, 1, 1 });
	Image im = e.draw(p);
	CHECK(im.px(1, 2)[3] < 40 && im.px(14, 2)[3] > 215);                                      // left texel transparent -> right texel opaque, bilinear between
	e.r.resize(32, 8, 1.f); Painter p2; p2.begin(32, 8, 1); p2.fillRect({ 0, 0, 32, 8 }, { 1, 1, 1, 1 });
	Image im2 = e.draw(p2); CHECK(im2.width == 32 && im2.height == 8 && im2.px(31, 7)[3] == 255);
}

TEST(soft_renderer_partial_update_preserves_and_exports_only_the_dirty_region) {
	SoftRenderer renderer;
	CHECK(renderer.init({ 8, 8, 1.f, nullptr, false }));
	renderer.beginFrame({ 8, 8, 1.f, Color{1, 0, 0, 1}, 0 });
	std::vector<u8> bgra;
	renderer.snapshotBGRA(bgra);
	FrameInfo frame{ 8, 8, 1.f, Color{0, 1, 0, 1}, 1 };
	frame.damage = { 2, 2, 2, 2 }; frame.partialUpdate = true;
	renderer.beginFrame(frame);
	renderer.snapshotBGRA(bgra, frame.damage);
	const std::size_t outside = (std::size_t(0) * 8 + 0) * 4;
	const std::size_t inside = (std::size_t(2) * 8 + 2) * 4;
	CHECK_EQ(bgra[outside + 2], 255u); // original red remains in the backing image
	CHECK_EQ(bgra[inside + 1], 255u);  // the requested rectangle is now green
	CHECK_EQ(bgra[inside + 2], 0u);
	renderer.shutdown();
}

// Acceptance criterion: swapping the renderer requires ZERO changes to application code. This function sees only the interface.
namespace {
	u32 appFrame(Renderer& r, Painter& p) {
		const u8 px[4 * 4] = { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255 };
		TextureHandle tex = r.createTexture({ 2, 2, TextureFormat::RGBA8, px, false });
		p.begin(64, 48, 1.f);
		p.fillRect({ 0, 0, 64, 48 }, { 0.1f, 0.1f, 0.15f, 1 });
		RectStyle s; s.fill = { 0.9f, 0.5f, 0.1f, 1 }; s.border = { 1, 1, 1, 1 }; s.borderWidth = 1; s.radius = CornerRadii::all(6);
		p.roundRect({ 4, 4, 30, 20 }, s);
		p.image(tex, { 38, 4, 20, 20 }, { 0, 0, 1, 1 });
		p.shadow({ 8, 30, 40, 12 }, {});
		const DrawList dl = p.finish();
		r.beginFrame({ 64, 48, 1.f, {0, 0, 0, 1}, 0 }); r.submit(dl); r.endFrame(); r.present();
		r.destroyTexture(tex);
		return dl.nCmds;
	}
}
TEST(renderer_backends_are_interchangeable_behind_the_interface) {
	SoftRenderer soft; NullRenderer null;
	Renderer* backends[2] = { &soft, &null };
	u32 cmds[2] = { 0, 0 };
	for (int i = 0; i < 2; ++i) { Painter p; CHECK(backends[i]->init({ 64, 48, 1.f })); cmds[i] = appFrame(*backends[i], p); CHECK(backends[i]->caps().name[0] != '\0'); backends[i]->shutdown(); }
	CHECK_EQ(cmds[0], cmds[1]);                                                         // the same DrawList reached both
	CHECK(cmds[0] >= 2u);
}

// ---- paint budgets on the Blender-shaped tree ----------------------------------------------------------
TEST(paint_blender_layout_stays_within_the_draw_call_budget) {
	blender::Ui u; blender::build(u); u.t.layout(u.root, 1920, 1080);
	NullRenderer nr; nr.init({ 1920, 1080, 1 }); auto pix = blender::makeAtlasPixels();
	TextureHandle atlas = nr.createTexture({ 128, 12, TextureFormat::A8, pix.data(), true });
	Painter p; blender::PaintCtx c{ &p, &u.t, atlas };
	p.begin(1920, 1080, 1); blender::paintNode(c, u.root, { 0, 0 }, 0);
	DrawList dl = p.finish();
	std::printf("    %u widgets, %u prims -> %u draw calls (budget < 50), %u vertices\n", c.widgets, p.stats().prims, p.stats().drawCalls, p.stats().vertices);
	CHECK(c.widgets > 600); CHECK(dl.nCmds < 50);
	Painter naive(Painter::Options{ false, true }); blender::PaintCtx c2{ &naive, &u.t, atlas };
	naive.begin(1920, 1080, 1); blender::paintNode(c2, u.root, { 0, 0 }, 0); (void)naive.finish();
	CHECK(naive.stats().drawCalls > 10 * p.stats().drawCalls);                                // batching is what buys the budget
}

// ---- golden images ------------------------------------------------------------------------------------
// Goldens detect REGRESSIONS; they cannot prove the maths is right (the analytic tests above do that). Each golden was reviewed by eye.
namespace {
	Image sceneGallery(Env& e) {
		Painter p; p.begin(420, 300, 1.f);
		p.fillRect({ 0, 0, 420, 300 }, { 0.16f, 0.16f, 0.18f, 1 });
		RectStyle a; a.fill = { 0.23f, 0.51f, 0.96f, 1 }; a.border = { 1, 1, 1, 0.9f }; a.borderWidth = 2; a.radius = CornerRadii::all(12);
		p.shadow({ 20, 20, 110, 60 }, { {0, 0, 0, 0.7f}, {0, 4}, 14, 0, CornerRadii::all(12) });
		p.roundRect({ 20, 20, 110, 60 }, a);
		RectStyle g; g.gradient = true; g.fill = { 0.95f, 0.3f, 0.2f, 1 }; g.gradientEnd = { 0.2f, 0.2f, 0.9f, 1 }; g.gradientAngle = 1.5708f; g.radius = { 20, 4, 20, 4 }; g.border = { 1, 1, 1, 1 }; g.borderWidth = 2;
		p.roundRect({ 150, 20, 110, 60 }, g);
		RectStyle d; d.fill = { 0.1f, 0.1f, 0.12f, 1 }; d.border = { 0.9f, 0.8f, 0.2f, 1 }; d.borderWidth = 2; d.dashed = true; d.dashLength = 8; d.radius = CornerRadii::all(6);
		p.roundRect({ 280, 20, 110, 60 }, d);
		RectStyle c; c.fill = { 0.9f, 0.9f, 0.9f, 1 }; c.radius = CornerRadii::all(100);
		p.roundRect({ 20, 110, 60, 60 }, c);
		p.fillRect({ 100, 110, 120, 40 }, { 0.25f, 0.5f, 0.3f, 1 }, CornerRadii::all(6));
		p.shadow({ 100, 110, 120, 40 }, { {0, 0, 0, 0.8f}, {0, 6}, 16, 0, CornerRadii::all(6), true });
		const GlyphQuad gq[4] = { {{0, 0, 16, 16}, {0, 0, .25f, 1}}, {{18, 0, 16, 16}, {.25f, 0, .25f, 1}}, {{36, 0, 16, 16}, {.5f, 0, .25f, 1}}, {{54, 0, 16, 16}, {.75f, 0, .25f, 1}} };
		p.glyphRun(e.blobs, gq, 4, { 110, 160 }, { 1, 1, 1, 1 });
		p.image(e.checker, { 240, 110, 64, 64 }, { 0, 0, 1, 1 }, { 1, 1, 1, 1 }, CornerRadii::all(16));
		p.image(e.checker, { 320, 110, 64, 64 }, { 0, 0, 1, 1 });
		p.bezier({ 30, 260 }, { 130, 260 }, { 130, 200 }, { 230, 200 }, 3, { 0.4f, 0.9f, 0.5f, 1 });
		p.line({ 250, 200 }, { 400, 270 }, 6, { 1, 0.4f, 0.2f, 1 }, { 0.3f, 0.6f, 1, 1 });
		p.pushClip({ 300, 190, 90, 40 }); p.fillRect({ 280, 180, 200, 100 }, { 0.9f, 0.2f, 0.6f, 1 }, CornerRadii::all(10)); p.popClip();
		return e.draw(p, { 0, 0, 0, 1 });
	}
	Image sceneCornerMatrix(f32 dpi) {
		Env e(u32(240 * dpi), u32(120 * dpi), dpi); Painter p; p.begin(u32(240 * dpi), u32(120 * dpi), dpi);
		p.fillRect({ 0, 0, 240, 120 }, { 0.12f, 0.12f, 0.14f, 1 });
		const f32 radii[6] = { 0, 2, 6, 14, 16, 40 };                                         // 16 = exactly half, 40 must clamp to a circle
		for (int i = 0; i < 6; ++i) { RectStyle s; s.fill = { 0.33f, 0.55f, 0.85f, 1 }; s.border = { 0.75f, 0.75f, 0.8f, 1 }; s.borderWidth = 1; s.radius = CornerRadii::all(radii[i]); p.roundRect({ 8.f + 38.f * f32(i), 10, 32, 32 }, s); }
		const CornerRadii per[6] = { {0, 0, 0, 0}, {6, 0, 0, 0}, {0, 6, 0, 0}, {0, 0, 14, 0}, {14, 0, 14, 0}, {0, 14, 0, 14} };   // TL, TR, BR, BL combinations
		for (int i = 0; i < 6; ++i) { RectStyle s; s.fill = { 0.86f, 0.45f, 0.35f, 1 }; s.border = { 0.75f, 0.75f, 0.8f, 1 }; s.borderWidth = 1; s.radius = per[i]; p.roundRect({ 8.f + 38.f * f32(i), 50, 32, 32 }, s); }
		p.fillRect({ 10, 92, 220, 20 }, { 0.3f, 0.7f, 0.4f, 1 }, CornerRadii::all(100));        // pill
		return e.draw(p, { 0, 0, 0, 1 }, dpi);
	}
	Image sceneNodeEditor() {
		Env e(320, 200); Painter p; p.begin(320, 200, 1);
		p.fillRect({ 0, 0, 320, 200 }, { 0.14f, 0.14f, 0.16f, 1 });
		p.fillRect({ 30, 30, 70, 50 }, { 0.3f, 0.3f, 0.35f, 1 }, CornerRadii::all(4)); p.fillRect({ 220, 110, 70, 50 }, { 0.3f, 0.3f, 0.35f, 1 }, CornerRadii::all(4));
		p.bezier({ 100, 52 }, { 160, 52 }, { 160, 134 }, { 218, 134 }, 2, { 0.95f, 0.7f, 0.15f, 1 });
		p.bezier({ 100, 70 }, { 150, 70 }, { 170, 150 }, { 218, 150 }, 2, { 0.3f, 0.75f, 0.9f, 1 });
		for (const Vec2 sock : {Vec2{ 100, 52 }, Vec2{ 218, 134 }}) p.fillRect({ sock.x - 3, sock.y - 3, 6, 6 }, { 0.95f, 0.7f, 0.15f, 1 }, CornerRadii::all(3));   // sockets match their wire
		for (const Vec2 sock : {Vec2{ 100, 70 }, Vec2{ 218, 150 }}) p.fillRect({ sock.x - 3, sock.y - 3, 6, 6 }, { 0.3f, 0.75f, 0.9f, 1 }, CornerRadii::all(3));
		const Vec2 zig[5] = { {30, 120}, {60, 170}, {90, 120}, {120, 170}, {150, 120} };
		p.polyline(zig, 5, 3, { 0.9f, 0.35f, 0.5f, 1 });
		p.pushClip({ 160, 20, 140, 60 }); p.line({ 120, 0 }, { 320, 120 }, 14, { 0.5f, 0.9f, 0.5f, 1 }); p.popClip();
		return e.draw(p, { 0, 0, 0, 1 });
	}
	Image sceneShadowsAndLayers() {
		Env e(300, 180); Painter p; p.begin(300, 180, 1);
		p.fillRect({ 0, 0, 300, 180 }, { 0.9f, 0.9f, 0.93f, 1 });
		const f32 blur[4] = { 0, 6, 12, 20 }, off[4] = { 2, 3, 5, 8 };
		for (int i = 0; i < 4; ++i) {
			const Rect r{ 20.f + 70.f * f32(i), 20, 56, 56 };
			p.shadow(r, { {0, 0, 0, 0.45f}, {0, off[i]}, blur[i], 0, CornerRadii::all(8) });
			p.fillRect(r, { 1, 1, 1, 1 }, CornerRadii::all(8));
		}
		p.setLayer(1);                                                                      // painted FIRST, but an overlay: must end up on top
		p.shadow({ 70, 118, 120, 40 }, { {0, 0, 0, 0.5f}, {0, 5}, 14, 0, CornerRadii::all(5) });
		p.fillRect({ 70, 118, 120, 40 }, { 0.98f, 0.98f, 0.98f, 1 }, CornerRadii::all(5));
		p.setLayer(0);
		p.fillRect({ 20, 100, 120, 50 }, { 0.2f, 0.4f, 0.8f, 1 }, CornerRadii::all(5));
		p.fillRect({ 150, 100, 130, 30 }, { 0.82f, 0.32f, 0.2f, 1 }, CornerRadii::all(15));
		return e.draw(p, { 0, 0, 0, 1 });
	}
	Image sceneBlender(bool reorder) {
		blender::Ui u; blender::build(u); u.t.layout(u.root, 1280, 720);
		Env e(640, 360, 0.5f); auto pix = blender::makeAtlasPixels();
		TextureHandle atlas = e.r.createTexture({ 128, 12, TextureFormat::A8, pix.data(), true });
		Painter p(Painter::Options{ reorder, true }); blender::PaintCtx c{ &p, &u.t, atlas };
		p.begin(640, 360, 0.5f); blender::paintNode(c, u.root, { 0, 0 }, 0);
		return e.draw(p, { 0, 0, 0, 1 }, 0.5f);
	}
}
TEST(golden_primitives_gallery) { Env e(420, 300); CHECK_GOLDEN("primitives_gallery", sceneGallery(e)); }
TEST(golden_corner_radius_matrix_at_two_dpi_scales) { CHECK_GOLDEN("corner_matrix_1x", sceneCornerMatrix(1.f)); CHECK_GOLDEN("corner_matrix_2x", sceneCornerMatrix(2.f)); }
TEST(golden_node_editor_wires_and_clipping) { CHECK_GOLDEN("node_editor_wires", sceneNodeEditor()); }
TEST(golden_shadows_and_layers) { CHECK_GOLDEN("shadows_and_layers", sceneShadowsAndLayers()); }
TEST(golden_blender_layout_at_half_dpi) { CHECK_GOLDEN("blender_layout_640x360", sceneBlender(true)); }
TEST(paint_blender_layout_is_identical_with_and_without_reordering) {
	const ImageDiff d = compareImages(sceneBlender(true), sceneBlender(false), 0);
	CHECK_EQ(d.badPixels, 0u);                                                          // 641 widgets, 1300+ prims: batching changed nothing visible
}
