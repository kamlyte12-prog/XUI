#pragma once
// Internal: .xss syntax tree. Expressions live in one flat vector and refer to each other by index.
#include <xui/core/diagnostic.hpp>
#include <xui/style/style_types.hpp>
#include <vector>
namespace xui::xss {
	constexpr u32 kNone = 0xFFFFFFFFu;
	enum class ExprKind : u8 { Number, Color, String, Path, Call, Method, Binary, Neg };
	struct Expr {
		ExprKind kind{};
		char op = 0;                       // Binary: + - * /
		u32 off = 0, len = 0;              // source span (for diagnostics)
		f32 num = 0; Unit unit = Unit::None; u32 color = 0;
		StrId name;                        // String value / dotted Path / Call+Method name
		u32 a = kNone, b = kNone;          // Binary lhs/rhs, Neg operand, Method receiver (a)
		u32 argFirst = 0, argCount = 0;    // into Ast::args
	};
	struct DeclAst { PropId prop = PropId::Bg; u32 off = 0, len = 0; std::vector<u32> values; };
	struct TransAst { State from = State::Resting, to = State::Hover; bool all = true; PropId prop = PropId::Bg; f32 dur = 0, delay = 0; Easing ease; };
	struct StateBlockAst { State state = State::Resting; std::vector<DeclAst> decls; };
	struct RuleAst { std::vector<Selector> sels; std::vector<StateBlockAst> blocks; std::vector<TransAst> trans; };
	struct TokenDef { StrId name; u32 expr = kNone; u32 off = 0, len = 0; };
	struct ThemeAst { StrId name; std::vector<TokenDef> tokens; };
	struct FrameAst { f32 t = 0; std::vector<DeclAst> decls; };
	struct KeyframesAst { StrId name; u32 off = 0, len = 0; std::vector<FrameAst> frames; };
	struct Ast {
		std::vector<Expr> exprs; std::vector<u32> args;
		std::vector<ThemeAst> themes;      // [0] = base tokens
		std::vector<RuleAst> rules; std::vector<KeyframesAst> keyframes;
	};
	/// Parse and append to @p ast. Recovers after errors so several are reported per run.
	bool parse(std::string_view src, StringInterner& in, DiagnosticList& diags, Ast& ast);
}
