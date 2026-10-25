#include "xtest.hpp"
#include <xui/lang/xui.hpp>
#include <cstdio>
#include <map>
using namespace xui;

// The reference example from the task statement (section 2), byte-for-byte.
static const char* kSpec = R"(
Window(title: "Main Screen", size: 800x600) {
  Column(gap: 8, pad: 16, align: Stretch) {
    Text("Enter your name")
    TextBox[placeholder: "Type here", bind: userName, grow: 1]
    Row(gap: 8, justify: End) {
      Button[label: "Click Me", variant: Primary, press: onSubmit]
    }
  }
}
)";

static Document mustParse(StringInterner& in, const char* src) {
	auto r = parseXui(src, "t.xui", in);
	if (!r) { std::printf("%s", r.error().format().c_str()); ++xtest::failures(); std::abort(); }
	return std::move(*r);
}
static std::string errorsOf(const char* src) {
	StringInterner in; auto r = parseXui(src, "bad.xui", in);
	CHECK(!r);
	return r ? std::string() : r.error().format();
}
static bool has(const std::string& s, const char* n) { return s.find(n) != std::string::npos; }
// Parses `Text(<expr>)` and returns the S-expression of the first argument.
static std::string ex(const char* expr) {
	StringInterner in; std::string src = std::string("Text(") + expr + ")";
	auto r = parseXui(src, "e.xui", in);
	if (!r) { std::printf("%s", r.error().format().c_str()); ++xtest::failures(); return "<error>"; }
	return dumpExpr(*r, in, r->args[0].expr);
}

TEST(xui_spec_example_parses_verbatim_with_expected_structure) {
	StringInterner in; Document d = mustParse(in, kSpec);
	CHECK_EQ(d.roots.size(), 1u);
	const XNode& win = d.nodes[d.roots[0]];
	CHECK(in.view(win.type) == "Window");
	auto wa = d.argsOf(win); CHECK_EQ(wa.size(), 2u);
	CHECK(in.view(wa[0].name) == "title" && dumpExpr(d, in, wa[0].expr) == "\"Main Screen\"");
	CHECK(in.view(wa[1].name) == "size" && dumpExpr(d, in, wa[1].expr) == "800x600");
	CHECK_EQ(d.childrenOf(win).size(), 1u);
	const XNode& col = d.nodes[d.childrenOf(win)[0]];
	CHECK(in.view(col.type) == "Column" && d.argsOf(col).size() == 3 && d.childrenOf(col).size() == 3);
	const XNode& text = d.nodes[d.childrenOf(col)[0]];
	CHECK(in.view(text.type) == "Text" && d.argsOf(text).size() == 1 && d.argsOf(text)[0].name.v == 0);   // positional
	const XNode& tb = d.nodes[d.childrenOf(col)[1]];
	auto tp = d.propsOf(tb); CHECK_EQ(tp.size(), 3u);
	CHECK(tp[1].role == XPropRole::Bind && dumpExpr(d, in, tp[1].expr) == "userName");
	CHECK(tp[2].role == XPropRole::Value && dumpExpr(d, in, tp[2].expr) == "1");
	const XNode& row = d.nodes[d.childrenOf(col)[2]];
	const XNode& btn = d.nodes[d.childrenOf(row)[0]];
	auto bp = d.propsOf(btn); CHECK_EQ(bp.size(), 3u);
	CHECK(bp[2].role == XPropRole::Event && in.view(bp[2].name) == "press" && dumpExpr(d, in, bp[2].expr) == "onSubmit");
}
TEST(xui_spec_example_validates_against_core_schema) {
	StringInterner in;
	Program p; p.files.push_back(mustParse(in, kSpec));
	auto v = validate(p, XuiSchema::withCoreWidgets(), in);
	if (!v) std::printf("%s", v.error().format().c_str());
	CHECK(v.has_value());
}
TEST(xui_expression_precedence_and_associativity) {
	CHECK_EQ(ex("1 + 2 * 3"), std::string("(+ 1 (* 2 3))"));
	CHECK_EQ(ex("(1 + 2) * 3"), std::string("(* (+ 1 2) 3)"));
	CHECK_EQ(ex("10 - 4 - 3"), std::string("(- (- 10 4) 3)"));              // left-assoc
	CHECK_EQ(ex("a || b && c"), std::string("(|| a (&& b c))"));
	CHECK_EQ(ex("a == b && c < d"), std::string("(&& (== a b) (< c d))"));
	CHECK_EQ(ex("-x * 2"), std::string("(* (neg x) 2)"));
	CHECK_EQ(ex("!a.b"), std::string("(! (. a b))"));
	CHECK_EQ(ex("a ? b : c ? d : e"), std::string("(? a b (? c d e))"));    // right-assoc
	CHECK_EQ(ex("n % 2 != 0"), std::string("(!= (% n 2) 0)"));
}
TEST(xui_postfix_member_call_index) {
	CHECK_EQ(ex("user.name"), std::string("(. user name)"));
	CHECK_EQ(ex("items[i + 1].title"), std::string("(. ([] items (+ i 1)) title)"));
	CHECK_EQ(ex("fmt(a, b.c)"), std::string("(call fmt a (. b c))"));
	CHECK_EQ(ex("list.count() > 0"), std::string("(> (call (. list count)) 0)"));
}
TEST(xui_literals_and_units) {
	CHECK_EQ(ex("8px"), std::string("8px"));       CHECK_EQ(ex("50%"), std::string("50%"));
	CHECK_EQ(ex("1.5em"), std::string("1.5em"));   CHECK_EQ(ex("120ms"), std::string("120ms"));
	CHECK_EQ(ex("1920x1080"), std::string("1920x1080"));
	CHECK_EQ(ex("true"), std::string("true"));     CHECK_EQ(ex("#3b82f6"), std::string("#3b82f6ff"));
	CHECK_EQ(ex("\"hi\""), std::string("\"hi\""));
}
TEST(xui_string_interpolation) {
	CHECK_EQ(ex("\"Hello {user.name}!\""), std::string("(interp \"Hello \" (. user name) \"!\")"));
	CHECK_EQ(ex("\"{a}{b}\""), std::string("(interp a b)"));
	CHECK_EQ(ex("\"{n * 2} items\""), std::string("(interp (* n 2) \" items\")"));
	CHECK_EQ(ex("\"literal \\{braces\\}\""), std::string("\"literal {braces}\""));   // escaped braces stay literal
	CHECK_EQ(ex("\"tab\\there\""), std::string("\"tab\there\""));
}
TEST(xui_control_flow_props) {
	StringInterner in;
	Document d = mustParse(in, "Column { Row[if: user.admin && !busy] Row[for: item in items, key: item.id] Row[for: item, i in items] Row[for: i in 0..10] }");
	const XNode& col = d.nodes[d.roots[0]]; auto kids = d.childrenOf(col); CHECK_EQ(kids.size(), 4u);
	auto p0 = d.propsOf(d.nodes[kids[0]])[0];
	CHECK(p0.role == XPropRole::If && dumpExpr(d, in, p0.expr) == "(&& (. user admin) (! busy))");
	auto p1 = d.propsOf(d.nodes[kids[1]]);
	CHECK(p1.size() == 2 && p1[0].role == XPropRole::For && in.view(p1[0].forVar) == "item" && p1[0].forIndex.v == 0 && dumpExpr(d, in, p1[0].expr) == "items");
	CHECK(in.view(p1[1].name) == "key");                                    // the comma ended the `for` prop
	auto p2 = d.propsOf(d.nodes[kids[2]])[0];
	CHECK(in.view(p2.forVar) == "item" && in.view(p2.forIndex) == "i");     // `item, i in items`
	auto p3 = d.propsOf(d.nodes[kids[3]])[0];
	CHECK_EQ(dumpExpr(d, in, p3.expr), std::string("(.. 0 10)"));
}
TEST(xui_components_slots_and_imports) {
	StringInterner in;
	Document d = mustParse(in, R"(
        @import "theme.xui"
        component Card(title, subtitle: "none") {
          Panel(title: title) { Text(subtitle) Slot() Slot(name: "footer") }
        }
        Card("Hi") { Text("body") }
    )");
	CHECK_EQ(d.imports.size(), 1u); CHECK(in.view(d.imports[0].path) == "theme.xui");
	CHECK_EQ(d.components.size(), 1u);
	const XComponent& c = d.components[0];
	CHECK(in.view(c.name) == "Card" && c.paramCount == 2 && d.params[c.paramFirst + 1].defaultExpr != kNoExpr);
	CHECK_EQ(d.childrenOf(d.nodes[c.root]).size(), 3u);
	CHECK_EQ(d.roots.size(), 1u);
}
TEST(xui_comments_and_trailing_commas) {
	StringInterner in;
	Document d = mustParse(in, "// c\nRow(gap: 4,) [id: a, /* x */ class: b,] { /* none */ }");
	CHECK(d.roots.size() == 1 && d.nodes[d.roots[0]].argCount == 1 && d.nodes[d.roots[0]].propCount == 2);
}
TEST(xui_error_positions_and_snippets) {
	std::string e = errorsOf("Column {\n  Text(\"a\"\n}\n");
	CHECK(has(e, "bad.xui:3:1: error: expected ',' or ')' in argument list, found '}'"));
	CHECK(has(e, " 3 | }"));
}
TEST(xui_error_catalogue) {
	CHECK(has(errorsOf("Text(\"abc)"), "unterminated string"));
	CHECK(has(errorsOf("Text(\"{}\")"), "empty interpolation"));
	CHECK(has(errorsOf("Text(\"{a\")"), "unterminated interpolation"));
	CHECK(has(errorsOf("Text(\"{a +}\")"), "expected an expression"));
	CHECK(has(errorsOf("Button[label: \"a\" press: f]"), "expected ',' or ']'"));
	CHECK(has(errorsOf("Button[label: 1, label: 2]"), "duplicate property 'label'"));
	CHECK(has(errorsOf("TextBox[bind: 5]"), "'bind' needs a state path"));
	CHECK(has(errorsOf("Button[press: 5]"), "an event handler must be a function name"));
	CHECK(has(errorsOf("Row[for: item items]"), "expected 'in'"));
	CHECK(has(errorsOf("Text(1pxx)"), "unknown unit 'pxx'"));
	CHECK(has(errorsOf("Text(#ggg)"), "invalid colour '#ggg'"));
	CHECK(has(errorsOf("Slot()"), "Slot() is only valid inside a component"));
	CHECK(has(errorsOf("component A { Row Row }"), "exactly one root node"));
	CHECK(has(errorsOf("component A { Row }\ncomponent A { Row }"), "already defined"));
	CHECK(has(errorsOf("@include \"x\""), "unknown directive"));
	CHECK(has(errorsOf("Column { 42 }"), "expected a widget name"));
	CHECK(has(errorsOf("Text(f 1)"), "expected ',' or ')'"));
	CHECK(has(errorsOf("Text(1(2))"), "only functions can be called"));
	CHECK(has(errorsOf("Text(a ? b)"), "expected ':' in conditional"));
	CHECK(has(errorsOf("Column { Row"), "expected '}' to close the children of Column"));
	CHECK(has(errorsOf("Text(1 @ 2)"), "expected ',' or ')'"));
}
TEST(xui_recovers_and_reports_multiple_errors) {
	std::string e = errorsOf("Column {\n  Button[label: ]\n  Text(\"ok\")\n  Button[press: 1]\n  Text(#zzz)\n}\n");
	CHECK(has(e, "bad.xui:2:")); CHECK(has(e, "bad.xui:4:")); CHECK(has(e, "bad.xui:5:"));   // three independent errors, three lines
}
TEST(xui_interpolation_errors_point_inside_the_string) {
	std::string e = errorsOf("Text(\"Hello {user.}\")");
	CHECK(has(e, "bad.xui:1:"));
	CHECK(has(e, "expected an expression") || has(e, "unexpected"));
	// The caret must be inside the string, not at column 1.
	CHECK(!has(e, "bad.xui:1:1:"));
}

// ---- schema validation -------------------------------------------------------------------------
struct MemFs { std::map<std::string, std::string> files; };
static std::optional<std::string> readMem(void* c, std::string_view p) {
	auto& m = static_cast<MemFs*>(c)->files; auto it = m.find(std::string(p));
	if (it == m.end()) return std::nullopt;
	return it->second;
}
// Goes through loadXui (the real path) so the component registry is populated.
static std::string validateSrc(const char* src) {
	StringInterner in; MemFs fs; fs.files["t.xui"] = src;
	auto p = loadXui("t.xui", readMem, &fs, in);
	if (!p) { std::printf("%s", p.error().format().c_str()); ++xtest::failures(); return "<load error>"; }
	auto v = validate(*p, XuiSchema::withCoreWidgets(), in);
	return v ? std::string() : v.error().format();
}
TEST(xui_schema_typo_gets_did_you_mean_with_caret) {
	std::string e = validateSrc("Column {\n    TextBox[palceholder: \"Type here\", bind: userName]\n}\n");
	CHECK(has(e, "t.xui:2:13: error: unknown property 'palceholder' on TextBox"));
	CHECK(has(e, "did you mean 'placeholder'?")); CHECK(has(e, "^~~~~~~~~~~"));
}
TEST(xui_schema_catalogue) {
	CHECK(has(validateSrc("Buton[label: \"x\"]"), "unknown widget 'Buton'"));
	CHECK(has(validateSrc("Buton[label: \"x\"]"), "did you mean 'Button'?"));
	CHECK(has(validateSrc("Text(\"a\", \"b\")"), "Text takes 1 positional argument(s)"));
	CHECK(has(validateSrc("Text(\"a\", text: \"b\")"), "'text' is given more than once"));
	CHECK(has(validateSrc("Window(titel: \"x\")"), "unknown parameter 'titel'"));
	CHECK(validateSrc("Button[label: \"x\", press: whatever, if: a, bind: b]").empty());   // control/event props always valid
	CHECK(validateSrc("Row[for: i in 0..3] { Text(\"{i}\") }").empty());
	CHECK(has(validateSrc("component A { Slot(bogus: 1) }"), "Slot() only accepts"));
}
TEST(xui_schema_component_params) {
	CHECK(validateSrc("component Card(title, note: \"\") { Panel(title: title) { Slot() } }\nCard(\"a\") { Text(\"b\") }\nCard(title: \"z\", note: \"n\")").empty());
	CHECK(has(validateSrc("component Card(title) { Panel(title: title) }\nCard()"), "missing required parameter 'title' for component Card"));
	CHECK(has(validateSrc("component Card(title) { Panel(title: title) }\nCard(\"a\", \"b\")"), "Card takes 1 positional argument(s)"));
	CHECK(has(validateSrc("component Card(title) { Panel(title: title) }\nCard(titl: \"a\")"), "did you mean 'title'?"));
}

// ---- loader ------------------------------------------------------------------------------------
TEST(xui_loader_resolves_relative_imports_and_components) {
	MemFs fs; fs.files = {
		{"ui/main.xui", "@import \"parts/card.xui\"\n@import \"../shared/btn.xui\"\nColumn { Card(\"x\") FancyButton }"},
		{"ui/parts/card.xui", "@import \"../../shared/btn.xui\"\ncomponent Card(title) { Panel(title: title) { FancyButton } }"},   // diamond import, loaded once
		{"shared/btn.xui", "component FancyButton { Button(label: \"ok\") }"} };
	StringInterner in;
	auto r = loadXui("ui/main.xui", readMem, &fs, in);
	if (!r) std::printf("%s", r.error().format().c_str());
	CHECK(r.has_value());
	if (r) {
		CHECK_EQ(r->files.size(), 3u); CHECK_EQ(r->components.size(), 2u);
		CHECK(r->findComponent(in.intern("Card")) != nullptr);
		CHECK(validate(*r, XuiSchema::withCoreWidgets(), in).has_value());
	}
}
TEST(xui_component_expansion_substitutes_props_slots_and_instance_ids) {
	MemFs fs; fs.files = {
		{"ui/main.xui", R"(@import "components/card.xui"
Window(size: 420x240)[id: "root", class: "root-window"] {
  Card(title: "Welcome", note: "Optional")[id: "welcome", class: "featured"] {
    Text("Caller body")[id: "body"]
    Text("Actions")[slot: "footer", id: "actions"]
  }
})"},
		{"ui/components/card.xui", R"(component Card(title, note: "Default note") {
Panel {
  Text(title)[id: "title", class: "card-title", part: "title"]
  Text(note)[id: "note"]
  Slot()
  Slot(name: "footer") { Text("Fallback footer") }
}
})"} };
	StringInterner in;
	auto program = loadXui("ui/main.xui", readMem, &fs, in);
	CHECK(program.has_value()); if (!program) return;
	auto valid = validate(*program, XuiSchema::withCoreWidgets(), in);
	CHECK(valid.has_value()); if (!valid) return;
	Document expanded; std::string error;
	CHECK(expandXuiComponents(*program, in, expanded, error));
	if (!error.empty()) std::printf("  expansion error: %s\n", error.c_str());
	if (!error.empty()) return;
	CHECK_EQ(expanded.roots.size(), 1u);
	CHECK_EQ(expanded.nodes.size(), 6u); // Window, Panel, title, note, caller body, named footer.
	CHECK_EQ(expanded.childrenOf(expanded.nodes[expanded.roots[0]]).size(), 1u);
	Program expandedProgram; expandedProgram.files.push_back(std::move(expanded));
	CHECK(validate(expandedProgram, XuiSchema::withCoreWidgets(), in).has_value());
	const Document& result = expandedProgram.files.front();
	bool hasRootId = false, hasWelcomeId = false, hasNamespacedTitle = false, hasBody = false, hasActions = false, hasFallback = false;
	bool hasComponentTitlePart = false, strippedPartAttribute = true;
	for (const XNode& node : result.nodes) {
		for (const XProp& prop : result.propsOf(node)) {
			const std::string_view propName = in.view(prop.name);
			if (propName == "part") strippedPartAttribute = false;
			if (propName == "class" && prop.expr < result.exprs.size() && in.view(result.exprs[prop.expr].name).find(detail::componentPartMarker("Card", "title")) != std::string_view::npos)
				hasComponentTitlePart = true;
			if (propName == "id") {
				const std::string_view id = in.view(result.exprs[prop.expr].name);
				hasRootId |= id == "root";
				hasWelcomeId |= id == "welcome";
				hasNamespacedTitle |= id == "welcome.title";
				hasBody |= id == "body";
				hasActions |= id == "actions";
			}
		}
		for (const XArg& arg : result.argsOf(node)) {
			if (arg.expr < result.exprs.size() && result.exprs[arg.expr].kind == XExprKind::String) {
				const std::string_view text = in.view(result.exprs[arg.expr].name);
				hasFallback |= text == "Fallback footer";
			}
		}
	}
	CHECK(hasRootId && hasWelcomeId && hasNamespacedTitle && hasBody && hasActions);
	CHECK(hasComponentTitlePart && strippedPartAttribute);
	CHECK(!hasFallback); // a provided named slot replaces its fallback content.
}
TEST(xui_component_arguments_reject_positional_after_named) {
	MemFs fs; fs.files = {
		{"main.xui", "@import \"card.xui\"\nWindow { Card(title: \"Hello\", \"Subtitle\") }"},
		{"card.xui", "component Card(title, subtitle) { Panel }"}
	};
	StringInterner in; auto program = loadXui("main.xui", readMem, &fs, in);
	CHECK(program.has_value()); if (!program) return;
	auto valid = validate(*program, XuiSchema::withCoreWidgets(), in);
	CHECK(!valid.has_value());
	if (!valid) CHECK(has(valid.error().format(), "positional arguments cannot follow named arguments"));
}
TEST(xui_component_part_is_rejected_outside_component_definition) {
	MemFs fs; fs.files = {
		{"main.xui", "@import \"card.xui\"\nWindow { Card[part: \"title\"] }"},
		{"card.xui", "component Card { Panel }"}
	};
	StringInterner in; auto program = loadXui("main.xui", readMem, &fs, in);
	CHECK(program.has_value()); if (!program) return;
	auto valid = validate(*program, XuiSchema::withCoreWidgets(), in);
	CHECK(!valid.has_value());
	if (!valid) CHECK(has(valid.error().format(), "part` is only valid on widgets inside a component definition"));
}
TEST(xui_component_expansion_rejects_recursive_components) {
	MemFs fs; fs.files = { {"main.xui", "@import \"loop.xui\"\nWindow { Loop }"}, {"loop.xui", "component Loop { Panel { Loop } }"} };
	StringInterner in; auto program = loadXui("main.xui", readMem, &fs, in);
	CHECK(program.has_value()); if (!program) return;
	Document expanded; std::string error;
	CHECK(!expandXuiComponents(*program, in, expanded, error));
	CHECK(has(error, "recursive component expansion"));
}
TEST(xui_loader_detects_cycles_missing_files_and_duplicates) {
	StringInterner in;
	{
		MemFs fs; fs.files = { {"a.xui", "@import \"b.xui\"\nRow"}, {"b.xui", "@import \"a.xui\"\nRow"} };
		auto r = loadXui("a.xui", readMem, &fs, in); CHECK(!r);
		if (!r) { std::string e = r.error().format(); CHECK(has(e, "import cycle: a.xui -> b.xui -> a.xui")); CHECK(has(e, "b.xui:1:1")); }
	}
	{
		MemFs fs; fs.files = { {"a.xui", "@import \"nope.xui\"\nRow"} };
		auto r = loadXui("a.xui", readMem, &fs, in); CHECK(!r); if (!r) CHECK(has(r.error().format(), "cannot read 'nope.xui'"));
	}
	{ MemFs fs; auto r = loadXui("missing.xui", readMem, &fs, in); CHECK(!r); if (!r) CHECK(has(r.error().format(), "cannot read 'missing.xui'")); }
	{
		MemFs fs; fs.files = { {"a.xui", "@import \"b.xui\"\ncomponent X { Row }\nRow"}, {"b.xui", "component X { Row }"} };
		auto r = loadXui("a.xui", readMem, &fs, in); CHECK(!r); if (!r) CHECK(has(r.error().format(), "component 'X' is already defined in a.xui"));
	}
	{
		MemFs fs; fs.files = { {"a.xui", "@import \"b.xui\"\nRow"}, {"b.xui", "Row["} };   // syntax error inside an imported file is reported with that file's name
		auto r = loadXui("a.xui", readMem, &fs, in); CHECK(!r); if (!r) CHECK(has(r.error().format(), "b.xui:1:"));
	}
}

// Regression: found by the mutation fuzzer. A stray '}' at top level made error recovery loop forever.
TEST(xui_regression_stray_closing_brace_terminates) {
	CHECK(has(errorsOf("(\"\"}}"), "error"));
	CHECK(has(errorsOf("}"), "expected a widget name"));
	CHECK(has(errorsOf("Row }\nText(\"x\")\n}"), "error"));
	CHECK(has(errorsOf("]]] ))) }}}"), "error"));
	CHECK(has(errorsOf("Column { ) } }"), "error"));
}

// Regression: pathological nesting used to overflow the stack (segfault). Now it is an ordinary diagnostic.
TEST(xui_regression_deep_nesting_is_rejected_not_crashed) {
	const int n = 100000;
	std::string parens = "Text(" + std::string(n, '(') + "1" + std::string(n, ')') + ")";
	std::string braces; for (int i = 0; i < n; ++i) braces += "Row {"; braces += std::string(n, '}');
	std::string unary = "Text(" + std::string(n, '!') + "x)";
	StringInterner in;
	for (const std::string* s : { &parens, &braces, &unary }) {
		auto r = parseXui(*s, "deep.xui", in);
		CHECK(!r);
		if (!r) CHECK(r.error().format().find("nested too deeply") != std::string::npos);
	}
}
TEST(xui_legitimately_deep_ui_still_parses) {
	std::string src; for (int i = 0; i < 100; ++i) src += "Row {"; src += "Text(\"leaf\")"; src += std::string(100, '}');
	StringInterner in; CHECK(parseXui(src, "ok.xui", in).has_value());
	std::string ex = "Text(" + std::string(50, '(') + "1" + std::string(50, ')') + ")";   // real expressions nest < 10 deep
	CHECK(parseXui(ex, "ok.xui", in).has_value());
}
