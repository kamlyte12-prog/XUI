#pragma once
/// @file xui.hpp  The .xui structure language: AST, parser, import loader, schema validation.
///
///     Type(constructorArgs...) [properties] { children }
///
/// The AST is a set of flat arrays addressed by index (no per-node allocation, cheap to diff for hot reload).
/// Expressions, nodes, args, props and children each live in one vector on the Document.
#include <xui/core/diagnostic.hpp>
#include <xui/core/result.hpp>
#include <xui/style/style_types.hpp>   // Unit
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace xui {

	constexpr u32 kNoExpr = 0xFFFFFFFFu;

	enum class XExprKind : u8 { Number, Size, Bool, String, Interp, Color, Name, Member, Index, Call, Unary, Binary, Ternary, Range };
	enum class XOp : u8 { None, Neg, Not, Add, Sub, Mul, Div, Mod, Eq, Ne, Lt, Le, Gt, Ge, And, Or };

	/// One expression node. Which fields are meaningful depends on `kind`:
	///  Number: num+unit | Size: num x num2 | Bool: bval | String: name (unescaped text) | Color: color (0xRRGGBBAA)
	///  Interp: parts[argFirst..+argCount] | Name: name | Member: a.name | Index: a[b]
	///  Call: callee a (Name/Member), callArgs[argFirst..+argCount] | Unary: op,a | Binary: op,a,b | Ternary: a?b:c | Range: a..b
	struct XExpr {
		XExprKind kind = XExprKind::Number;
		XOp op = XOp::None;
		bool bval = false;
		Unit unit = Unit::None;
		u32 off = 0, len = 0;
		f32 num = 0, num2 = 0;
		u32 color = 0;
		StrId name;
		u32 a = kNoExpr, b = kNoExpr, c = kNoExpr;
		u32 argFirst = 0, argCount = 0;
	};
	/// One segment of an interpolated string: either literal text (expr == kNoExpr) or an expression.
	struct XInterpPart { StrId lit; u32 expr = kNoExpr; };

	/// Constructor argument. name.v == 0 means positional.
	struct XArg { StrId name; u32 expr = kNoExpr; u32 off = 0, len = 0; };

	enum class XPropRole : u8 { Value, Bind, Event, If, For };
	/// Entry of a `[...]` list. `bind:` is two-way, `press:`-style names are event handlers, `if:` / `for:` control structure.
	struct XProp {
		StrId name; XPropRole role = XPropRole::Value;
		u32 expr = kNoExpr;               ///< value / path / handler / condition / for-collection (Range for `a..b`)
		StrId forVar, forIndex;           ///< for: `item in items` / `item, i in items`
		u32 off = 0, len = 0;
	};

	struct XNode {
		StrId type; u32 off = 0, len = 0;                 ///< span of the type name
		u32 argFirst = 0, argCount = 0;                   ///< into Document::args
		u32 propFirst = 0, propCount = 0;                 ///< into Document::props
		u32 childFirst = 0, childCount = 0;               ///< into Document::children (node indices)
	};
	struct XParam { StrId name; u32 defaultExpr = kNoExpr; };
	struct XComponent { StrId name; u32 off = 0, len = 0; u32 paramFirst = 0, paramCount = 0; u32 root = kNoExpr; };
	struct XImport { StrId path; u32 off = 0, len = 0; };

	/// A parsed .xui file. Keeps its source so later phases (validation, binding) can report file:line:col errors.
	struct Document {
		std::string file, source;
		std::vector<XExpr> exprs;
		std::vector<u32> callArgs;
		std::vector<XInterpPart> parts;
		std::vector<XArg> args;
		std::vector<XProp> props;
		std::vector<XNode> nodes;
		std::vector<u32> children;
		std::vector<u32> roots;                 ///< top-level nodes, in source order
		std::vector<XParam> params;
		std::vector<XComponent> components;
		std::vector<XImport> imports;

		std::span<const XArg> argsOf(const XNode& n) const { return { args.data() + n.argFirst, n.argCount }; }
		std::span<const XProp> propsOf(const XNode& n) const { return { props.data() + n.propFirst, n.propCount }; }
		std::span<const u32> childrenOf(const XNode& n) const { return { children.data() + n.childFirst, n.childCount }; }
		std::span<const XParam> paramsOf(const XComponent& c) const { return { params.data() + c.paramFirst, c.paramCount }; }
		DiagnosticList diagnostics() const { return DiagnosticList(file, source); }
	};

	/// Parse one file. Recovers after errors (siblings start on a new line) so several are reported per run.
	Result<Document, DiagnosticList> parseXui(std::string_view source, std::string_view file, StringInterner& in);

	/// S-expression rendering of an expression, e.g. `(+ 1 (* 2 3))`. For tests and debugging.
	std::string dumpExpr(const Document& d, const StringInterner& in, u32 expr);
	/// True for the event property names: press release click doubleClick change submit focus blur hover scroll drag drop key resize close.
	bool isEventName(std::string_view name);

	// ---- imports -------------------------------------------------------------------------------------
	using ReadFileFn = std::optional<std::string>(*)(void* ctx, std::string_view path);

	/// All files reachable from an entry file. files[0] is the entry. Components are global across files.
	struct Program {
		struct CompRef { u32 file = 0, index = 0; };
		std::vector<Document> files;
		std::unordered_map<StrId, CompRef> components;
		const XComponent* findComponent(StrId name, const Document** owner = nullptr) const;
	};
	struct XuiError {
		std::vector<DiagnosticList> lists;      ///< one per file that had problems
		std::string format() const;
	};
	/// Load @p entryPath and everything it `@import`s (paths resolve relative to the importing file).
	/// Detects import cycles, missing files and duplicate component names.
	Result<Program, XuiError> loadXui(std::string_view entryPath, ReadFileFn read, void* ctx, StringInterner& in);

	// ---- schema validation ---------------------------------------------------------------------------
	/// Which widget types exist and which constructor params / properties they accept. Widgets register
	/// themselves here; withCoreWidgets() pre-registers the standard library's core subset.
	class XuiSchema {
	public:
		struct Widget { std::vector<std::string> ctor, props; };
		/// @param ctor  positional constructor parameters, in order (also accepted by name)
		/// @param props additional named properties
		void addWidget(std::string type, std::vector<std::string> ctor = {}, std::vector<std::string> props = {});
		const Widget* find(std::string_view type) const;
		static const std::vector<std::string_view>& genericProps();   ///< id class key grow shrink ... valid on every node
		static XuiSchema withCoreWidgets();
		std::vector<std::string_view> widgetNames() const;
	private:
		std::unordered_map<std::string, Widget> m_widgets;
	};
	/// Check types, argument counts and property names (with did-you-mean). Returns one list per file with problems.
	Result<void, XuiError> validate(const Program& prog, const XuiSchema& schema, const StringInterner& in);
}
