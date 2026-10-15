#include "xtest.hpp"
#include <xui/layout/layout.hpp>
#include <cmath>
#include <cstdio>
namespace xtest { extern std::size_t allocCount; }
using namespace xui;

// ---- helpers ---------------------------------------------------------------------------------
static LayoutStyle box(f32 w, f32 h) { LayoutStyle s; s.width = Length::px(w); s.height = Length::px(h); return s; }
static bool near(f32 a, f32 b) { return std::fabs(a - b) < 0.01f; }
static bool rectIs(const char* what, Rect r, f32 x, f32 y, f32 w, f32 h) {
	bool ok = near(r.x, x) && near(r.y, y) && near(r.w, w) && near(r.h, h);
	if (!ok) std::printf("    %s: got (%g,%g %gx%g) expected (%g,%g %gx%g)\n", what, r.x, r.y, r.w, r.h, x, y, w, h);
	return ok;
}
#define CHECK_RECT(tree, node, x, y, w, h) CHECK(rectIs(#node, (tree).rect(node), x, y, w, h))
static Size leaf40x20(void*, NodeId, Constraint) { return { 40, 20 }; }
static Size leafZero(void*, NodeId, Constraint) { return { 0, 0 }; }
// Wrapping "text": natural width 200, line height 20; height depends on the width it is given.
static Size leafWrap(void*, NodeId, Constraint c) {
	f32 w = std::min(200.f, std::isfinite(c.w) ? std::max(c.w, 1.f) : 200.f);
	return { w, 20.f * std::ceil(200.f / w) };
}
struct Fix { LayoutTree t; };

TEST(layout_row_fixed_children_padding_gap) {
	LayoutTree t; LayoutStyle rs = box(300, 100); rs.padding = Edges::all(10); rs.gap = { 5, 0 };
	NodeId root = t.create(LayoutKind::Row, rs), a = t.create(LayoutKind::Leaf, box(50, 20)), b = t.create(LayoutKind::Leaf, box(60, 30));
	t.append(root, a); t.append(root, b); t.layout(root, 300, 100);
	CHECK_RECT(t, a, 10, 10, 50, 20);
	CHECK_RECT(t, b, 65, 10, 60, 30);
}
TEST(layout_column_stretches_cross_axis_by_default) {
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	NodeId root = t.create(LayoutKind::Column, box(200, 300)), a = t.create(LayoutKind::Leaf), b = t.create(LayoutKind::Leaf);
	t.append(root, a); t.append(root, b); t.layout(root, 200, 300);
	CHECK_RECT(t, a, 0, 0, 200, 20);      // width stretched to the column, height = intrinsic
	CHECK_RECT(t, b, 0, 20, 200, 20);
}
TEST(layout_margins_offset_and_reduce_stretch) {
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	NodeId root = t.create(LayoutKind::Column, box(200, 300)); LayoutStyle cs; cs.margin = { 5, 7, 15, 3 };
	NodeId a = t.create(LayoutKind::Leaf, cs), b = t.create(LayoutKind::Leaf);
	t.append(root, a); t.append(root, b); t.layout(root, 200, 300);
	CHECK_RECT(t, a, 5, 7, 180, 20);      // 200 - 5 - 15
	CHECK_RECT(t, b, 0, 30, 200, 20);     // 7 + 20 + 3 below a
}
TEST(layout_flex_grow_distributes_free_space) {
	LayoutTree t; NodeId root = t.create(LayoutKind::Row, box(300, 50)); NodeId k[3];
	for (int i = 0; i < 3; ++i) { LayoutStyle s = box(50, 50); s.grow = f32(i); k[i] = t.create(LayoutKind::Leaf, s); t.append(root, k[i]); }
	t.layout(root, 300, 50);          // free 150 split 0:1:2
	CHECK_RECT(t, k[0], 0, 0, 50, 50); CHECK_RECT(t, k[1], 50, 0, 100, 50); CHECK_RECT(t, k[2], 150, 0, 150, 50);
}
TEST(layout_flex_shrink_is_weighted_by_basis) {
	LayoutTree t; NodeId root = t.create(LayoutKind::Row, box(100, 20));
	LayoutStyle s1 = box(80, 20), s2 = box(80, 20); s2.shrink = 3;
	NodeId a = t.create(LayoutKind::Leaf, s1), b = t.create(LayoutKind::Leaf, s2);
	t.append(root, a); t.append(root, b); t.layout(root, 100, 20);   // overflow 60 shared 80:240
	CHECK_RECT(t, a, 0, 0, 65, 20); CHECK_RECT(t, b, 65, 0, 35, 20);
}
TEST(layout_flex_max_violation_redistributes) {
	LayoutTree t; NodeId root = t.create(LayoutKind::Row, box(300, 10));
	LayoutStyle sa; sa.width = Length::px(0); sa.height = Length::px(10); sa.grow = 1; sa.maxW = 100; LayoutStyle sb = sa; sb.maxW = kInf;
	NodeId a = t.create(LayoutKind::Leaf, sa), b = t.create(LayoutKind::Leaf, sb);
	t.append(root, a); t.append(root, b); t.layout(root, 300, 10);
	CHECK_RECT(t, a, 0, 0, 100, 10); CHECK_RECT(t, b, 100, 0, 200, 10);   // A frozen at max; B takes the rest
}
TEST(layout_flex_min_violation_redistributes) {
	LayoutTree t; NodeId root = t.create(LayoutKind::Row, box(100, 10));
	LayoutStyle sa; sa.width = Length::px(0); sa.height = Length::px(10); sa.grow = 1; sa.minW = 60; LayoutStyle sb = sa; sb.minW = 0;
	NodeId a = t.create(LayoutKind::Leaf, sa), b = t.create(LayoutKind::Leaf, sb);
	t.append(root, a); t.append(root, b); t.layout(root, 100, 10);
	CHECK_RECT(t, a, 0, 0, 60, 10); CHECK_RECT(t, b, 60, 0, 40, 10);
}
static f32 justifyX(Justify j, int index) {
	LayoutTree t; LayoutStyle rs = box(100, 10); rs.justify = j; NodeId root = t.create(LayoutKind::Row, rs); NodeId k[3];
	for (int i = 0; i < 3; ++i) { k[i] = t.create(LayoutKind::Leaf, box(10, 10)); t.append(root, k[i]); }
	t.layout(root, 100, 10); return t.rect(k[index]).x;
}
TEST(layout_justify_modes) {
	CHECK(near(justifyX(Justify::Start, 0), 0));    CHECK(near(justifyX(Justify::Center, 0), 35));  CHECK(near(justifyX(Justify::End, 0), 70));
	CHECK(near(justifyX(Justify::SpaceBetween, 0), 0)); CHECK(near(justifyX(Justify::SpaceBetween, 1), 45)); CHECK(near(justifyX(Justify::SpaceBetween, 2), 90));
	CHECK(near(justifyX(Justify::SpaceAround, 0), 70.f / 6)); CHECK(near(justifyX(Justify::SpaceAround, 1), 45)); CHECK(near(justifyX(Justify::SpaceAround, 2), 100 - 70.f / 6 - 10));
	CHECK(near(justifyX(Justify::SpaceEvenly, 0), 17.5f)); CHECK(near(justifyX(Justify::SpaceEvenly, 1), 45)); CHECK(near(justifyX(Justify::SpaceEvenly, 2), 72.5f));
}
TEST(layout_align_items) {
	auto yOf = [](Align a) { LayoutTree t; LayoutStyle rs = box(100, 100); rs.alignItems = a; NodeId root = t.create(LayoutKind::Row, rs), c = t.create(LayoutKind::Leaf, box(10, 20));
	t.append(root, c); t.layout(root, 100, 100); return t.rect(c).y; };
	CHECK(near(yOf(Align::Start), 0)); CHECK(near(yOf(Align::Center), 40)); CHECK(near(yOf(Align::End), 80));
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	NodeId root = t.create(LayoutKind::Row, box(100, 100)), c = t.create(LayoutKind::Leaf), fixed = t.create(LayoutKind::Leaf, box(10, 30));
	t.append(root, c); t.append(root, fixed); t.layout(root, 100, 100);
	CHECK_RECT(t, c, 0, 0, 40, 100);          // auto height stretches
	CHECK_RECT(t, fixed, 40, 0, 10, 30);      // explicit height is respected
	LayoutStyle ss; ss.alignSelf = Align::End; ss.width = Length::px(10); ss.height = Length::px(10);
	NodeId self = t.create(LayoutKind::Leaf, ss); t.append(root, self); t.layout(root, 100, 100);
	CHECK_RECT(t, self, 50, 90, 10, 10);
}
TEST(layout_percent_sizes) {
	LayoutTree t; NodeId root = t.create(LayoutKind::Row, box(200, 100)); LayoutStyle s; s.width = Length::pct(50); s.height = Length::pct(25);
	NodeId c = t.create(LayoutKind::Leaf, s); t.append(root, c); t.layout(root, 200, 100);
	CHECK_RECT(t, c, 0, 0, 100, 25);
}
TEST(layout_absolute_children_use_insets) {
	LayoutTree t; NodeId root = t.create(LayoutKind::Stack, box(200, 100));
	auto abs = [&](Length l, Length tp, Length r, Length b, Length w, Length h) {
		LayoutStyle s; s.position = Position::Absolute; s.left = l; s.top = tp; s.right = r; s.bottom = b; s.width = w; s.height = h;
		NodeId n = t.create(LayoutKind::Leaf, s); t.append(root, n); return n; };
	NodeId tl = abs(Length::px(10), Length::px(20), {}, {}, Length::px(30), Length::px(40));
	NodeId br = abs({}, {}, Length::px(5), Length::px(5), Length::px(20), Length::px(10));
	NodeId st = abs(Length::px(10), Length::px(0), Length::px(10), {}, {}, Length::px(10));
	t.layout(root, 200, 100);
	CHECK_RECT(t, tl, 10, 20, 30, 40); CHECK_RECT(t, br, 175, 85, 20, 10); CHECK_RECT(t, st, 10, 0, 180, 10);
}
TEST(layout_absolute_children_do_not_take_part_in_flow) {
	LayoutTree t; NodeId root = t.create(LayoutKind::Row, box(200, 100)); LayoutStyle as = box(50, 50); as.position = Position::Absolute;
	NodeId a = t.create(LayoutKind::Leaf, as), b = t.create(LayoutKind::Leaf, box(30, 30));
	t.append(root, a); t.append(root, b); t.layout(root, 200, 100);
	CHECK_RECT(t, b, 0, 0, 30, 30);
}
TEST(layout_stack_aligns_children) {
	LayoutTree t; LayoutStyle ss = box(200, 100); ss.alignItems = Align::Center; NodeId root = t.create(LayoutKind::Stack, ss);
	LayoutStyle m = box(50, 50); m.margin = { 10, 0, 0, 0 };
	NodeId a = t.create(LayoutKind::Leaf, box(50, 50)), b = t.create(LayoutKind::Leaf, m);
	t.append(root, a); t.append(root, b); t.layout(root, 200, 100);
	CHECK_RECT(t, a, 75, 25, 50, 50);
	CHECK_RECT(t, b, 10 + (200 - 10 - 50) * 0.5f, 25, 50, 50);   // centred between its margins
}
TEST(layout_grid_fixed_and_fr_tracks_with_spans) {
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	LayoutStyle gs = box(400, 100); gs.colCount = 3; gs.cols[0] = GridTrack::px(100); gs.cols[1] = GridTrack::fr(1); gs.cols[2] = GridTrack::fr(2);
	NodeId g = t.create(LayoutKind::Grid, gs); NodeId k[5];
	for (int i = 0; i < 5; ++i) { LayoutStyle cs; if (i == 3) cs.colSpan = 2; k[i] = t.create(LayoutKind::Leaf, cs); t.append(g, k[i]); }
	t.layout(g, 400, 100);   // cols: 100, fr1 of 300 = 100, fr2 = 200
	CHECK_RECT(t, k[0], 0, 0, 100, 20); CHECK_RECT(t, k[1], 100, 0, 100, 20); CHECK_RECT(t, k[2], 200, 0, 200, 20);
	CHECK_RECT(t, k[3], 0, 20, 200, 20);          // spans columns 0-1 (100 + 100)
	CHECK_RECT(t, k[4], 200, 20, 200, 20);
}
TEST(layout_grid_auto_track_sizes_to_content_and_gaps) {
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	LayoutStyle gs; gs.colCount = 2; gs.cols[0] = GridTrack::autoTrack(); gs.cols[1] = GridTrack::px(50); gs.gap = { 10, 6 };
	LayoutStyle rowS = box(500, 200); rowS.alignItems = Align::Start;   // otherwise the auto-height grid stretches to 200
	NodeId root = t.create(LayoutKind::Row, rowS), g = t.create(LayoutKind::Grid, gs); t.append(root, g);
	NodeId k[4]; for (auto& n : k) { n = t.create(LayoutKind::Leaf); t.append(g, n); }
	t.layout(root, 500, 200);
	CHECK_RECT(t, g, 0, 0, 40 + 10 + 50, 20 + 6 + 20);   // intrinsic: auto col = 40, px col = 50, one gap each way
	CHECK_RECT(t, k[1], 50, 0, 50, 20); CHECK_RECT(t, k[2], 0, 26, 40, 20);
}
TEST(layout_split_proportional_and_resize_preserves_ratios) {
	LayoutTree t; LayoutStyle ss = box(1000, 500); ss.gutter = 4; NodeId sp = t.create(LayoutKind::Split, ss); NodeId k[3];
	f32 wts[3] = { 1, 2, 1 };
	for (int i = 0; i < 3; ++i) { LayoutStyle c; c.splitWeight = wts[i]; k[i] = t.create(LayoutKind::Leaf, c); t.append(sp, k[i]); }
	t.layout(sp, 1000, 500);   // 992 usable
	CHECK_RECT(t, k[0], 0, 0, 248, 500); CHECK_RECT(t, k[1], 252, 0, 496, 500); CHECK_RECT(t, k[2], 752, 0, 248, 500);
	CHECK_EQ(t.gutterCount(sp), 2u);
	CHECK(rectIs("gutter0", t.gutterRect(sp, 0), 248, 0, 4, 500)); CHECK(rectIs("gutter1", t.gutterRect(sp, 1), 748, 0, 4, 500));
	t.layout(sp, 500, 500);    // 492 usable -> same ratios
	CHECK_RECT(t, k[0], 0, 0, 123, 500); CHECK_RECT(t, k[1], 127, 0, 246, 500); CHECK_RECT(t, k[2], 377, 0, 123, 500);   // 127 + 246 + 4 gutter
}
TEST(layout_split_vertical_direction) {
	LayoutTree t; LayoutStyle ss = box(300, 200); ss.splitDir = SplitDir::Vertical; ss.gutter = 0; NodeId sp = t.create(LayoutKind::Split, ss);
	NodeId a = t.create(LayoutKind::Leaf), b = t.create(LayoutKind::Leaf); t.append(sp, a); t.append(sp, b); t.layout(sp, 300, 200);
	CHECK_RECT(t, a, 0, 0, 300, 100); CHECK_RECT(t, b, 0, 100, 300, 100);
}
TEST(layout_split_minimums_win_over_weights) {
	LayoutTree t; LayoutStyle ss = box(100, 10); ss.gutter = 0; NodeId sp = t.create(LayoutKind::Split, ss);
	LayoutStyle c1; c1.splitMin = 70; NodeId a = t.create(LayoutKind::Leaf, c1), b = t.create(LayoutKind::Leaf);
	t.append(sp, a); t.append(sp, b); t.layout(sp, 100, 10);
	CHECK_RECT(t, a, 0, 0, 70, 10); CHECK_RECT(t, b, 70, 0, 30, 10);
	t.layout(sp, 50, 10);      // minimums no longer fit: scaled down, never negative
	CHECK(t.rect(a).w >= 0 && t.rect(b).w >= 0 && near(t.rect(a).w + t.rect(b).w, 50));
}
TEST(layout_split_gutter_drag_moves_only_the_adjacent_pair_and_clamps) {
	LayoutTree t; LayoutStyle ss = box(1000, 100); ss.gutter = 4; NodeId sp = t.create(LayoutKind::Split, ss); NodeId k[3];
	for (auto& n : k) { LayoutStyle c; c.splitMin = 50; n = t.create(LayoutKind::Leaf, c); t.append(sp, n); }
	t.layout(sp, 1000, 100);   // 992/3 each
	const f32 third = 992.f / 3, c0 = t.rect(k[2]).w;
	SplitDrag d = t.splitDragBegin(sp, 0);
	CHECK(d.active);
	t.splitDragUpdate(d, 100); t.layout(sp, 1000, 100);
	CHECK(near(t.rect(k[0]).w, third + 100)); CHECK(near(t.rect(k[1]).w, third - 100)); CHECK(near(t.rect(k[2]).w, c0));   // third pane untouched
	t.splitDragUpdate(d, 100000); t.layout(sp, 1000, 100);        // clamp: neighbour stops at its minimum
	CHECK(near(t.rect(k[1]).w, 50)); CHECK(near(t.rect(k[0]).w, 2 * third - 50));
	t.splitDragUpdate(d, -100000); t.layout(sp, 1000, 100);
	CHECK(near(t.rect(k[0]).w, 50)); CHECK(near(t.rect(k[1]).w, 2 * third - 50));
	t.splitDragUpdate(d, 0); t.layout(sp, 1000, 100);             // returning to the start restores the start exactly (no drift)
	CHECK(near(t.rect(k[0]).w, third)); CHECK(near(t.rect(k[1]).w, third));
	CHECK(rectIs("gutter follows", t.gutterRect(sp, 0), third, 0, 4, 100));
}
TEST(layout_split_drag_when_a_pane_was_pinned_at_its_minimum_does_not_jump) {
	LayoutTree t; LayoutStyle ss = box(100, 10); ss.gutter = 0; NodeId sp = t.create(LayoutKind::Split, ss);
	LayoutStyle c1; c1.splitMin = 70; NodeId a = t.create(LayoutKind::Leaf, c1), b = t.create(LayoutKind::Leaf), c = t.create(LayoutKind::Leaf);
	t.append(sp, a); t.append(sp, b); t.append(sp, c); t.layout(sp, 100, 10);   // weights 1:1:1 but a is pinned at 70
	f32 bBefore = t.rect(b).w, cBefore = t.rect(c).w;
	SplitDrag d = t.splitDragBegin(sp, 1); t.splitDragUpdate(d, 0); t.layout(sp, 100, 10);
	CHECK(near(t.rect(a).w, 70)); CHECK(near(t.rect(b).w, bBefore)); CHECK(near(t.rect(c).w, cBefore));
}
TEST(layout_blender_like_nested_workspace) {
	LayoutTree t; t.setLeafMeasure(leafZero, nullptr);
	NodeId root = t.create(LayoutKind::Column, box(1920, 1080));
	LayoutStyle top = box(1920, 24); NodeId topBar = t.create(LayoutKind::Leaf, top);
	LayoutStyle ws; ws.grow = 1; ws.gutter = 4; NodeId work = t.create(LayoutKind::Split, ws);          // viewport | (outliner / properties)
	LayoutStyle vp; vp.splitWeight = 3; NodeId viewport = t.create(LayoutKind::Leaf, vp);
	LayoutStyle rs; rs.splitWeight = 1; rs.splitDir = SplitDir::Vertical; rs.gutter = 4; NodeId right = t.create(LayoutKind::Split, rs);
	NodeId outliner = t.create(LayoutKind::Leaf), props = t.create(LayoutKind::Leaf);
	LayoutStyle tl = box(1920, 200); NodeId timeline = t.create(LayoutKind::Leaf, tl);
	LayoutStyle sb = box(1920, 22); NodeId status = t.create(LayoutKind::Leaf, sb);
	t.append(root, topBar); t.append(root, work); t.append(root, timeline); t.append(root, status);
	t.append(work, viewport); t.append(work, right); t.append(right, outliner); t.append(right, props);
	t.layout(root, 1920, 1080);
	CHECK_RECT(t, topBar, 0, 0, 1920, 24);
	CHECK_RECT(t, work, 0, 24, 1920, 1080 - 24 - 200 - 22);            // grows into what the fixed rows leave
	f32 usable = 1920 - 4;
	CHECK_RECT(t, viewport, 0, 0, usable * 0.75f, 834);
	CHECK_RECT(t, right, usable * 0.75f + 4, 0, usable * 0.25f, 834);
	CHECK_RECT(t, outliner, 0, 0, usable * 0.25f, (834 - 4) / 2.f);
	CHECK_RECT(t, props, 0, (834 - 4) / 2.f + 4, usable * 0.25f, (834 - 4) / 2.f);
	CHECK_RECT(t, timeline, 0, 858, 1920, 200); CHECK_RECT(t, status, 0, 1058, 1920, 22);
	Rect abs = t.absRect(props);
	CHECK(near(abs.x, usable * 0.75f + 4) && near(abs.y, 24 + (834 - 4) / 2.f + 4));
}
TEST(layout_scroll_keeps_content_size_and_clamps_offset) {
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	LayoutStyle ss = box(200, 100); NodeId sc = t.create(LayoutKind::Scroll, ss); NodeId k[10];
	for (auto& n : k) { LayoutStyle c = box(200, 30); n = t.create(LayoutKind::Leaf, c); t.append(sc, n); }
	t.layout(sc, 200, 100);
	CHECK(near(t.contentSize(sc).y, 300));
	CHECK_RECT(t, k[9], 0, 270, 200, 30);                              // no shrinking inside a scroller
	t.setScrollOffset(sc, { 0, 9999 }); CHECK(near(t.scrollOffset(sc).y, 200));
	t.setScrollOffset(sc, { 0, -5 }); CHECK(near(t.scrollOffset(sc).y, 0));
	t.setScrollOffset(sc, { 0, 50 });
	CHECK(near(t.absRect(k[1]).y, 30 - 50));                           // scroll shifts descendants without relayout
	CHECK(!t.needsLayout(sc));
}
TEST(layout_wrapping_leaf_height_follows_the_width_it_is_given) {
	LayoutTree t; t.setLeafMeasure(leafWrap, nullptr);
	NodeId col = t.create(LayoutKind::Column, box(100, 400)), text = t.create(LayoutKind::Leaf), after = t.create(LayoutKind::Leaf, box(10, 10));
	t.append(col, text); t.append(col, after); t.layout(col, 100, 400);
	CHECK_RECT(t, text, 0, 0, 100, 40);                                // stretched to 100 wide -> two lines
	CHECK_RECT(t, after, 0, 40, 10, 10);
	t.setStyle(col, box(200, 400)); t.layout(col, 200, 400);
	CHECK_RECT(t, text, 0, 0, 200, 20); CHECK_RECT(t, after, 0, 20, 10, 10);   // one line at full width; sibling moves up
}
TEST(layout_auto_sized_container_fits_content) {
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	LayoutStyle cs; cs.padding = Edges::all(5); cs.gap = { 0, 3 };
	LayoutStyle rowS = box(500, 500); rowS.alignItems = Align::Start;
	NodeId root = t.create(LayoutKind::Row, rowS), col = t.create(LayoutKind::Column, cs), a = t.create(LayoutKind::Leaf), b = t.create(LayoutKind::Leaf);
	t.append(root, col); t.append(col, a); t.append(col, b); t.layout(root, 500, 500);
	CHECK(rectIs("auto column", t.rect(col), 0, 0, 40 + 10, 20 + 3 + 20 + 10));
	CHECK_RECT(t, a, 5, 5, 40, 20); CHECK_RECT(t, b, 5, 28, 40, 20);
}

// ---- handles & structure ---------------------------------------------------------------------
TEST(layout_handles_detect_stale_ids_and_subtree_destroy) {
	LayoutTree t; NodeId root = t.create(LayoutKind::Column), a = t.create(LayoutKind::Column), b = t.create(LayoutKind::Leaf);
	t.append(root, a); t.append(a, b);
	CHECK_EQ(t.nodeCount(), 3u);
	t.destroy(a);
	CHECK(!t.alive(a) && !t.alive(b) && t.alive(root)); CHECK_EQ(t.nodeCount(), 1u); CHECK_EQ(t.childCount(root), 0u);
	NodeId reused = t.create(LayoutKind::Leaf);
	CHECK(!t.alive(a) && !t.alive(b) && t.alive(reused));                // old handles stay dead even though slots were reused
}
TEST(layout_structure_rules) {
	LayoutTree t; NodeId root = t.create(LayoutKind::Column), a = t.create(LayoutKind::Column), leaf = t.create(LayoutKind::Leaf), x = t.create(LayoutKind::Leaf);
	CHECK(t.append(root, a)); CHECK(t.append(a, x));
	CHECK(!t.append(a, root));          // cycle
	CHECK(!t.append(leaf, x));          // leaves cannot have children
	CHECK(!t.append(a, a));
	NodeId y = t.create(LayoutKind::Leaf), z = t.create(LayoutKind::Leaf);
	CHECK(t.append(a, z)); CHECK(t.insertBefore(a, y, z));
	CHECK(t.firstChild(a) == x); CHECK(t.nextSibling(x) == y); CHECK(t.nextSibling(y) == z); CHECK_EQ(t.childCount(a), 3u);
	CHECK(t.append(root, y));           // re-parenting moves it
	CHECK_EQ(t.childCount(a), 2u); CHECK(t.parent(y) == root);
}

// ---- incremental layout ---------------------------------------------------------------------
namespace {
	struct Panels { LayoutTree t; NodeId root; NodeId panel[10]; NodeId leaf[10][10]; };
	void buildPanels(Panels& p, bool fixedPanels) {
		p.t.setLeafMeasure(leaf40x20, nullptr);
		p.root = p.t.create(LayoutKind::Column, box(800, 2000));
		for (int i = 0; i < 10; ++i) {
			LayoutStyle ps = fixedPanels ? box(800, 60) : LayoutStyle{}; ps.padding = Edges::all(2);
			p.panel[i] = p.t.create(LayoutKind::Row, ps); p.t.append(p.root, p.panel[i]);
			for (int j = 0; j < 10; ++j) { p.leaf[i][j] = p.t.create(LayoutKind::Leaf); p.t.append(p.panel[i], p.leaf[i][j]); }
		}
		p.t.layout(p.root, 800, 2000);
	}
}
TEST(layout_clean_tree_does_no_work) {
	Panels p; buildPanels(p, false); p.t.resetStats();
	p.t.layout(p.root, 800, 2000);
	CHECK_EQ(p.t.stats().measureCalls, 0u); CHECK_EQ(p.t.stats().nodesArranged, 0u);
	CHECK(!p.t.needsLayout(p.root));
}
TEST(layout_relayout_boundary_confines_work_to_the_fixed_size_panel) {
	Panels p; buildPanels(p, true); p.t.resetStats();
	p.t.markContentChanged(p.leaf[4][3]);
	CHECK(p.t.needsLayout(p.root));
	p.t.layout(p.root, 800, 2000);
	CHECK_EQ(p.t.stats().nodesArranged, 2u);         // the panel (boundary) and the changed leaf; nothing else
	CHECK(p.t.stats().measureCalls <= 12);           // ~one measure per leaf of that panel
}
TEST(layout_auto_sized_change_propagates_up_but_siblings_keep_their_caches) {
	Panels p; buildPanels(p, false); p.t.resetStats();
	p.t.markContentChanged(p.leaf[4][3]);
	p.t.layout(p.root, 800, 2000);
	CHECK_EQ(p.t.stats().nodesArranged, 3u);         // root, panel 4, the leaf; every other panel/leaf is skipped
	CHECK(p.t.stats().measureCalls <= 32);           // far below the 111 nodes in the tree
	CHECK(p.t.stats().measureCacheHits > 0);
}
TEST(layout_resizing_the_root_relayouts_but_reuses_measure_cache) {
	Panels p; buildPanels(p, false); p.t.resetStats();
	p.t.layout(p.root, 900, 2000);
	CHECK(near(p.t.rect(p.panel[0]).w, 900));
	CHECK_EQ(p.t.stats().nodesArranged, 11u);        // root + 10 panels (width changed); the unchanged leaves are skipped
}
TEST(layout_splitter_drag_touches_only_the_two_panes) {
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	NodeId sp = t.create(LayoutKind::Split, box(1200, 400)); NodeId pane[4], leaves[4][20];
	for (int i = 0; i < 4; ++i) {
		pane[i] = t.create(LayoutKind::Column); t.append(sp, pane[i]);
		for (int j = 0; j < 20; ++j) { leaves[i][j] = t.create(LayoutKind::Leaf); t.append(pane[i], leaves[i][j]); }
	}
	t.layout(sp, 1200, 400); t.resetStats();
	SplitDrag d = t.splitDragBegin(sp, 1);
	t.splitDragUpdate(d, 37); t.layout(sp, 1200, 400);
	// split + pane1 + pane2 + their 40 leaves (their width changed). Panes 0 and 3 (and their leaves) are untouched.
	CHECK_EQ(t.stats().nodesArranged, 1u + 2u + 40u);
}
TEST(layout_measure_cache_prevents_exponential_blowup_in_deep_nesting) {
	// 40 alternating Row/Column levels. Without constraint-aware caching the number of measures grows like 3^depth.
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	NodeId root = t.create(LayoutKind::Column, box(500, 500)), cur = root;
	for (int i = 0; i < 40; ++i) { LayoutStyle s; s.padding = Edges::all(1); NodeId n = t.create(i % 2 ? LayoutKind::Row : LayoutKind::Column, s); t.append(cur, n); cur = n; }
	NodeId leaf = t.create(LayoutKind::Leaf); t.append(cur, leaf);
	t.resetStats(); t.layout(root, 500, 500);
	CHECK(t.stats().measureCalls < 40u * 6u);
}
TEST(layout_steady_state_relayout_allocates_nothing) {
	Panels p; buildPanels(p, false);
	for (int i = 0; i < 3; ++i) { p.t.markContentChanged(p.leaf[i][0]); p.t.layout(p.root, 800, 2000); }   // warm scratch buffers
	std::size_t before = xtest::allocCount;
	for (int i = 0; i < 200; ++i) {
		p.t.markContentChanged(p.leaf[i % 10][i % 7]);
		LayoutStyle s = p.t.style(p.panel[i % 10]); s.gap.x = f32(i % 5); p.t.setStyle(p.panel[i % 10], s);
		p.t.layout(p.root, f32(800 + (i & 1)), 2000);
	}
	CHECK_EQ(xtest::allocCount - before, 0u);
}

#include "layout_fuzz.hpp"
TEST(layout_differential_fuzz_incremental_equals_full) {
	int failures = 0;
	for (u64 seed = 1; seed <= 3000 && failures < 3; ++seed) {
		std::string e = layoutfuzz::runCase(seed, 8);
		if (!e.empty()) { std::printf("    seed %llu: %s\n", (unsigned long long)seed, e.c_str()); ++failures; }
	}
	CHECK_EQ(failures, 0);
}
// Every seed that ever exposed a bug, kept as a permanent regression. Root causes, in the order they were found:
//   70, 333                     percentage resolved against an unbounded space, then cached and reused for a bounded one
//   1287, 3815, 3948            min-width clamp inflated a box after its wrapped content was laid out; size-based cache reuse then
//                               returned the old line count
//   15429, 25497, 39929         same size, different heights: results that overflowed the bound they were measured under
//   59844                       a percentage that resolved against an *available* bound rather than the container's final size
//   103179, 119469              clamping inside a descendant must taint every ancestor's cache entry
//   4309, 167438, 195914        content that fits exactly: `bound - padding` lands one ulp under the natural width and a wrapping
//                               measure flips by a whole line (max-content reuse now needs a margin)
TEST(layout_fuzz_regression_seeds) {
	const u64 seeds[] = { 70, 333, 1287, 3815, 3948, 4309, 15429, 25497, 39929, 59844, 103179, 119469, 167438, 195914 };
	for (u64 seed : seeds) {
		std::string e = layoutfuzz::runCase(seed, 10, /*useBasis=*/false);   // the generator as it was when each seed was found
		if (!e.empty()) { std::printf("    seed %llu: %s\n", (unsigned long long)seed, e.c_str()); ++xtest::failures(); }
	}
}
TEST(layout_percent_resolves_against_the_containing_blocks_actual_size) {
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	LayoutStyle ls; ls.width = Length::pct(50); ls.height = Length::pct(25);
	NodeId p1 = t.create(LayoutKind::Row, box(40, 40)), p2 = t.create(LayoutKind::Row, box(200, 80)), l = t.create(LayoutKind::Leaf, ls);
	t.append(p1, l); t.layout(p1, 40, 40);
	CHECK_RECT(t, l, 0, 0, 20, 10);
	t.append(p2, l); t.layout(p2, 200, 80);                         // re-parented: the percentage follows the new container
	CHECK_RECT(t, l, 0, 0, 100, 20);
	t.layout(p2, 100, 80);                                          // resized container
	CHECK_RECT(t, l, 0, 0, 50, 20);
}
TEST(layout_percent_in_a_content_sized_container_behaves_as_auto) {
	// Cyclic percentage (the container sizes itself from the child, the child from the container): treated as auto, like CSS.
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	LayoutStyle ls; ls.width = Length::pct(50);
	LayoutStyle rs; rs.alignItems = Align::Start;
	NodeId root = t.create(LayoutKind::Row, box(500, 100)), box_ = t.create(LayoutKind::Column, rs), l = t.create(LayoutKind::Leaf, ls);
	t.append(root, box_); t.append(box_, l); t.layout(root, 500, 100);
	CHECK(near(t.rect(box_).w, 40));                                // sized by the leaf's intrinsic 40, not by 50% of anything
}

TEST(layout_mark_all_dirty_relayouts_every_node) {
	Panels p; buildPanels(p, false);
	p.t.resetStats(); p.t.markAllDirty(p.root); p.t.layout(p.root, 800, 2000);
	CHECK_EQ(p.t.stats().nodesArranged, p.t.nodeCount());
	CHECK(near(p.t.rect(p.leaf[9][9]).w, 40));                       // results unchanged by a full relayout
	p.t.resetStats(); p.t.layout(p.root, 800, 2000);
	CHECK_EQ(p.t.stats().nodesArranged, 0u);
}

TEST(layout_flex_basis_zero_shares_space_equally_and_skips_content_measurement) {
	LayoutTree t; t.setLeafMeasure(leaf40x20, nullptr);
	NodeId root = t.create(LayoutKind::Row, box(300, 50));
	NodeId a, b;
	for (NodeId* n : { &a, &b }) {
		LayoutStyle s; s.grow = 1; s.basis = Length::px(0); *n = t.create(LayoutKind::Column, s); t.append(root, *n);
		for (int i = 0; i < 8; ++i) t.append(*n, t.create(LayoutKind::Leaf));
	}
	t.resetStats(); t.layout(root, 300, 50);
	CHECK_RECT(t, a, 0, 0, 150, 50); CHECK_RECT(t, b, 150, 0, 150, 50);   // equal shares regardless of content (basis 0)
	LayoutStyle s2; s2.grow = 1; NodeId c = t.create(LayoutKind::Column, s2); t.append(root, c); t.append(c, t.create(LayoutKind::Leaf));
	t.layout(root, 300, 50);                                              // with auto basis the content width (40) counts
	CHECK(t.rect(c).w > 40 - 0.01f && t.rect(a).w < 150);
}
