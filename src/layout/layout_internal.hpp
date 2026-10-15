#pragma once
// Internal: scratch element types shared by layout_tree.cpp and layout_solve.cpp.
#include <xui/layout/layout.hpp>
#include <cmath>
namespace xui {
	struct LayoutTree::FlexItem {
		u32 node = 0;
		f32 basis = 0, hyp = 0, main = 0, tent = 0, raw = 0, cross = 0;
		f32 minMain = 0, maxMain = kInf, minCross = 0, maxCross = kInf;
		f32 msM = 0, meM = 0, msC = 0, meC = 0;       // margins: main start/end, cross start/end
		f32 grow = 0, shrink = 1;
		f32 measuredMain = 0, measuredCross = 0;
		Align align = Align::Stretch;
		bool frozen = false, stretch = false, haveCross = false;
	};
	struct LayoutTree::GridItem {
		u32 node = 0; i32 col = 0, row = 0; u32 cs = 1, rs = 1;
		f32 ml = 0, mt = 0, mr = 0, mb = 0;
	};
}
