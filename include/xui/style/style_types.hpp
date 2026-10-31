#pragma once
/// @file style_types.hpp  Compiled-style value types shared by the .xss compiler and the runtime.
#include <xui/core/interner.hpp>
#include <xui/core/small_vector.hpp>
#include <array>
#include <span>
#include <string>
#include <string_view>
namespace xui {
	/// Internal class name used to carry a component part through the existing class matcher.
	/// Authored XSS uses the readable `Component::part` spelling instead.
	namespace detail {
		inline std::string componentPartMarker(std::string_view component, std::string_view part) {
			return "__xui_component_part_" + std::string(component) + "__" + std::string(part);
		}
	}

	enum class Unit : u8 { None, Px, Percent, Em, Rem, Vw, Vh, Dp, Sp, Ms, S, Deg };
	enum class AtomKind : u8 { None, Number, Color, Ident, String, Curve, Func, Slot };

	/// One scalar of a property value. 12 bytes, trivially copyable.
	/// Number: num+unit. Color: bits=0xRRGGBBAA. Ident/String/Func: bits=StrId. Curve: bits=curve index.
	/// Slot: bits=palette slot (theme-dependent value; replaced with the real atom by StyleSheet::resolve()).
	struct Atom {
		AtomKind kind = AtomKind::None;
		Unit unit = Unit::None;
		u16 pad_ = 0;
		f32 num = 0;
		u32 bits = 0;
		static Atom number(f32 v, Unit u = Unit::None) { Atom a; a.kind = AtomKind::Number; a.num = v; a.unit = u; return a; }
		static Atom color(u32 rgba) { Atom a; a.kind = AtomKind::Color; a.bits = rgba; return a; }
		static Atom ident(StrId s) { Atom a; a.kind = AtomKind::Ident; a.bits = s.v; return a; }
		static Atom string(StrId s) { Atom a; a.kind = AtomKind::String; a.bits = s.v; return a; }
		StrId str() const { return StrId{ bits }; }
	};
	bool atomEqual(const Atom& a, const Atom& b);

	/// A property value: up to 8 atoms (shorthands are expanded to their canonical arity).
	struct StyleValue {
		static constexpr int kMaxAtoms = 8;
		u8 count = 0;
		Atom atoms[kMaxAtoms];
	};

	/// Longhand property ids. Canonical atom layouts:
	///  Pad/Margin/Radius: 4 atoms (top,right,bottom,left | tl,tr,br,bl). Gap: 2 (row,col). Size/MinSize/MaxSize: 2 (w,h).
	///  Border: [width, style-ident, color] or [none]. Shadow: [x y blur spread? color inset?].
	///  Gradient: [function-ident, angle, 2-4 evenly spaced colors]. Glow: [blur, spread, color].
	enum class PropId : u8 {
		Bg, Fg, Border, Outline, OutlineOffset, Radius, Pad, Margin, Gap, Size, MinSize, MaxSize, MinWidth, MaxWidth, MinHeight, MaxHeight, Opacity, Shadow, Glow, Gradient, Blur, Transform,
		Font, FontSize, FontWeight, LetterSpacing, Align, Justify, Grow, Shrink, Cursor, ZIndex, Clip, Overflow,
		Animation, AnimationKeep, AnimationCancel, ScrollTrack, ScrollThumb, ScrollbarSize, ScrollbarVisibility, ScrollBehavior, ScrollSpeed, Value, WindowTitle, Count
	};
	constexpr int kPropCount = static_cast<int>(PropId::Count);
	enum class Expand : u8 { None, Sides4, Pair };
	struct PropInfo { std::string_view name; Expand expand; u8 minN, maxN; u16 kindMask; };
	const PropInfo& propInfo(PropId id);
	bool findProp(std::string_view name, PropId& out);
	std::span<const std::string_view> propNames();

	/// Interaction states. Enum order == application priority (later wins). Resting always applies.
	enum class State : u8 { Resting, Enabled, ReadOnly, Hover, Focus, FocusVisible, Selected, Checked, Open, Closed, Pressed, Dragging, Loading, Error, Placeholder, Disabled, Floating, Count };
	constexpr int kStateCount = static_cast<int>(State::Count);
	using StateMask = u32;
	constexpr StateMask stateBit(State s) { return static_cast<StateMask>(1u << static_cast<int>(s)); }
	std::string_view stateName(State s);
	bool findState(std::string_view name, State& out);
	std::span<const std::string_view> stateNames();

	enum class EaseKind : u8 { Linear, EaseIn, EaseOut, EaseInOut, CubicBezier, Spring };
	/// CubicBezier: p = x1,y1,x2,y2.  Spring: p = stiffness, damping, mass.
	struct Easing { EaseKind kind = EaseKind::Linear; f32 p[4] = { 0, 0, 0, 0 }; };

	struct Transition {
		State from = State::Resting, to = State::Hover;
		bool allProps = true;
		PropId prop = PropId::Bg;
		f32 durationMs = 0, delayMs = 0;
		Easing ease;
	};

	struct Declaration { PropId prop = PropId::Bg; StyleValue value; };
	struct SelectorFilter { StrId prop; Atom value; };
	/// Selector: optional type, optional id, any number of classes and [prop: value] filters.
	struct Selector {
		StrId type, id;                        ///< 0 = unspecified
		SmallVector<StrId, 2> classes;
		SmallVector<SelectorFilter, 2> filters;
	};
	/// Fixed precedence (no cascade): id > class > filter > type; equal scores resolved by source order.
	inline u32 selectorScore(const Selector& s) {
		return (s.id.v ? 1000u : 0u) + 100u * static_cast<u32>(s.classes.size()) + 10u * static_cast<u32>(s.filters.size()) + (s.type.v ? 1u : 0u);
	}

	struct Keyframe { f32 t = 0; u32 declFirst = 0, declCount = 0; };
	struct KeyframesDef { StrId name; u32 frameFirst = 0, frameCount = 0; };

	/// Fully resolved property table for one node (scratch buffer; ~5 KB, keep on the stack or in a pool).
	struct PropTable {
		u64 mask = 0;
		StyleValue v[kPropCount];
		bool has(PropId p) const { return (mask >> static_cast<int>(p)) & 1u; }
		const StyleValue& get(PropId p) const { return v[static_cast<int>(p)]; }
	};
}
