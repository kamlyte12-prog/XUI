#include "xss_ast.hpp"
#include <xui/core/color.hpp>
#include <xui/style/xss.hpp>
#include <algorithm>
#include <unordered_set>
namespace xui {
	namespace {
		const char* kindName(AtomKind k) {
			switch (k) {
			case AtomKind::Number: return "a number"; case AtomKind::Color: return "a colour"; case AtomKind::Ident: return "an identifier";
			case AtomKind::String: return "a string"; case AtomKind::Curve: return "an easing curve"; case AtomKind::Func: return "a function";
			default: return "a value";
			}
		}
		// Built-in defaults so the reference snippet works verbatim; user tokens override them (later definitions win).
		constexpr std::string_view kBaseTokens = R"(tokens {
  color.on-primary = #ffffff
  color.primary = #3b82f6
  color.primary.hover = #2563eb
  color.primary.active = #1d4ed8
  color.muted = #52525b
  color.border = #27272a
  color.shadow = #00000066
})";
		// Default Fluent 2 light controls are expressed in the same stylesheet language as user styles.
		// These rules are compiled before the caller's source, so authored selectors always override them.
		constexpr std::string_view kFluent2Defaults = R"(
Window { resting: { bg: #f5f5f5, fg: #242424, font-size: 14px } }
Text, Label { resting: { fg: #242424, font-size: 14px } }
Column, Row, Split, CheckList { resting: { fg: #242424 } }
Panel { resting: { bg: #ffffff, fg: #242424, border: 1px solid #e1e1e1, radius: 8px, pad: 12px } }
Button, ToggleButton, MenuItem { resting: { bg: #f5f5f5, fg: #242424, border: 1px solid #d1d1d1, radius: 4px, pad: 6px 12px }
  hover: { bg: #ebebeb } pressed: { bg: #e1e1e1 } disabled: { opacity: 0.45 } }
TextBox, TextArea, NumberField, SearchField, KeybindField, Dropdown, DatePicker, ColorPicker {
  resting: { bg: #ffffff, fg: #242424, border: 1px solid #8a8886, radius: 4px, pad: 6px 8px }
  hover: { border: 1px solid #605e5c } focus: { border: 1px solid #0f6cbd }
  placeholder: { fg: #616161 } disabled: { opacity: 0.45 }
}
CheckItem, CheckBox, Switch { resting: { fg: #242424, radius: 4px }
  hover: { bg: #f0f0f0 } checked: { fg: #0f6cbd } disabled: { opacity: 0.45 } }
RadioGroup, SegmentedControl, Rating { resting: { bg: #ffffff, fg: #242424, border: 1px solid #d1d1d1, radius: 4px, pad: 6px }
  hover: { fg: #0f6cbd } checked: { fg: #0f6cbd } disabled: { opacity: 0.45 } }
Slider, ProgressBar { resting: { bg: #d1d1d1, fg: #0f6cbd, radius: 4px } disabled: { opacity: 0.45 } }
Badge { resting: { bg: #f0f0f0, fg: #424242, border: 1px solid #e1e1e1, radius: 4px, pad: 4px 8px, font-size: 12px } }
Toolbar, StatusBar { resting: { bg: #f0f0f0, fg: #424242, border: 1px solid #e1e1e1, radius: 4px, pad: 8px, gap: 8px } }
FpsCounter { resting: { bg: #ffffff, fg: #616161, border: 1px solid #e1e1e1, radius: 4px, pad: 4px 8px, font-size: 12px } }
Image { resting: { bg: #f0f0f0, fg: #616161, border: 1px solid #d1d1d1, radius: 4px } }
Icon { resting: { fg: #424242 } disabled: { opacity: 0.45 } }
TreeView { resting: { bg: #ffffff, fg: #242424, border: 1px solid #d1d1d1, radius: 4px, pad: 4px, font-size: 14px }
  hover: { border: 1px solid #8a8886 } disabled: { opacity: 0.45 } }
Spinner { resting: { fg: #0f6cbd } disabled: { opacity: 0.45 } }
Separator, Divider { resting: { fg: #d1d1d1 } }
Breadcrumb { resting: { fg: #424242 } hover: { fg: #0f6cbd } selected: { fg: #0f6cbd } disabled: { opacity: 0.45 } }
Stepper { resting: { bg: #ffffff, fg: #242424, border: 1px solid #8a8886, radius: 4px, pad: 6px 8px } hover: { border: 1px solid #605e5c } focus: { border: 1px solid #0f6cbd } error: { border: 1px solid #c42b1c } disabled: { opacity: 0.45 } }
NumberButton { resting: { bg: #f5f5f5, fg: #424242, radius: 4px } hover: { bg: #ebebeb } pressed: { bg: #e1e1e1 } disabled: { opacity: 0.45 } }
Tabs { resting: { bg: #ffffff, fg: #242424, border: 1px solid #d1d1d1, radius: 6px } }
Tab { resting: { bg: #f5f5f5, fg: #616161, pad: 8px }
  hover: { bg: #ebebeb, fg: #242424 } selected: { fg: #0f6cbd } }
ScrollView { resting: { bg: #ffffff, fg: #242424, border: 1px solid #d1d1d1, radius: 4px, scrollbar-size: 10px,
  scroll-track: #f5f5f5, scroll-thumb: #a19f9d } hover: { scroll-thumb: #797775 } }
Menu { resting: { bg: #ffffff, fg: #242424, border: 1px solid #d1d1d1, radius: 6px, shadow: 0px 4px 14px 0px #00000024 } }
Tooltip { resting: { bg: #292929, fg: #ffffff, border: 1px solid #292929, radius: 4px, pad: 6px 8px, font-size: 12px } }
TitleBar { resting: { bg: #f5f5f5, fg: #242424, pad: 4px, gap: 4px } }
TitleButton { resting: { fg: #424242, radius: 4px } hover: { bg: #e5e5e5 } pressed: { bg: #d6d6d6 } }
DockHost { resting: { bg: #f5f5f5, fg: #242424, radius: 6px, pad: 4px } }
DockPanel { resting: { bg: #ffffff, fg: #242424, border: 1px solid #d1d1d1, radius: 6px, pad: 8px, shadow: 0px 4px 14px 0px #00000024 } }
DockHeader { resting: { bg: #f0f0f0, fg: #424242, border: 1px solid #e1e1e1, radius: 4px, pad: 8px }
  hover: { bg: #ebebeb } selected: { fg: #0f6cbd, font-weight: 600 } }
DockTab { resting: { bg: #f5f5f5, fg: #616161, pad: 6px } hover: { bg: #ebebeb } selected: { bg: #ffffff, fg: #0f6cbd, font-weight: 600 } }
DockCloseButton { resting: { fg: #616161 } hover: { bg: #c42b1c, fg: #ffffff } }
DockSplitter { resting: { bg: #d1d1d1 } hover: { bg: #0f6cbd } pressed: { bg: #115ea3 } }
DockPreview { resting: { bg: #0f6cbd44, border: 1px solid #0f6cbd, radius: 4px } }
Button, ToggleButton, CheckBox, CheckItem, Switch, RadioGroup, TextBox, TextArea, NumberField, SearchField,
Dropdown, DatePicker, ColorPicker, Slider, Tabs, Tab, Menu, MenuItem, KeybindField, SegmentedControl, Stepper, Rating, Breadcrumb {
  focus-visible: { outline: 2px solid #0f6cbd, outline-offset: 2px }
}
)";
	}

	class XssCompiler {
	public:
		XssCompiler(xss::Ast& ast, StringInterner& in, DiagnosticList& d, StyleSheet& sh) : m_a(ast), m_in(in), m_d(d), m_sh(sh) {}

		void run() {
			const std::size_t nThemes = m_a.themes.size();
			for (const auto& t : m_a.themes) m_sh.m_themeNames.push_back(t.name);
			for (std::size_t t = 1; t < nThemes; ++t) for (const auto& def : m_a.themes[t].tokens) m_themed.insert(def.name);
			m_themedMemo.assign(m_a.exprs.size(), -1);

			// Build one environment per theme: base tokens, then that theme's overrides.
			m_env.resize(nThemes);
			m_sh.m_tokens.resize(nThemes);
			for (std::size_t t = 0; t < nThemes; ++t) {
				Env& e = m_env[t];
				for (const auto& def : m_a.themes[0].tokens) define(e, def);
				if (t > 0) for (const auto& def : m_a.themes[t].tokens) define(e, def);
				for (StrId n : e.order) if (auto a = evalToken(e, n)) m_sh.m_tokens[t][n] = *a;
			}
			for (const auto& k : m_a.keyframes) compileKeyframes(k);
			for (const auto& r : m_a.rules) compileRule(r);
			for (const Pending& p : m_pending)
				if (!m_sh.findKeyframes(p.name)) {
					std::vector<std::string_view> names; for (const auto& k : m_sh.m_kf) names.push_back(m_in.view(k.name));
					std::string s = didYouMean(m_in.view(p.name), names);
					m_d.error(p.off, p.len, "unknown keyframes '" + std::string(m_in.view(p.name)) + "'", s.empty() ? "define it with `keyframes name { ... }`" : "did you mean '" + s + "'?");
				}
			// Palette: evaluate every theme-dependent slot once per theme. Theme switch = pointer offset at resolve time.
			m_sh.m_slotCount = static_cast<u32>(m_slots.size());
			m_sh.m_palette.assign(nThemes * m_slots.size(), Atom{});
			for (std::size_t t = 0; t < nThemes; ++t)
				for (std::size_t i = 0; i < m_slots.size(); ++i)
					if (auto a = evalExpr(m_slots[i], m_env[t])) m_sh.m_palette[t * m_slots.size() + i] = *a;
		}

	private:
		struct Env { std::unordered_map<StrId, u32> defs; std::unordered_map<StrId, Atom> memo; std::unordered_set<StrId> visiting; std::vector<StrId> order; };
		struct Pending { StrId name; u32 off, len; };

		static void define(Env& e, const xss::TokenDef& d) {
			if (!e.defs.count(d.name)) e.order.push_back(d.name);
			e.defs[d.name] = d.expr;
		}
		void err(u32 off, u32 len, std::string m, std::string hint = {}) { m_d.error(off, len, std::move(m), std::move(hint)); }
		const xss::Expr& ex(u32 i) const { return m_a.exprs[i]; }

		std::optional<Atom> evalToken(Env& env, StrId name) {
			if (auto it = env.memo.find(name); it != env.memo.end()) return it->second;
			const xss::Expr& e = ex(env.defs.at(name));
			if (!env.visiting.insert(name).second) {
				err(e.off, e.len, "circular token reference '" + std::string(m_in.view(name)) + "'");
				return std::nullopt;
			}
			auto a = evalExpr(env.defs.at(name), env);
			env.visiting.erase(name);
			if (a) env.memo[name] = *a;
			return a;
		}

		bool amount(u32 argIdx, Env& env, f32& out) {
			auto a = evalExpr(m_a.args[argIdx], env);
			if (!a) return false;
			const xss::Expr& e = ex(m_a.args[argIdx]);
			if (a->kind != AtomKind::Number || (a->unit != Unit::None && a->unit != Unit::Percent)) {
				err(e.off, e.len, std::string("expected a percentage or a number, got ") + kindName(a->kind), "e.g. 10% or 0.1");
				return false;
			}
			out = a->unit == Unit::Percent ? a->num / 100.f : a->num;
			return true;
		}

		std::optional<Atom> evalExpr(u32 ei, Env& env) {
			const xss::Expr& e = ex(ei);
			using K = xss::ExprKind;
			switch (e.kind) {
			case K::Number: return Atom::number(e.num, e.unit);
			case K::Color: return Atom::color(e.color);
			case K::String: return Atom::string(e.name);
			case K::Path: {
				if (env.defs.count(e.name)) return evalToken(env, e.name);
				std::string_view n = m_in.view(e.name);
				if (n.find('.') == std::string_view::npos) return Atom::ident(e.name);   // bare enum word: Primary, solid, center
				std::vector<std::string_view> names; for (StrId s : env.order) names.push_back(m_in.view(s));
				std::string s = didYouMean(n, names);
				err(e.off, e.len, "unknown token '" + std::string(n) + "'", s.empty() ? "" : "did you mean '" + s + "'?");
				return std::nullopt;
			}
			case K::Call: {
				std::string_view n = m_in.view(e.name);
				f32 v[4] = { 0, 0, 0, 0 };
				if (n != "cubic-bezier" && n != "spring") {
					err(e.off, e.len, "unknown function '" + std::string(n) + "'", "values may use cubic-bezier() and spring(); transform functions only appear in `transform:`");
					return std::nullopt;
				}
				if (e.argCount > 4) { err(e.off, e.len, "too many arguments"); return std::nullopt; }
				for (u32 i = 0; i < e.argCount; ++i) {
					auto a = evalExpr(m_a.args[e.argFirst + i], env);
					if (!a) return std::nullopt;
					if (a->kind != AtomKind::Number) { err(e.off, e.len, "arguments must be numbers"); return std::nullopt; }
					v[i] = a->num;
				}
				Easing en;
				if (n == "cubic-bezier") {
					if (e.argCount != 4) { err(e.off, e.len, "cubic-bezier() takes 4 numbers"); return std::nullopt; }
					en.kind = EaseKind::CubicBezier; for (int i = 0; i < 4; ++i) en.p[i] = v[i];
				}
				else {
					en.kind = EaseKind::Spring; en.p[0] = e.argCount > 0 ? v[0] : 170.f; en.p[1] = e.argCount > 1 ? v[1] : 26.f; en.p[2] = e.argCount > 2 ? v[2] : 1.f;
				}
				m_sh.m_curves.push_back(en);
				Atom a; a.kind = AtomKind::Curve; a.bits = static_cast<u32>(m_sh.m_curves.size() - 1);
				return a;
			}
			case K::Method: {
				auto r = evalExpr(e.a, env);
				if (!r) return std::nullopt;
				std::string_view n = m_in.view(e.name);
				if (r->kind != AtomKind::Color) {
					err(e.off, e.len, "." + std::string(n) + "() needs a colour, but the receiver is " + kindName(r->kind));
					return std::nullopt;
				}
				Color c = Color::fromRGBA8(r->bits); f32 t = 0;
				auto need = [&](u32 lo, u32 hi) {
					if (e.argCount < lo || e.argCount > hi) { err(e.off, e.len, "." + std::string(n) + "() takes " + std::to_string(lo) + (lo == hi ? "" : "-" + std::to_string(hi)) + " argument(s)"); return false; }
					return true;
					};
				if (n == "mix") {
					if (!need(1, 2)) return std::nullopt;
					auto o = evalExpr(m_a.args[e.argFirst], env);
					if (!o) return std::nullopt;
					if (o->kind != AtomKind::Color) { err(e.off, e.len, ".mix() first argument must be a colour"); return std::nullopt; }
					t = 0.5f;
					if (e.argCount == 2 && !amount(e.argFirst + 1, env, t)) return std::nullopt;
					return Atom::color(c.mix(Color::fromRGBA8(o->bits), t).toRGBA8());
				}
				if (n != "darken" && n != "lighten" && n != "alpha" && n != "saturate" && n != "desaturate") {
					static constexpr std::string_view methods[] = { "darken", "lighten", "alpha", "saturate", "desaturate", "mix" };
					std::string s = didYouMean(n, methods);
					err(e.off, e.len, "unknown colour method '." + std::string(n) + "()'", s.empty() ? "" : "did you mean '" + s + "'?");
					return std::nullopt;
				}
				if (!need(1, 1) || !amount(e.argFirst, env, t)) return std::nullopt;
				Color o = n == "darken" ? c.darken(t) : n == "lighten" ? c.lighten(t) : n == "alpha" ? c.withAlpha(t) : n == "saturate" ? c.saturate(t) : c.desaturate(t);
				return Atom::color(o.toRGBA8());
			}
			case K::Neg: {
				auto a = evalExpr(e.a, env);
				if (!a) return std::nullopt;
				if (a->kind != AtomKind::Number) { err(e.off, e.len, std::string("cannot negate ") + kindName(a->kind)); return std::nullopt; }
				a->num = -a->num; return a;
			}
			case K::Binary: {
				auto l = evalExpr(e.a, env); auto r = evalExpr(e.b, env);
				if (!l || !r) return std::nullopt;
				if (l->kind != AtomKind::Number || r->kind != AtomKind::Number) {
					err(e.off, e.len, std::string("arithmetic needs numbers, got ") + kindName(l->kind) + " and " + kindName(r->kind)); return std::nullopt;
				}
				if (e.op == '+' || e.op == '-') {
					if (l->unit != r->unit) { err(e.off, e.len, "cannot combine values with different units", "convert both sides to the same unit"); return std::nullopt; }
					return Atom::number(e.op == '+' ? l->num + r->num : l->num - r->num, l->unit);
				}
				if (e.op == '*') {
					if (l->unit != Unit::None && r->unit != Unit::None) { err(e.off, e.len, "cannot multiply two dimensions"); return std::nullopt; }
					return Atom::number(l->num * r->num, l->unit != Unit::None ? l->unit : r->unit);
				}
				if (r->num == 0) { err(e.off, e.len, "division by zero"); return std::nullopt; }
				if (r->unit == Unit::None) return Atom::number(l->num / r->num, l->unit);
				if (r->unit == l->unit) return Atom::number(l->num / r->num, Unit::None);
				err(e.off, e.len, "cannot divide values with different units"); return std::nullopt;
			}
			}
			return std::nullopt;
		}

		// True if the expression (transitively) reads a token that some @theme overrides.
		bool themed(u32 ei) {
			if (m_themed.empty()) return false;
			signed char& memo = m_themedMemo[ei];
			if (memo >= 0) return memo == 1;
			memo = 0;   // cycle guard
			const xss::Expr& e = ex(ei);
			bool r = false;
			if (e.kind == xss::ExprKind::Path) {
				if (m_themed.count(e.name)) r = true;
				else if (auto it = m_env[0].defs.find(e.name); it != m_env[0].defs.end()) r = themed(it->second);
			}
			if (e.a != xss::kNone) r = r || themed(e.a);
			if (e.b != xss::kNone) r = r || themed(e.b);
			for (u32 i = 0; i < e.argCount; ++i) r = r || themed(m_a.args[e.argFirst + i]);
			m_themedMemo[ei] = r ? 1 : 0;
			return r;
		}

		bool compileDecl(const xss::DeclAst& d, std::vector<Declaration>& out) {
			if (d.prop == PropId::Gradient) {
				if (d.values.size() != 1 || ex(d.values[0]).kind != xss::ExprKind::Call) {
					err(d.off, d.len, "gradient expects linear-gradient(), radial-gradient(), or conic-gradient()");
					return false;
				}
				const xss::Expr& call = ex(d.values[0]);
				const std::string_view fn = m_in.view(call.name);
				const bool linear = fn == "linear-gradient", radial = fn == "radial-gradient", conic = fn == "conic-gradient";
				if (!linear && !radial && !conic) {
					err(call.off, call.len, "unknown gradient function '" + std::string(fn) + "'");
					return false;
				}
				const bool hasAngle = linear || conic;
				const u32 firstColor = call.argFirst + (hasAngle ? 1u : 0u);
				const u32 stopCount = call.argCount >= (hasAngle ? 3u : 2u) ? call.argCount - (hasAngle ? 1u : 0u) : 0u;
				if (stopCount < 2 || stopCount > 4) {
					err(call.off, call.len, "gradients need 2 to 4 color stops (and an angle for linear/conic gradients)");
					return false;
				}
				Declaration decl; decl.prop = d.prop;
				Atom kind = Atom::ident(call.name); decl.value.atoms[decl.value.count++] = kind;
				f32 angle = 0.f;
				if (hasAngle) {
					const xss::Expr& angleExpr = ex(m_a.args[call.argFirst]);
					if (angleExpr.kind != xss::ExprKind::Number || (angleExpr.unit != Unit::Deg && angleExpr.unit != Unit::None)) {
						err(angleExpr.off, angleExpr.len, "gradient angle must be a number in degrees");
						return false;
					}
					angle = angleExpr.num * 0.01745329251994329577f;
				}
				decl.value.atoms[decl.value.count++] = Atom::number(angle);
				for (u32 i = 0; i < stopCount; ++i) {
					const u32 exprIndex = m_a.args[firstColor + i];
					auto value = evalExpr(exprIndex, m_env[0]);
					if (!value || value->kind != AtomKind::Color) {
						err(ex(exprIndex).off, ex(exprIndex).len, "gradient stops must be colors or color tokens");
						return false;
					}
					if (themed(exprIndex)) {
						m_slots.push_back(exprIndex);
						Atom slot; slot.kind = AtomKind::Slot; slot.bits = static_cast<u32>(m_slots.size() - 1);
						decl.value.atoms[decl.value.count++] = slot;
					}
					else decl.value.atoms[decl.value.count++] = *value;
				}
				out.push_back(decl);
				return true;
			}
			const PropInfo& pi = propInfo(d.prop);
			Env& env = m_env[0];
			std::vector<Atom> shown; std::vector<AtomKind> kinds;
			for (u32 vi : d.values) {
				const xss::Expr& e = ex(vi);
				if (d.prop == PropId::Animation && e.kind == xss::ExprKind::Call && m_in.view(e.name) == "cancel") {
					if (e.argCount != 1) { err(e.off, e.len, "animation cancel() takes one argument: stop or finish"); return false; }
					const xss::Expr& choice = ex(m_a.args[e.argFirst]);
					const std::string_view policy = choice.kind == xss::ExprKind::Path ? m_in.view(choice.name) : std::string_view{};
					if (policy != "stop" && policy != "finish") { err(choice.off, choice.len, "animation cancel() expects stop or finish"); return false; }
					Atom modifier = Atom::ident(m_in.intern(std::string("cancel-") + std::string(policy)));
					kinds.push_back(modifier.kind); shown.push_back(modifier);
					continue;
				}
				if (d.prop == PropId::Transform && e.kind == xss::ExprKind::Call) {   // translate(10px, 4px) -> [Func, 10px, 4px]
					Atom f; f.kind = AtomKind::Func; f.bits = e.name.v;
					shown.push_back(f); kinds.push_back(AtomKind::Func);
					for (u32 i = 0; i < e.argCount; ++i) {
						auto a = evalExpr(m_a.args[e.argFirst + i], env);
						if (!a) return false;
						shown.push_back(*a); kinds.push_back(a->kind);
					}
					continue;
				}
				auto a = evalExpr(vi, env);
				if (!a) return false;
				kinds.push_back(a->kind);
				if (themed(vi)) { m_slots.push_back(vi); Atom s; s.kind = AtomKind::Slot; s.bits = static_cast<u32>(m_slots.size() - 1); shown.push_back(s); }
				else shown.push_back(*a);
			}
			if (kinds.size() < pi.minN || kinds.size() > pi.maxN) {
				err(d.off, d.len, "'" + std::string(pi.name) + "' takes " + (pi.minN == pi.maxN ? std::to_string(pi.minN) : std::to_string(pi.minN) + " to " + std::to_string(pi.maxN)) + " value(s), got " + std::to_string(kinds.size()));
				return false;
			}
			if (d.prop == PropId::AnimationCancel && !shown.empty() && shown[0].kind == AtomKind::Ident) {
				const std::string_view policy = m_in.view(shown[0].str());
				if (policy != "stop" && policy != "finish") {
					err(d.off, d.len, "animation-cancel expects stop or finish");
					return false;
				}
			}
			if (d.prop == PropId::Outline) {
				if (kinds.empty() || kinds[0] != AtomKind::Number ||
					(shown[0].unit != Unit::None && shown[0].unit != Unit::Px && shown[0].unit != Unit::Dp)) {
					err(d.off, d.len, "outline starts with a width in px or dp");
					return false;
				}
				const std::size_t colorIndex = shown.size() == 2 ? 1u : 2u;
				if (kinds[colorIndex] != AtomKind::Color) {
					err(d.off, d.len, "outline needs a color, optionally preceded by a style");
					return false;
				}
				if (shown.size() == 3) {
					if (kinds[1] != AtomKind::Ident) {
						err(d.off, d.len, "outline style must be solid or dashed");
						return false;
					}
					const std::string_view style = m_in.view(shown[1].str());
					if (style != "solid" && style != "dashed") {
						err(d.off, d.len, "outline style must be solid or dashed");
						return false;
					}
				}
			}
			if (d.prop == PropId::OutlineOffset && (kinds.empty() || kinds[0] != AtomKind::Number ||
				(shown[0].unit != Unit::None && shown[0].unit != Unit::Px && shown[0].unit != Unit::Dp) || shown[0].num < 0.f)) {
				err(d.off, d.len, "outline-offset must be a non-negative px or dp length");
				return false;
			}
			for (std::size_t i = 0; i < kinds.size(); ++i)
				if (!(pi.kindMask & (1u << static_cast<int>(kinds[i])))) {
					err(ex(d.values[std::min(i, d.values.size() - 1)]).off, ex(d.values[std::min(i, d.values.size() - 1)]).len,
						"'" + std::string(pi.name) + "' does not accept " + kindName(kinds[i]));
					return false;
				}
			Declaration decl; decl.prop = d.prop;
			const std::size_t n = shown.size();
			auto put = [&](std::initializer_list<std::size_t> idx) { for (std::size_t i : idx) decl.value.atoms[decl.value.count++] = shown[i]; };
			if (pi.expand == Expand::Sides4) {
				if (n == 1) put({ 0, 0, 0, 0 }); else if (n == 2) put({ 0, 1, 0, 1 }); else if (n == 3) put({ 0, 1, 2, 1 }); else put({ 0, 1, 2, 3 });
			}
			else if (pi.expand == Expand::Pair) {
				if (n == 1) put({ 0, 0 }); else put({ 0, 1 });
			}
			else {
				for (std::size_t i = 0; i < n; ++i) decl.value.atoms[decl.value.count++] = shown[i];
			}
			if (d.prop == PropId::Animation && shown[0].kind == AtomKind::Ident) m_pending.push_back({ shown[0].str(), d.off, d.len });
			out.push_back(decl);
			return true;
		}

		void compileRule(const xss::RuleAst& r) {
			std::vector<Declaration> per[kStateCount];
			for (const auto& b : r.blocks) for (const auto& d : b.decls) compileDecl(d, per[static_cast<int>(b.state)]);
			for (const Selector& sel : r.sels) {
				StyleSheet::Rule rule; rule.sel = sel; rule.order = static_cast<u32>(m_sh.m_rules.size()); rule.score = selectorScore(sel);
				for (int st = 0; st < kStateCount; ++st) {
					rule.stateFirst[st] = static_cast<u32>(m_sh.m_decls.size());
					rule.stateCount[st] = static_cast<u32>(per[st].size());
					m_sh.m_decls.insert(m_sh.m_decls.end(), per[st].begin(), per[st].end());
				}
				rule.transFirst = static_cast<u32>(m_sh.m_trans.size());
				for (const auto& t : r.trans) {
					Transition tr; tr.from = t.from; tr.to = t.to; tr.allProps = t.all; tr.prop = t.prop; tr.durationMs = t.dur; tr.delayMs = t.delay; tr.ease = t.ease;
					m_sh.m_trans.push_back(tr);
				}
				rule.transCount = static_cast<u32>(r.trans.size());
				m_sh.m_byType[sel.type].push_back(static_cast<u32>(m_sh.m_rules.size()));
				m_sh.m_rules.push_back(std::move(rule));
			}
		}

		void compileKeyframes(const xss::KeyframesAst& k) {
			if (m_sh.findKeyframes(k.name)) { err(k.off, k.len, "duplicate keyframes '" + std::string(m_in.view(k.name)) + "'"); return; }
			KeyframesDef def; def.name = k.name; def.frameFirst = static_cast<u32>(m_sh.m_frames.size());
			std::vector<const xss::FrameAst*> fs; for (const auto& f : k.frames) fs.push_back(&f);
			std::stable_sort(fs.begin(), fs.end(), [](auto* a, auto* b) { return a->t < b->t; });
			for (const auto* f : fs) {
				std::vector<Declaration> ds;
				for (const auto& d : f->decls) compileDecl(d, ds);
				Keyframe kf; kf.t = f->t; kf.declFirst = static_cast<u32>(m_sh.m_decls.size()); kf.declCount = static_cast<u32>(ds.size());
				m_sh.m_decls.insert(m_sh.m_decls.end(), ds.begin(), ds.end());
				m_sh.m_frames.push_back(kf);
			}
			def.frameCount = static_cast<u32>(m_sh.m_frames.size()) - def.frameFirst;
			m_sh.m_kf.push_back(def);
		}

		xss::Ast& m_a; StringInterner& m_in; DiagnosticList& m_d; StyleSheet& m_sh;
		std::vector<Env> m_env;
		std::unordered_set<StrId> m_themed;
		std::vector<signed char> m_themedMemo;
		std::vector<u32> m_slots;
		std::vector<Pending> m_pending;
	};

	Result<StyleSheet, DiagnosticList> compileXss(std::string_view source, std::string_view fileName, StringInterner& in) {
		DiagnosticList diags{ std::string(fileName), std::string(source) };
		xss::Ast ast;
		{ DiagnosticList base{ "<xss-base>", std::string(kBaseTokens) }; xss::parse(kBaseTokens, in, base, ast); }
		{ DiagnosticList base{ "<fluent2-defaults>", std::string(kFluent2Defaults) }; xss::parse(kFluent2Defaults, in, base, ast); }
		xss::parse(source, in, diags, ast);
		if (diags.hasErrors()) return fail(std::move(diags));   // don't cascade compile errors from a broken parse
		StyleSheet sheet;
		XssCompiler(ast, in, diags, sheet).run();
		if (diags.hasErrors()) return fail(std::move(diags));
		return sheet;
	}
}
