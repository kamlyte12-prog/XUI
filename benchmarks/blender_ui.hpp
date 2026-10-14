#pragma once
// Shared by benchmarks and tests: a Blender-shaped UI tree (top bar, tabs, toolbar, viewport, outliner, properties panels,
// timeline, status bar) of ~940 nodes. Not part of the library.
#include <xui/layout/layout.hpp>
#include <vector>
namespace blender {
using namespace xui;
inline Size leafCb(void*, NodeId, Constraint) { return {64, 22}; }       // a label/field of fixed intrinsic size
inline LayoutStyle fixed(f32 w, f32 h) { LayoutStyle s; s.width = Length::px(w); s.height = Length::px(h); return s; }
struct Ui { LayoutTree t; NodeId root, dragSplit, someLeaf; std::vector<NodeId> leaves; };

inline NodeId row(Ui& u, NodeId parent, int n, f32 gap = 4, LayoutStyle base = {}) {
    base.gap = {gap, 0}; base.alignItems = Align::Center;
    NodeId r = u.t.create(LayoutKind::Row, base); u.t.append(parent, r);
    for (int i = 0; i < n; ++i) { NodeId l = u.t.create(LayoutKind::Leaf); u.t.append(r, l); u.leaves.push_back(l); }
    return r;
}
inline void build(Ui& u) {
    u.t.reserve(2048); u.t.setLeafMeasure(leafCb, nullptr);
    LayoutStyle rs; rs.alignItems = Align::Stretch;
    u.root = u.t.create(LayoutKind::Column, rs);
    LayoutStyle bar = fixed(0, 26); bar.width = {}; bar.padding = Edges::all(2);
    row(u, u.root, 14, 4, bar);                                    // top bar: menus
    row(u, u.root, 8, 2, bar);                                     // workspace tabs
    LayoutStyle ws; ws.grow = 1; ws.basis = Length::px(0); ws.gutter = 4;   // flex: 1 -> never measures its panes' content just to size itself
     ws.splitDir = SplitDir::Horizontal;
    NodeId work = u.t.create(LayoutKind::Split, ws); u.t.append(u.root, work); u.dragSplit = work;
    LayoutStyle tool; tool.splitWeight = 0.3f; tool.splitMin = 40; tool.gap = {0, 2}; tool.padding = Edges::all(2);
    NodeId toolbar = u.t.create(LayoutKind::Column, tool); u.t.append(work, toolbar);
    for (int i = 0; i < 12; ++i) { NodeId l = u.t.create(LayoutKind::Leaf); u.t.append(toolbar, l); u.leaves.push_back(l); }
    LayoutStyle vp; vp.splitWeight = 6; vp.splitMin = 100; NodeId viewport = u.t.create(LayoutKind::Stack, vp); u.t.append(work, viewport);
    for (int i = 0; i < 6; ++i) { LayoutStyle o = fixed(60, 20); o.position = Position::Absolute; o.left = Length::px(10.f * i); o.top = Length::px(8.f * i); NodeId l = u.t.create(LayoutKind::Leaf, o); u.t.append(viewport, l); }
    LayoutStyle rt; rt.splitWeight = 2; rt.splitMin = 120; rt.splitDir = SplitDir::Vertical; rt.gutter = 4;
    NodeId right = u.t.create(LayoutKind::Split, rt); u.t.append(work, right);
    LayoutStyle ol; ol.splitWeight = 1; ol.splitMin = 60; NodeId outliner = u.t.create(LayoutKind::Scroll, ol); u.t.append(right, outliner);
    for (int i = 0; i < 110; ++i) row(u, outliner, 1 + (i % 3), 4);  // tree rows
    LayoutStyle pr; pr.splitWeight = 2; pr.splitMin = 60; NodeId props = u.t.create(LayoutKind::Scroll, pr); u.t.append(right, props);
    for (int p = 0; p < 11; ++p) {                                    // collapsible panels
        LayoutStyle ps; ps.gap = {0, 3}; ps.padding = Edges::all(4); NodeId panel = u.t.create(LayoutKind::Column, ps); u.t.append(props, panel);
        row(u, panel, 2, 4);                                           // header
        for (int r = 0; r < 14; ++r) row(u, panel, 2, 6);              // label + field rows
    }
    row(u, u.root, 40, 2, bar);                                    // timeline controls
    row(u, u.root, 12, 6, bar);                                    // status bar
    u.someLeaf = u.leaves[u.leaves.size() / 2];
}
}
