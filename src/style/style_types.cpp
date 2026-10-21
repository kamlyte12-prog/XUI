#include <xui/style/style_types.hpp>
namespace xui {
	namespace {
		constexpr u16 K(AtomKind k) { return static_cast<u16>(1u << static_cast<int>(k)); }
		constexpr u16 NUM = K(AtomKind::Number), COL = K(AtomKind::Color), ID = K(AtomKind::Ident), STR = K(AtomKind::String),
			CUR = K(AtomKind::Curve), FN = K(AtomKind::Func);
		// Order must match enum PropId.
		constexpr PropInfo kProps[kPropCount] = {
			{"bg", Expand::None, 1, 1, COL | ID},
			{"fg", Expand::None, 1, 1, COL},
			{"border", Expand::None, 1, 3, NUM | ID | COL},
			{"radius", Expand::Sides4, 1, 4, NUM},
			{"pad", Expand::Sides4, 1, 4, NUM | ID},
			{"margin", Expand::Sides4, 1, 4, NUM | ID},
			{"gap", Expand::Pair, 1, 2, NUM},
			{"size", Expand::Pair, 1, 2, NUM | ID},
			{"min-size", Expand::Pair, 1, 2, NUM | ID},
			{"max-size", Expand::Pair, 1, 2, NUM | ID},
			{"min-width", Expand::None, 1, 1, NUM},
			{"max-width", Expand::None, 1, 1, NUM},
			{"min-height", Expand::None, 1, 1, NUM},
			{"max-height", Expand::None, 1, 1, NUM},
			{"opacity", Expand::None, 1, 1, NUM},
			{"shadow", Expand::None, 1, 6, NUM | COL | ID},
			{"glow", Expand::None, 3, 3, NUM | COL},
			{"gradient", Expand::None, 1, 1, FN},
			{"blur", Expand::None, 1, 1, NUM},
			{"transform", Expand::None, 1, 6, NUM | ID | FN},
			{"font", Expand::None, 1, 1, STR | ID},
			{"font-size", Expand::None, 1, 1, NUM},
			{"font-weight", Expand::None, 1, 1, NUM | ID},
			{"letter-spacing", Expand::None, 1, 1, NUM},
			{"align", Expand::None, 1, 2, ID},
			{"justify", Expand::None, 1, 1, ID},
			{"grow", Expand::None, 1, 1, NUM},
			{"shrink", Expand::None, 1, 1, NUM},
			{"cursor", Expand::None, 1, 1, ID},
			{"z-index", Expand::None, 1, 1, NUM},
			{"clip", Expand::None, 1, 1, ID | NUM},
			{"overflow", Expand::None, 1, 2, ID},
			{"animation", Expand::None, 1, 7, ID | NUM | CUR | FN},
			{"animation-keep", Expand::None, 1, 1, ID},
			{"animation-cancel", Expand::None, 1, 1, ID},
			{"scroll-track", Expand::None, 1, 1, COL},
			{"scroll-thumb", Expand::None, 1, 1, COL},
			{"scrollbar-size", Expand::None, 1, 1, NUM},
			{"scrollbar-visibility", Expand::None, 1, 1, ID},
			{"scroll-behavior", Expand::None, 1, 1, ID},
			{"scroll-speed", Expand::None, 1, 1, NUM},
			{"value", Expand::None, 1, 1, NUM},
		};
		constexpr std::string_view kPropNames[kPropCount] = {
			"bg", "fg", "border", "radius", "pad", "margin", "gap", "size", "min-size", "max-size", "min-width", "max-width", "min-height", "max-height", "opacity", "shadow", "glow", "gradient", "blur",
			"transform", "font", "font-size", "font-weight", "letter-spacing", "align", "justify", "grow", "shrink", "cursor",
			"z-index", "clip", "overflow", "animation", "animation-keep", "animation-cancel", "scroll-track", "scroll-thumb", "scrollbar-size",
			"scrollbar-visibility", "scroll-behavior", "scroll-speed", "value" };
		constexpr std::string_view kStateNames[kStateCount] = {
			"resting", "enabled", "readonly", "hover", "focus", "focus-visible", "selected", "checked", "open", "closed", "pressed", "dragging", "loading", "error", "placeholder", "disabled" };
	}
	const PropInfo& propInfo(PropId id) { return kProps[static_cast<int>(id)]; }
	bool findProp(std::string_view n, PropId& out) {
		for (int i = 0; i < kPropCount; ++i) if (kProps[i].name == n) { out = static_cast<PropId>(i); return true; }
		// Familiar stylesheet spellings map onto the compact canonical property ids.
		if (n == "background") { out = PropId::Bg; return true; }
		if (n == "color") { out = PropId::Fg; return true; }
		if (n == "padding") { out = PropId::Pad; return true; }
		if (n == "font-family") { out = PropId::Font; return true; }
		return false;
	}
	std::span<const std::string_view> propNames() { return kPropNames; }
	std::string_view stateName(State s) { return kStateNames[static_cast<int>(s)]; }
	bool findState(std::string_view n, State& out) {
		if (n == "active") { out = State::Pressed; return true; }
		for (int i = 0; i < kStateCount; ++i) if (kStateNames[i] == n) { out = static_cast<State>(i); return true; }
		return false;
	}
	std::span<const std::string_view> stateNames() { return kStateNames; }
	bool atomEqual(const Atom& a, const Atom& b) {
		auto strLike = [](AtomKind k) { return k == AtomKind::Ident || k == AtomKind::String; };
		if (strLike(a.kind) && strLike(b.kind)) return a.bits == b.bits;   // `Primary` == "Primary"
		if (a.kind != b.kind) return false;
		if (a.kind == AtomKind::Number) return a.num == b.num && a.unit == b.unit;
		return a.bits == b.bits;
	}
}
