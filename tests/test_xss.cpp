#include "xtest.hpp"
#include <xui/core/color.hpp>
#include <xui/style/xss.hpp>
#include <cstdio>
using namespace xui;

namespace xtest { extern std::size_t allocCount; }

// The reference snippet from the task statement (section 3), byte-for-byte.
static const char* kSpec = R"(
tokens {
  color.bg           = #0b0b0b
  color.fg           = #fafafa
  color.primary      = #3b82f6
  color.primary.hover= #2563eb
  space.2            = 8px
  space.4            = 16px
  radius.md          = 6px
}

Window {
  resting: { bg: color.bg, fg: color.fg }
}

Column {
  resting: { gap: space.2, pad: space.4 }
}

TextBox {
  resting: { bg: color.bg.darken(10%), radius: radius.md, pad: 8px 12px }
  focus:   { border: 1px solid color.primary }
}

Button[variant: Primary] {
  resting:  { bg: color.primary, fg: color.on-primary, radius: radius.md, pad: 8px 16px }
  hover:    { bg: color.primary.hover }
  pressed:  { bg: color.primary.active }
  disabled: { bg: color.muted, opacity: 0.6 }

  transition: resting -> hover   120ms ease-out
  transition: hover   -> pressed  60ms
}
)";

struct Ctx { StringInterner in; };
static StyleSheet mustCompile(StringInterner& in, const char* src) {
	auto r = compileXss(src, "test.xss", in);
	if (!r) { std::printf("%s", r.error().format().c_str()); ++xtest::failures(); std::abort(); }
	return std::move(*r);
}
static std::string errorsOf(const char* src) {
	StringInterner in;
	auto r = compileXss(src, "bad.xss", in);
	CHECK(!r);
	return r ? std::string() : r.error().format();
}
static bool has(const std::string& s, const char* needle) { return s.find(needle) != std::string::npos; }

struct VariantLookup { StrId prop, value; };
static bool lookupVariant(void* c, StrId p, Atom& out) {
	auto* v = static_cast<VariantLookup*>(c);
	if (p != v->prop) return false;
	out = Atom::ident(v->value); return true;
}

TEST(xss_spec_snippet_compiles_verbatim) {
	StringInterner in;
	auto r = compileXss(kSpec, "spec.xss", in);
	if (!r) std::printf("%s", r.error().format().c_str());
	CHECK(r.has_value());
	if (r) { CHECK_EQ(r->ruleCount(), 4u); }
}
TEST(xss_spec_button_resolution_and_states) {
	StringInterner in; StyleSheet sh = mustCompile(in, kSpec);
	VariantLookup vl{ in.intern("variant"), in.intern("Primary") };
	MatchInput mi; mi.type = in.intern("Button"); mi.lookup = lookupVariant; mi.ctx = &vl;
	StyleSetId set = sh.match(mi);
	CHECK(set.valid());
	PropTable t;
	sh.resolve(set, 0, 0, t);
	CHECK(t.has(PropId::Bg) && t.get(PropId::Bg).atoms[0].bits == 0x3b82f6ffu);
	const StyleValue& pad = t.get(PropId::Pad);
	CHECK_EQ(pad.count, 4);                                   // `8px 16px` expanded to t r b l
	CHECK(pad.atoms[0].num == 8 && pad.atoms[1].num == 16 && pad.atoms[2].num == 8 && pad.atoms[3].num == 16);
	CHECK(pad.atoms[0].unit == Unit::Px);
	CHECK_EQ(t.get(PropId::Fg).atoms[0].bits, 0xffffffffu);   // base default color.on-primary
	sh.resolve(set, stateBit(State::Hover), 0, t);
	CHECK_EQ(t.get(PropId::Bg).atoms[0].bits, 0x2563ebffu);
	CHECK(t.get(PropId::Radius).atoms[2].num == 6);           // still inherited from resting
	sh.resolve(set, stateBit(State::Hover) | stateBit(State::Pressed), 0, t);
	CHECK_EQ(t.get(PropId::Bg).atoms[0].bits, 0x1d4ed8ffu);   // pressed outranks hover
	sh.resolve(set, stateBit(State::Disabled), 0, t);
	CHECK(t.get(PropId::Opacity).atoms[0].num == 0.6f);
	CHECK_EQ(t.get(PropId::Bg).atoms[0].bits, 0x52525bffu);
}
TEST(xss_filter_mismatch_does_not_match) {
	StringInterner in; StyleSheet sh = mustCompile(in, kSpec);
	VariantLookup vl{ in.intern("variant"), in.intern("Secondary") };
	MatchInput mi; mi.type = in.intern("Button"); mi.lookup = lookupVariant; mi.ctx = &vl;
	PropTable t; sh.resolve(sh.match(mi), 0, 0, t);
	CHECK(!t.has(PropId::Bg));
	mi.lookup = nullptr; sh.resolve(sh.match(mi), 0, 0, t);   // no lookup at all -> no match either
	CHECK(!t.has(PropId::Bg));
}
TEST(xss_transitions_parsed) {
	StringInterner in; StyleSheet sh = mustCompile(in, kSpec);
	VariantLookup vl{ in.intern("variant"), in.intern("Primary") };
	MatchInput mi; mi.type = in.intern("Button"); mi.lookup = lookupVariant; mi.ctx = &vl;
	auto tr = sh.transitions(sh.match(mi));
	CHECK_EQ(tr.size(), 2u);
	if (tr.size() == 2) {
		CHECK(tr[0].from == State::Resting && tr[0].to == State::Hover && tr[0].durationMs == 120 && tr[0].ease.kind == EaseKind::EaseOut);
		CHECK(tr[1].from == State::Hover && tr[1].to == State::Pressed && tr[1].durationMs == 60 && tr[1].ease.kind == EaseKind::Linear);
	}
}
TEST(xss_derived_token_darken) {
	StringInterner in; StyleSheet sh = mustCompile(in, kSpec);
	MatchInput mi; mi.type = in.intern("TextBox");
	PropTable t; sh.resolve(sh.match(mi), 0, 0, t);
	Color base = Color::fromRGBA8(0x0b0b0bff), got = Color::fromRGBA8(t.get(PropId::Bg).atoms[0].bits);
	CHECK(toOKLab(got).L < toOKLab(base).L);
	CHECK_EQ(t.get(PropId::Pad).atoms[1].num, 12.f);          // 8px 12px -> r = 12
	sh.resolve(sh.match(mi), stateBit(State::Focus), 0, t);
	CHECK_EQ(t.get(PropId::Border).count, 3);                 // 1px solid <colour>
	CHECK_EQ(t.get(PropId::Border).atoms[2].bits, 0x3b82f6ffu);
}
TEST(xss_tokens_alias_and_arithmetic) {
	StringInterner in;
	StyleSheet sh = mustCompile(in, R"(tokens {
        space.2 = 8px
        space.4 = space.2 * 2
        gap.alias = space.4
        half = space.4 / 2
        sum = space.2 + 4px
        neg = -4px
        accent = #3b82f6
        accent.soft = accent.alpha(50%)
        accent.mixed = accent.mix(#ffffff, 25%)
    })");
	auto tok = [&](const char* n) { return sh.token(in.intern(n)); };
	CHECK(tok("space.4") && tok("space.4")->num == 16 && tok("space.4")->unit == Unit::Px);
	CHECK(tok("gap.alias") && tok("gap.alias")->num == 16);
	CHECK(tok("half") && tok("half")->num == 8);
	CHECK(tok("sum") && tok("sum")->num == 12);
	CHECK(tok("neg") && tok("neg")->num == -4);
	CHECK(tok("accent.soft") && (tok("accent.soft")->bits & 0xff) == 128);
	CHECK(tok("accent.mixed") && tok("accent.mixed")->bits != tok("accent")->bits);
	CHECK(!tok("nope"));
}
TEST(xss_precedence_id_class_filter_type) {
	StringInterner in;
	StyleSheet sh = mustCompile(in, R"(
        Button { resting: { opacity: 0.1 } }
        .big   { resting: { opacity: 0.2 } }
        Button[variant: Primary] { resting: { opacity: 0.3 } }
        #ok    { resting: { opacity: 0.4 } }
        .big   { resting: { opacity: 0.25 } }
    )");
	VariantLookup vl{ in.intern("variant"), in.intern("Primary") };
	StrId big = in.intern("big");
	auto opacity = [&](bool withClass, bool withId, bool withVariant) {
		MatchInput mi; mi.type = in.intern("Button");
		StrId cls[1] = { big }; if (withClass) mi.classes = cls;
		if (withId) mi.id = in.intern("ok");
		if (withVariant) { mi.lookup = lookupVariant; mi.ctx = &vl; }
		PropTable t; sh.resolve(sh.match(mi), 0, 0, t); return t.get(PropId::Opacity).atoms[0].num;
		};
	CHECK_EQ(opacity(false, false, false), 0.1f);
	CHECK_EQ(opacity(false, false, true), 0.3f);   // filter beats bare type
	CHECK_EQ(opacity(true, false, true), 0.25f);   // class beats filter; later .big wins over earlier .big
	CHECK_EQ(opacity(true, true, true), 0.4f);     // id beats everything
}
TEST(xss_style_sets_are_shared) {
	StringInterner in; StyleSheet sh = mustCompile(in, kSpec);
	MatchInput mi; mi.type = in.intern("Column");
	StyleSetId a = sh.match(mi), b = sh.match(mi);
	CHECK(a == b); CHECK_EQ(sh.setCount(), 1u);
	PropTable t; sh.resolve(a, 0, 0, t);
	CHECK(t.get(PropId::Gap).count == 2 && t.get(PropId::Gap).atoms[1].num == 8);
}
TEST(xss_theme_switch_uses_palette_no_recompile) {
	StringInterner in;
	StyleSheet sh = mustCompile(in, R"(
        tokens { color.bg = #ffffff  color.panel = color.bg.darken(10%)  space = 4px }
        @theme dark { tokens { color.bg = #101010 } }
        Panel { resting: { bg: color.panel, gap: space, fg: #123456 } }
    )");
	CHECK_EQ(sh.themeCount(), 2u);
	auto dark = sh.themeIndex(in.intern("dark")); CHECK(dark.has_value());
	MatchInput mi; mi.type = in.intern("Panel");
	StyleSetId s = sh.match(mi);
	PropTable light, dk;
	sh.resolve(s, 0, 0, light); sh.resolve(s, 0, *dark, dk);
	Color l = Color::fromRGBA8(light.get(PropId::Bg).atoms[0].bits), d = Color::fromRGBA8(dk.get(PropId::Bg).atoms[0].bits);
	CHECK(toOKLab(l).L > 0.8f);              // derived from white
	CHECK(toOKLab(d).L < 0.3f);              // derived from #101010 - same rule, different palette
	CHECK_EQ(light.get(PropId::Gap).atoms[0].kind, AtomKind::Number);       // non-themed values are not slots
	CHECK_EQ(light.get(PropId::Fg).atoms[0].bits, dk.get(PropId::Fg).atoms[0].bits);
}
TEST(xss_resolve_performs_zero_heap_allocations) {
	StringInterner in; StyleSheet sh = mustCompile(in, kSpec);
	VariantLookup vl{ in.intern("variant"), in.intern("Primary") };
	MatchInput mi; mi.type = in.intern("Button"); mi.lookup = lookupVariant; mi.ctx = &vl;
	StyleSetId s = sh.match(mi);
	static PropTable t;
	std::size_t before = xtest::allocCount;
	for (int i = 0; i < 10000; ++i) sh.resolve(s, (i & 1) ? stateBit(State::Hover) : stateBit(State::Pressed), 0, t);
	CHECK_EQ(xtest::allocCount - before, 0u);
}
TEST(xss_keyframes_and_animation) {
	StringInterner in;
	StyleSheet sh = mustCompile(in, R"(
        keyframes pulse { to { opacity: 0.4 } from { opacity: 1 } 50% { opacity: 0.7, blur: 2px } }
        Spinner { resting: { animation: pulse 1s cubic-bezier(0.4, 0, 0.2, 1) infinite } }
    )");
	const KeyframesDef* k = sh.findKeyframes(in.intern("pulse"));
	CHECK(k != nullptr);
	if (k) {
		auto fr = sh.frames(*k);
		CHECK_EQ(fr.size(), 3u);
		CHECK(fr[0].t == 0.f && fr[1].t == 0.5f && fr[2].t == 1.f);   // sorted by offset
		CHECK_EQ(sh.frameDecls(fr[1]).size(), 2u);
	}
	MatchInput mi; mi.type = in.intern("Spinner"); PropTable t; sh.resolve(sh.match(mi), 0, 0, t);
	const StyleValue& a = t.get(PropId::Animation);
	CHECK_EQ(a.count, 4);
	CHECK(a.atoms[1].unit == Unit::S && a.atoms[2].kind == AtomKind::Curve);
	CHECK(sh.curve(a.atoms[2].bits).kind == EaseKind::CubicBezier && sh.curve(a.atoms[2].bits).p[3] == 1.f);
}

TEST(xss_gradients_glow_alpha_and_existing_typography_compile) {
	StringInterner in;
	auto styles = mustCompile(in, R"(
Button.demo {
  resting: {
    gradient: linear-gradient(25deg, #102030, #405060, #708090, #a0b0c0),
    glow: 12px 2px #4488cc80,
    bg: #11223344,
    opacity: 0.6,
    font-size: 18px,
    font-weight: 700
  }
}
Panel.radial { resting: { gradient: radial-gradient(#ffffff, #000000) } }
Panel.conic { resting: { gradient: conic-gradient(45deg, #ff0000, #00ff00, #0000ff) } }
)");
	MatchInput match; match.type = in.intern("Button");
	const StrId klass = in.intern("demo"); match.classes = std::span<const StrId>(&klass, 1);
	PropTable table; styles.resolve(styles.match(match), stateBit(State::Resting), 0, table);
	CHECK(table.has(PropId::Gradient)); CHECK_EQ(table.get(PropId::Gradient).count, 6);
	CHECK(in.view(table.get(PropId::Gradient).atoms[0].str()) == "linear-gradient");
	CHECK_NEAR(table.get(PropId::Gradient).atoms[1].num, 25.f * 0.0174532925f, 1e-5f);
	CHECK(table.has(PropId::Glow) && table.get(PropId::Glow).count == 3);
	CHECK(table.has(PropId::Bg) && table.get(PropId::Bg).atoms[0].bits == 0x11223344u);
	CHECK(table.has(PropId::Opacity) && table.has(PropId::FontSize) && table.has(PropId::FontWeight));
	CHECK(!errorsOf("Panel { resting: { gradient: radial-gradient(#fff) } }").empty());
}
TEST(xss_transform_functions_flatten) {
	StringInterner in;
	StyleSheet sh = mustCompile(in, "Card { hover: { transform: translate(0px, -2px) scale(1.02) } }");
	MatchInput mi; mi.type = in.intern("Card"); PropTable t; sh.resolve(sh.match(mi), stateBit(State::Hover), 0, t);
	const StyleValue& v = t.get(PropId::Transform);
	CHECK_EQ(v.count, 5);   // Func Num Num Func Num
	CHECK(v.atoms[0].kind == AtomKind::Func && in.view(v.atoms[0].str()) == "translate" && v.atoms[2].num == -2.f);
}
TEST(xss_comments_and_multiple_selectors) {
	StringInterner in;
	StyleSheet sh = mustCompile(in, "/* block */ Row, Column { // line\n resting: { gap: 4px } }");
	CHECK_EQ(sh.ruleCount(), 2u);
}
TEST(xss_error_unknown_property_has_caret_and_suggestion) {
	std::string e = errorsOf("Button {\n  resting: { bgg: #fff }\n}\n");
	CHECK(has(e, "bad.xss:2:14: error: unknown property 'bgg'"));
	CHECK(has(e, "did you mean 'bg'?")); CHECK(has(e, "^~~"));
}
TEST(xss_error_reports_several_errors_in_one_pass) {
	std::string e = errorsOf("A { resting: { bgg: #fff, opacty: 1 } }\nB { resting: { fg: #zzz } }\n");
	CHECK(has(e, "bgg")); CHECK(has(e, "opacty")); CHECK(has(e, "invalid colour '#zzz'"));
}
TEST(xss_error_messages) {
	CHECK(has(errorsOf("A { resting: { gap: 4pxx } }"), "unknown unit 'pxx'"));
	CHECK(has(errorsOf("A { hovr: { bg: #fff } }"), "did you mean 'hover'?"));
	CHECK(has(errorsOf("A { resting: { bg: color.nope } }"), "unknown token 'color.nope'"));
	CHECK(has(errorsOf("A { resting: { opacity: #fff } }"), "'opacity' does not accept a colour"));
	CHECK(has(errorsOf("A { resting: { pad: 1px 2px 3px 4px 5px } }"), "takes 1 to 4 value(s), got 5"));
	CHECK(has(errorsOf("tokens { a = 1px\n b = a + 1em }\n"), "different units"));
	CHECK(has(errorsOf("tokens { a = b\n b = a.x }\nA { resting: { gap: a } }"), "unknown token"));
	CHECK(has(errorsOf("tokens { a = #fff.frobnicate(1) }"), "unknown colour method"));
	CHECK(has(errorsOf("tokens { a = #fff.darken(1px) }"), "expected a percentage"));
	CHECK(has(errorsOf("A { resting: { animation: nothere 1s } }"), "unknown keyframes 'nothere'"));
	CHECK(has(errorsOf("A { transition: resting -> hover }"), "needs a duration"));
	CHECK(has(errorsOf("A { resting: { bg: #fff\n fg: #000 } }"), "separate declarations with ','"));
	CHECK(has(errorsOf("A { resting: { bg: #fff }"), "expected '}'"));
	CHECK(has(errorsOf("@thme x { }"), "unknown at-rule"));
}
TEST(xss_circular_tokens_detected) {
	CHECK(has(errorsOf("tokens { a = b\n b = a }\n"), "circular token reference"));
}
TEST(xss_errors_are_printed_in_source_order) {
	std::string e = errorsOf("A { resting: { fg: #zzz, gap: 4pxx } }\nB { resting: { bgg: #fff } }\n");
	CHECK(e.find("invalid colour") < e.find("unknown unit"));
	CHECK(e.find("unknown unit") < e.find("unknown property"));
}
TEST(xss_regression_stray_punctuation_terminates) {
	CHECK(has(errorsOf("}"), "error"));
	CHECK(has(errorsOf("}}} ))) ]]]"), "error"));
	CHECK(has(errorsOf("A { ) } }"), "error"));
	CHECK(has(errorsOf("A { resting: { ) ] } }"), "error"));
	CHECK(has(errorsOf("keyframes k { } } }"), "error"));
}
TEST(xss_regression_deep_nesting_is_rejected_not_crashed) {
	const int n = 100000;
	CHECK(has(errorsOf(("tokens { a = " + std::string(n, '(') + "1" + std::string(n, ')') + " }").c_str()), "nested too deeply"));
	CHECK(has(errorsOf(("tokens { a = " + std::string(n, '-') + "1 }").c_str()), "nested too deeply"));
	std::string ok = "tokens { a = " + std::string(50, '(') + "1px" + std::string(50, ')') + " }";
	StringInterner in; CHECK(compileXss(ok, "ok.xss", in).has_value());
}
