#pragma once
// Internal: scratch element types shared by layout_tree.cpp and layout_solve.cpp.
#include <xui/layout/layout.hpp>
#include <cmath>
namespace xui {
	inline Edges contentInsets(const LayoutStyle& style) {
		return { style.padding.l + style.border.l, style.padding.t + style.border.t,
			style.padding.r + style.border.r, style.padding.b + style.border.b };
	}
	inline f32 layoutMin(const LayoutStyle& style, bool horizontal, f32 basis) {
		const f32 legacy = horizontal ? style.minW : style.minH;
		const Length& constraint = horizontal ? style.minWidth : style.minHeight;
		if (constraint.k == LenKind::Auto) return legacy;
		const f32 value = constraint.k == LenKind::Percent
			? (std::isfinite(basis) ? std::max(0.f, basis * constraint.v * 0.01f) : legacy)
			: std::max(0.f, constraint.v);
		return std::max(legacy, value);
	}
	inline f32 layoutMax(const LayoutStyle& style, bool horizontal, f32 basis) {
		const f32 legacy = horizontal ? style.maxW : style.maxH;
		const Length& constraint = horizontal ? style.maxWidth : style.maxHeight;
		if (constraint.k == LenKind::Auto) return legacy;
		const f32 value = constraint.k == LenKind::Percent
			? (std::isfinite(basis) ? std::max(0.f, basis * constraint.v * 0.01f) : legacy)
			: std::max(0.f, constraint.v);
		return std::min(legacy, value);
	}
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
		bool explicitCell = false;
	};
}
