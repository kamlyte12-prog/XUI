#include <xui/core/color.hpp>
#include <xui/lang/xui.hpp>
#include <cstdlib>
#include <string>
#include <charconv>
namespace xui {
	namespace {

		enum class Tok : u8 {
			End, Ident, Number, Size, String, Color, LBrace, RBrace, LBracket, RBracket, LParen, RParen,
			Colon, Comma, Dot, DotDot, Question, EqEq, NotEq, Lt, Le, Gt, Ge, Plus, Minus, Star, Slash, Percent,
			AndAnd, OrOr, Not, At
		};
		struct Token { Tok k = Tok::End; u32 off = 0, len = 0; f32 num = 0, num2 = 0; Unit unit = Unit::None; };

		bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
		bool isDigit(char c) { return c >= '0' && c <= '9'; }
		bool isIdChar(char c) { return isAlpha(c) || isDigit(c); }
		bool unitFromName(std::string_view s, Unit& u) {
			struct E { std::string_view n; Unit u; };
			static constexpr E t[] = { {"px", Unit::Px}, {"em", Unit::Em}, {"rem", Unit::Rem}, {"vw", Unit::Vw}, {"vh", Unit::Vh},
									  {"dp", Unit::Dp}, {"sp", Unit::Sp}, {"ms", Unit::Ms}, {"s", Unit::S}, {"deg", Unit::Deg} };
			for (const E& e : t) if (e.n == s) { u = e.u; return true; }
			return false;
		}

		/// Lexes [begin,end) of the full source, so offsets are always absolute (needed for string interpolation).
		class Lexer {
		public:
			Lexer(std::string_view s, std::size_t begin, std::size_t end, DiagnosticList& d) : m_s(s), m_i(begin), m_end(end), m_d(d) {}
			void run(std::vector<Token>& out) {
				for (;;) {
					skipTrivia();
					if (m_i >= m_end) { out.push_back({ Tok::End, u32(m_end), 0 }); return; }
					char c = m_s[m_i]; u32 st = u32(m_i);
					auto one = [&](Tok k) { ++m_i; out.push_back({ k, st, 1 }); };
					auto two = [&](Tok k) { m_i += 2; out.push_back({ k, st, 2 }); };
					char n = m_i + 1 < m_end ? m_s[m_i + 1] : '\0';
					if (isAlpha(c)) { while (m_i < m_end && isIdChar(m_s[m_i])) ++m_i; out.push_back({ Tok::Ident, st, u32(m_i - st) }); }
					else if (isDigit(c)) number(out);
					else if (c == '"') string(out);
					else if (c == '#') { ++m_i; while (m_i < m_end && isIdChar(m_s[m_i])) ++m_i; out.push_back({ Tok::Color, st, u32(m_i - st) }); }
					else if (c == '.' && n == '.') two(Tok::DotDot);
					else if (c == '=' && n == '=') two(Tok::EqEq);
					else if (c == '!' && n == '=') two(Tok::NotEq);
					else if (c == '<' && n == '=') two(Tok::Le);
					else if (c == '>' && n == '=') two(Tok::Ge);
					else if (c == '&' && n == '&') two(Tok::AndAnd);
					else if (c == '|' && n == '|') two(Tok::OrOr);
					else switch (c) {
					case '{': one(Tok::LBrace); break;   case '}': one(Tok::RBrace); break;
					case '[': one(Tok::LBracket); break; case ']': one(Tok::RBracket); break;
					case '(': one(Tok::LParen); break;   case ')': one(Tok::RParen); break;
					case ':': one(Tok::Colon); break;    case ',': one(Tok::Comma); break;
					case '.': one(Tok::Dot); break;      case '?': one(Tok::Question); break;
					case '<': one(Tok::Lt); break;       case '>': one(Tok::Gt); break;
					case '+': one(Tok::Plus); break;     case '-': one(Tok::Minus); break;
					case '*': one(Tok::Star); break;     case '/': one(Tok::Slash); break;
					case '%': one(Tok::Percent); break;  case '!': one(Tok::Not); break;
					case '@': one(Tok::At); break;
					default: m_d.error(st, 1, std::string("unexpected character '") + c + "'"); ++m_i; break;
					}
				}
			}
		private:
			void skipTrivia() {
				while (m_i < m_end) {
					char c = m_s[m_i];
					if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++m_i;
					else if (c == '/' && m_i + 1 < m_end && m_s[m_i + 1] == '/') { while (m_i < m_end && m_s[m_i] != '\n') ++m_i; }
					else if (c == '/' && m_i + 1 < m_end && m_s[m_i + 1] == '*') {
						std::size_t st = m_i; m_i += 2;
						while (m_i + 1 < m_end && !(m_s[m_i] == '*' && m_s[m_i + 1] == '/')) ++m_i;
						if (m_i + 1 >= m_end) { m_d.error(u32(st), 2, "unterminated block comment", "close it with */"); m_i = m_end; }
						else m_i += 2;
					}
					else break;
				}
			}
			f32 digits() {
				std::size_t b = m_i;
				while (m_i < m_end && isDigit(m_s[m_i])) ++m_i;
				if (m_i + 1 < m_end && m_s[m_i] == '.' && isDigit(m_s[m_i + 1])) { ++m_i; while (m_i < m_end && isDigit(m_s[m_i])) ++m_i; }
				// was: f32 v = 0; std::from_chars(m_s.data() + b, m_s.data() + m_i, v); return v;
				const std::string tmp(m_s.data() + b, m_s.data() + m_i);
				f32 v = std::strtof(tmp.c_str(), nullptr); return v;
			}
			void number(std::vector<Token>& out) {
				u32 st = u32(m_i);
				f32 v = digits();
				if (m_i + 1 < m_end && m_s[m_i] == 'x' && isDigit(m_s[m_i + 1])) {   // 800x600
					++m_i; f32 h = digits();
					out.push_back({ Tok::Size, st, u32(m_i - st), v, h, Unit::None }); return;
				}
				Unit u = Unit::None;
				if (m_i < m_end && m_s[m_i] == '%') { u = Unit::Percent; ++m_i; }
				else if (m_i < m_end && isAlpha(m_s[m_i])) {
					std::size_t us = m_i;
					while (m_i < m_end && isAlpha(m_s[m_i])) ++m_i;
					if (!unitFromName(m_s.substr(us, m_i - us), u))
						m_d.error(u32(us), u32(m_i - us), "unknown unit '" + std::string(m_s.substr(us, m_i - us)) + "'", "valid units: px % em rem vw vh dp sp ms s deg");
				}
				out.push_back({ Tok::Number, st, u32(m_i - st), v, 0, u });
			}
			void string(std::vector<Token>& out) {
				u32 st = u32(m_i); ++m_i;
				while (m_i < m_end && m_s[m_i] != '"' && m_s[m_i] != '\n') { if (m_s[m_i] == '\\' && m_i + 1 < m_end) ++m_i; ++m_i; }
				if (m_i >= m_end || m_s[m_i] != '"') m_d.error(st, 1, "unterminated string", "strings end with a closing \" on the same line");
				else ++m_i;
				out.push_back({ Tok::String, st, u32(m_i - st) });
			}
			std::string_view m_s; std::size_t m_i, m_end; DiagnosticList& m_d;
		};

		class Parser {
		public:
			Parser(std::string_view s, StringInterner& in, DiagnosticList& d, Document& doc, std::size_t begin, std::size_t end)
				: m_s(s), m_in(in), m_d(d), m_doc(doc) {
				Lexer(s, begin, end, d).run(m_t);
			}

			void parseFile() {
				while (peek().k != Tok::End) {
					const Token& t = peek();
					if (t.k == Tok::At) parseImport();
					else if (t.k == Tok::Ident && text(t) == "component" && peek(1).k == Tok::Ident) parseComponent();
					else {
						u32 n = parseNode();
						if (n != kNoExpr) m_doc.roots.push_back(n);
						else recoverSibling();
					}
				}
			}
			u32 parseWhole() {   // interpolation body: exactly one expression
				u32 e = parseExpr();
				if (e != kNoExpr && peek().k != Tok::End) {
					err(peek(), "unexpected " + describe(peek()) + " in interpolation");
					return kNoExpr;
				}
				return e;
			}
		private:
			// ---- token helpers -----------------------------------------------------------------------
			const Token& peek(std::size_t n = 0) const { std::size_t i = m_p + n; return m_t[i < m_t.size() ? i : m_t.size() - 1]; }
			Token next() { Token t = peek(); if (m_p + 1 < m_t.size()) ++m_p; return t; }
			bool accept(Tok k) { if (peek().k == k) { next(); return true; } return false; }
			std::string_view text(const Token& t) const { return m_s.substr(t.off, t.len); }
			void err(const Token& t, std::string m, std::string hint = {}) { m_d.error(t.off, t.len, std::move(m), std::move(hint)); }
			void err(u32 off, u32 len, std::string m, std::string hint = {}) { m_d.error(off, len, std::move(m), std::move(hint)); }
			bool expect(Tok k, const std::string& what) {
				if (peek().k == k) { next(); return true; }
				err(peek(), "expected " + what + ", found " + describe(peek()));
				return false;
			}
			std::string describe(const Token& t) const { return t.k == Tok::End ? "end of input" : "'" + std::string(text(t)) + "'"; }
			bool onNewLine(std::size_t i) const {
				if (i == 0 || i >= m_t.size()) return true;
				u32 from = m_t[i - 1].off + m_t[i - 1].len;
				for (u32 j = from; j < m_t[i].off; ++j) if (m_s[j] == '\n') return true;
				return false;
			}
			// Skip to the next sibling: a '}' closing the parent (not consumed) or an identifier starting a new line.
			void syncSibling() {
				int depth = 0;
				for (;;) {
					const Token& t = peek();
					if (t.k == Tok::End) return;
					if (depth == 0 && (t.k == Tok::RBrace || (t.k == Tok::Ident && onNewLine(m_p) && !(peek(1).k == Tok::Colon)) || t.k == Tok::At)) return;
					if (t.k == Tok::LParen || t.k == Tok::LBracket || t.k == Tok::LBrace) ++depth;
					if (t.k == Tok::RParen || t.k == Tok::RBracket || t.k == Tok::RBrace) --depth;
					next();
				}
			}
			// syncSibling() + a progress guarantee. syncSibling stops *at* '}' (the end of an enclosing block); at top level there
			// is no enclosing block, so without this a stray '}' would be re-examined forever. Every recovery loop must advance.
			void recoverSibling() {
				std::size_t before = m_p;
				syncSibling();
				if (m_p == before && peek().k != Tok::End) next();
			}
			// Skip to the next ',' or the closing token at depth 0 (neither consumed).
			void syncList(Tok closer) {
				int depth = 0;
				for (;;) {
					Tok k = peek().k;
					if (k == Tok::End) return;
					if (depth == 0 && (k == Tok::Comma || k == closer)) return;
					if (k == Tok::LParen || k == Tok::LBracket || k == Tok::LBrace) ++depth;
					if (k == Tok::RParen || k == Tok::RBracket || k == Tok::RBrace) { if (depth == 0) return; --depth; }
					next();
				}
			}

			// ---- expressions -------------------------------------------------------------------------
			u32 push(const XExpr& e) { m_doc.exprs.push_back(e); return u32(m_doc.exprs.size() - 1); }
			u32 endOf(u32 e) const { return m_doc.exprs[e].off + m_doc.exprs[e].len; }
			u32 bin(XOp op, u32 l, u32 r) {
				XExpr e;
				e.kind = XExprKind::Binary; e.op = op; e.a = l; e.b = r;
				e.off = m_doc.exprs[l].off; e.len = endOf(r) - e.off; return push(e);
			}
			u32 parseExpr() {
				DepthGuard g(m_depth);
				if (!g.ok) { err(peek(), "expression is nested too deeply"); return kNoExpr; }
				return parseTernary();
			}
			u32 parseTernary() {
				DepthGuard g(m_depth);
				if (!g.ok) { err(peek(), "expression is nested too deeply"); return kNoExpr; }
				u32 c = parseBinary(0);
				if (c == kNoExpr || peek().k != Tok::Question) return c;
				next();
				u32 a = parseTernary();
				if (a == kNoExpr || !expect(Tok::Colon, "':' in conditional expression")) return kNoExpr;
				u32 b = parseTernary();
				if (b == kNoExpr) return kNoExpr;
				XExpr e; e.kind = XExprKind::Ternary; e.a = c; e.b = a; e.c = b; e.off = m_doc.exprs[c].off; e.len = endOf(b) - e.off;
				return push(e);
			}
			struct OpInfo { Tok tok; XOp op; int prec; };
			static const OpInfo* opFor(Tok k) {
				static constexpr OpInfo t[] = {
					{Tok::OrOr, XOp::Or, 0}, {Tok::AndAnd, XOp::And, 1},
					{Tok::EqEq, XOp::Eq, 2}, {Tok::NotEq, XOp::Ne, 2},
					{Tok::Lt, XOp::Lt, 3}, {Tok::Le, XOp::Le, 3}, {Tok::Gt, XOp::Gt, 3}, {Tok::Ge, XOp::Ge, 3},
					{Tok::Plus, XOp::Add, 4}, {Tok::Minus, XOp::Sub, 4},
					{Tok::Star, XOp::Mul, 5}, {Tok::Slash, XOp::Div, 5}, {Tok::Percent, XOp::Mod, 5} };
				for (const OpInfo& o : t) if (o.tok == k) return &o;
				return nullptr;
			}
			u32 parseBinary(int minPrec) {   // precedence climbing; all binary operators are left-associative
				u32 l = parseUnary();
				while (l != kNoExpr) {
					const OpInfo* o = opFor(peek().k);
					if (!o || o->prec < minPrec) break;
					next();
					u32 r = parseBinary(o->prec + 1);
					if (r == kNoExpr) return kNoExpr;
					l = bin(o->op, l, r);
				}
				return l;
			}
			u32 parseUnary() {
				DepthGuard g(m_depth);
				if (!g.ok) { err(peek(), "expression is nested too deeply"); return kNoExpr; }
				if (peek().k == Tok::Minus || peek().k == Tok::Not) {
					Token t = next(); u32 o = parseUnary();
					if (o == kNoExpr) return kNoExpr;
					XExpr e; e.kind = XExprKind::Unary;
					e.op = t.k == Tok::Minus ? XOp::Neg : XOp::Not; e.a = o; e.off = t.off; e.len = endOf(o) - t.off;
					return push(e);
				}
				return parsePostfix();
			}
			u32 parsePostfix() {
				u32 e = parsePrimary();
				while (e != kNoExpr) {
					if (peek().k == Tok::Dot && peek(1).k == Tok::Ident) {
						next(); Token nm = next();
						XExpr m; m.kind = XExprKind::Member; m.a = e;
						m.name = m_in.intern(text(nm)); m.off = m_doc.exprs[e].off; m.len = nm.off + nm.len - m.off;
						e = push(m);
					}
					else if (peek().k == Tok::LParen) {
						XExprKind ck = m_doc.exprs[e].kind;
						if (ck != XExprKind::Name && ck != XExprKind::Member) {
							err(peek(), "only functions can be called", "call a name like `format(x)` or a member like `list.count()`");
							return kNoExpr;
						}
						next();
						std::vector<u32> args;
						if (!accept(Tok::RParen)) {
							for (;;) {
								u32 a = parseExpr();
								if (a == kNoExpr) return kNoExpr;
								args.push_back(a);
								if (accept(Tok::Comma)) continue;
								if (!expect(Tok::RParen, "',' or ')'")) return kNoExpr;
								break;
							}
						}
						XExpr c; c.kind = XExprKind::Call; c.a = e; c.off = m_doc.exprs[e].off; c.len = m_t[m_p - 1].off + 1 - c.off;
						c.argFirst = u32(m_doc.callArgs.size()); c.argCount = u32(args.size());
						m_doc.callArgs.insert(m_doc.callArgs.end(), args.begin(), args.end());
						e = push(c);
					}
					else if (peek().k == Tok::LBracket) {
						next(); u32 i = parseExpr();
						if (i == kNoExpr) return kNoExpr;
						Token close = peek();
						if (!expect(Tok::RBracket, "']'")) return kNoExpr;
						XExpr x; x.kind = XExprKind::Index; x.a = e; x.b = i; x.off = m_doc.exprs[e].off; x.len = close.off + 1 - x.off;
						e = push(x);
					}
					else break;
				}
				return e;
			}
			u32 parsePrimary() {
				Token t = peek();
				XExpr e; e.off = t.off; e.len = t.len;
				switch (t.k) {
				case Tok::Number: next(); e.kind = XExprKind::Number; e.num = t.num; e.unit = t.unit; return push(e);
				case Tok::Size: next(); e.kind = XExprKind::Size; e.num = t.num; e.num2 = t.num2; return push(e);
				case Tok::Color: {
					next(); Color c;
					if (!Color::parseHex(text(t), c)) {
						err(t, "invalid colour '" + std::string(text(t)) + "'", "use #rgb, #rgba, #rrggbb or #rrggbbaa");
						return kNoExpr;
					}
					e.kind = XExprKind::Color; e.color = c.toRGBA8(); return push(e);
				}
				case Tok::String: next(); return parseString(t);
				case Tok::Ident: {
					next(); std::string_view s = text(t);
					if (s == "true" || s == "false") { e.kind = XExprKind::Bool; e.bval = s == "true"; return push(e); }
					e.kind = XExprKind::Name; e.name = m_in.intern(s); return push(e);
				}
				case Tok::LParen: {
					next(); u32 inner = parseExpr();
					if (inner == kNoExpr || !expect(Tok::RParen, "')'")) return kNoExpr;
					return inner;
				}
				default: err(t, "expected an expression, found " + describe(t)); return kNoExpr;
				}
			}
			// A string literal, possibly with "{expr}" interpolations. `\{` `\}` `\"` `\\` `\n` `\t` are escapes.
			u32 parseString(const Token& t) {
				bool terminated = t.len >= 2 && m_s[t.off + t.len - 1] == '"';
				std::size_t begin = t.off + 1, end = t.off + t.len - (terminated ? 1 : 0);
				std::vector<XInterpPart> parts; std::string cur;
				auto flush = [&] { if (!cur.empty()) { parts.push_back({ m_in.intern(cur), kNoExpr }); cur.clear(); } };
				for (std::size_t i = begin; i < end; ++i) {
					char c = m_s[i];
					if (c == '\\' && i + 1 < end) {
						char n = m_s[++i];
						cur += n == 'n' ? '\n' : n == 't' ? '\t' : n;
					}
					else if (c == '{') {
						int depth = 1; std::size_t j = i + 1;
						for (; j < end; ++j) { if (m_s[j] == '{') ++depth; else if (m_s[j] == '}' && --depth == 0) break; }
						if (j >= end) {
							err(u32(i), 1, "unterminated interpolation", "close it with }, or write \\{ for a literal brace");
							return kNoExpr;
						}
						bool blank = true; for (std::size_t k = i + 1; k < j; ++k) if (m_s[k] != ' ' && m_s[k] != '\t') blank = false;
						if (blank) {
							err(u32(i), u32(j - i + 1), "empty interpolation", "put an expression inside the braces, or write \\{ \\} for literal braces");
							return kNoExpr;
						}
						Parser sub(m_s, m_in, m_d, m_doc, i + 1, j);
						u32 ex = sub.parseWhole();
						if (ex == kNoExpr) return kNoExpr;
						flush(); parts.push_back({ StrId{}, ex });
						i = j;
					}
					else cur += c;
				}
				XExpr e; e.off = t.off; e.len = t.len;
				// No interpolation seen => nothing was flushed, so `cur` is the entire (unescaped) literal.
				if (parts.empty()) { e.kind = XExprKind::String; e.name = m_in.intern(cur); return push(e); }
				flush();
				e.kind = XExprKind::Interp; e.argFirst = u32(m_doc.parts.size()); e.argCount = u32(parts.size());
				m_doc.parts.insert(m_doc.parts.end(), parts.begin(), parts.end());
				return push(e);
			}

			// ---- structure ---------------------------------------------------------------------------
			void parseImport() {
				Token at = next(); Token kw = next();
				if (kw.k != Tok::Ident || text(kw) != "import") {
					err(kw, "unknown directive " + describe(kw), "the only directive is @import \"file.xui\"");
					syncSibling();
					return;
				}
				Token s = peek();
				if (s.k != Tok::String || s.len < 2) {
					err(s, "expected a file path string after @import, found " + describe(s));
					syncSibling();
					return;
				}
				next();
				m_doc.imports.push_back({ m_in.intern(m_s.substr(s.off + 1, s.len - 2)), at.off, s.off + s.len - at.off });
			}
			void parseComponent() {
				Token kw = next(); Token nm = next();
				XComponent c; c.name = m_in.intern(text(nm)); c.off = kw.off; c.len = nm.off + nm.len - kw.off;
				for (const XComponent& o : m_doc.components) if (o.name == c.name) err(nm, "component '" + std::string(text(nm)) + "' is already defined in this file");
				c.paramFirst = u32(m_doc.params.size());
				if (accept(Tok::LParen)) {
					if (!accept(Tok::RParen)) {
						for (;;) {
							Token p = peek();
							if (p.k != Tok::Ident) {
								err(p, "expected a parameter name, found " + describe(p));
								syncList(Tok::RParen);
							}
							else {
								next(); XParam xp; xp.name = m_in.intern(text(p));
								if (accept(Tok::Colon)) {
									xp.defaultExpr = parseExpr();
									if (xp.defaultExpr == kNoExpr) syncList(Tok::RParen);
								}
								m_doc.params.push_back(xp);
							}
							if (accept(Tok::Comma)) continue;
							if (!expect(Tok::RParen, "',' or ')'")) syncList(Tok::RParen), accept(Tok::RParen);
							break;
						}
					}
				}
				c.paramCount = u32(m_doc.params.size()) - c.paramFirst;
				if (!expect(Tok::LBrace, "'{' to start the component body")) { syncSibling(); return; }
				bool prev = m_inComponent; m_inComponent = true;
				c.root = parseNode();
				if (c.root == kNoExpr) syncSibling();
				m_inComponent = prev;
				if (peek().k != Tok::RBrace && peek().k != Tok::End) {
					err(peek(), "a component has exactly one root node", "wrap the nodes in a Row, Column or Stack");
					int depth = 0;
					for (;;) {
						if (peek().k == Tok::End) break;
						if (peek().k == Tok::LBrace) ++depth;
						if (peek().k == Tok::RBrace) {
							if (depth == 0) break;
							--depth;
						}
						next();
					}
				}
				expect(Tok::RBrace, "'}'");
				m_doc.components.push_back(c);
			}

			u32 parseNode() {
				DepthGuard g(m_depth);
				if (!g.ok) { err(peek(), "widgets are nested too deeply"); return kNoExpr; }
				Token t = peek();
				if (t.k != Tok::Ident) { err(t, "expected a widget name, found " + describe(t)); return kNoExpr; }
				next();
				XNode n; n.type = m_in.intern(text(t)); n.off = t.off; n.len = t.len;
				if (text(t) == "Slot" && !m_inComponent) err(t, "Slot() is only valid inside a component", "declare it as `component Name { ... Slot() ... }`");
				n.argFirst = u32(m_doc.args.size());
				if (accept(Tok::LParen)) parseArgs();
				n.argCount = u32(m_doc.args.size()) - n.argFirst;
				n.propFirst = u32(m_doc.props.size());
				if (peek().k == Tok::LBracket) { next(); parseProps(); }
				n.propCount = u32(m_doc.props.size()) - n.propFirst;
				std::vector<u32> kids;
				if (peek().k == Tok::LBrace) {
					next();
					while (peek().k != Tok::RBrace && peek().k != Tok::End) {
						u32 c = parseNode();
						if (c == kNoExpr) recoverSibling(); else kids.push_back(c);
					}
					expect(Tok::RBrace, "'}' to close the children of " + std::string(text(t)));
				}
				n.childFirst = u32(m_doc.children.size()); n.childCount = u32(kids.size());
				m_doc.children.insert(m_doc.children.end(), kids.begin(), kids.end());
				m_doc.nodes.push_back(n);
				return u32(m_doc.nodes.size() - 1);
			}
			void parseArgs() {   // after '('; consumes ')'
				if (accept(Tok::RParen)) return;
				for (;;) {
					if (peek().k == Tok::RParen) { next(); return; }   // trailing comma
					XArg a; Token first = peek(); a.off = first.off;
					if (first.k == Tok::Ident && peek(1).k == Tok::Colon) { next(); next(); a.name = m_in.intern(text(first)); }
					a.expr = parseExpr();
					if (a.expr == kNoExpr) syncList(Tok::RParen);
					else { a.len = endOf(a.expr) - a.off; m_doc.args.push_back(a); }
					if (accept(Tok::Comma)) continue;
					if (!expect(Tok::RParen, "',' or ')' in argument list")) { syncList(Tok::RParen); accept(Tok::RParen); }
					return;
				}
			}
			void parseProps() {   // after '['; consumes ']'
				std::vector<StrId> seen;
				if (accept(Tok::RBracket)) return;
				for (;;) {
					if (peek().k == Tok::RBracket) { next(); return; }   // trailing comma
					parseProp(seen);
					if (accept(Tok::Comma)) continue;
					if (!expect(Tok::RBracket, "',' or ']' in property list")) { syncList(Tok::RBracket); accept(Tok::RBracket); }
					return;
				}
			}
			void parseProp(std::vector<StrId>& seen) {
				Token nm = peek();
				if (nm.k != Tok::Ident) { err(nm, "expected a property name, found " + describe(nm)); syncList(Tok::RBracket); return; }
				next();
				if (!expect(Tok::Colon, "':' after property name")) { syncList(Tok::RBracket); return; }
				XProp p; p.name = m_in.intern(text(nm)); p.off = nm.off;
				std::string_view name = text(nm);
				for (StrId s : seen) if (s == p.name) err(nm, "duplicate property '" + std::string(name) + "'");
				seen.push_back(p.name);
				if (name == "for") {
					p.role = XPropRole::For;
					Token v = peek();
					if (v.k != Tok::Ident) {
						err(v, "expected a loop variable, e.g. `for: item in items`");
						syncList(Tok::RBracket);
						return;
					}
					next(); p.forVar = m_in.intern(text(v));
					if (peek().k == Tok::Comma && peek(1).k == Tok::Ident && peek(2).k == Tok::Ident && text(peek(2)) == "in") {
						next();
						p.forIndex = m_in.intern(text(next()));
					}
					Token in = peek();
					if (in.k != Tok::Ident || text(in) != "in") {
						err(in, "expected 'in' after the loop variable, found " +
							describe(in), "e.g. `for: item in items` or `for: i in 0..10`");
						syncList(Tok::RBracket);
						return;
					}
					next();
					u32 lo = parseExpr();
					if (lo == kNoExpr) { syncList(Tok::RBracket); return; }
					if (accept(Tok::DotDot)) {
						u32 hi = parseExpr();
						if (hi == kNoExpr) { syncList(Tok::RBracket); return; }
						XExpr r; r.kind = XExprKind::Range;
						r.a = lo; r.b = hi; r.off = m_doc.exprs[lo].off; r.len = endOf(hi) - r.off; lo = push(r);
					}
					p.expr = lo;
				}
				else {
					u32 e = parseExpr();
					if (e == kNoExpr) { syncList(Tok::RBracket); return; }
					p.expr = e;
					const XExpr& ex = m_doc.exprs[e];
					if (name == "if") p.role = XPropRole::If;
					else if (name == "bind") {
						p.role = XPropRole::Bind;
						if (ex.kind != XExprKind::Name && ex.kind != XExprKind::Member && ex.kind != XExprKind::Index)
							err(ex.off, ex.len, "'bind' needs a state path to write to", "e.g. bind: userName or bind: form.email; use `value:` for one-way binding");
					}
					else if (isEventName(name)) {
						p.role = XPropRole::Event;
						if (ex.kind != XExprKind::Name && ex.kind != XExprKind::Member && ex.kind != XExprKind::Call)
							err(ex.off, ex.len, "an event handler must be a function name like `onSubmit` or a call like `remove(item)`");
					}
				}
				p.len = endOf(p.expr) - p.off;
				m_doc.props.push_back(p);
			}

			// Recursive descent must not overflow the stack on hostile input: every recursive entry point holds a DepthGuard.
			static constexpr int kMaxDepth = 512;   // guarded frames: ~170 nested parentheses (3 guards each) or 512 nested widgets
			int m_depth = 0;
			struct DepthGuard {
				int& d; bool ok;
				explicit DepthGuard(int& depth) : d(depth), ok(++depth <= kMaxDepth) {}
				~DepthGuard() { --d; }
			};
			std::string_view m_s; StringInterner& m_in; DiagnosticList& m_d; Document& m_doc;
			std::vector<Token> m_t; std::size_t m_p = 0; bool m_inComponent = false;
		};
	}

	bool isEventName(std::string_view n) {
		static constexpr std::string_view ev[] = { "press", "release", "click", "doubleClick", "change", "submit", "focus", "blur",
												  "hover", "scroll", "drag", "drop", "key", "resize", "close", "dock", "undock", "tabChange" };
		for (auto e : ev) if (e == n) return true;
		return false;
	}

	Result<Document, DiagnosticList> parseXui(std::string_view source, std::string_view file, StringInterner& in) {
		Document doc; doc.file = std::string(file); doc.source = std::string(source);
		DiagnosticList diags(doc.file, doc.source);
		Parser(doc.source, in, diags, doc, 0, doc.source.size()).parseFile();
		if (diags.hasErrors()) return fail(std::move(diags));
		return doc;
	}

	std::string dumpExpr(const Document& d, const StringInterner& in, u32 ei) {
		const XExpr& e = d.exprs[ei];
		auto num = [](f32 v) { char b[32]; std::snprintf(b, sizeof b, "%g", double(v)); return std::string(b); };
		auto unitStr = [](Unit u) -> const char* {
			switch (u) {
			case Unit::Px: return "px"; case Unit::Percent: return "%"; case Unit::Em: return "em"; case Unit::Rem: return "rem";
			case Unit::Vw: return "vw"; case Unit::Vh: return "vh"; case Unit::Dp: return "dp"; case Unit::Sp: return "sp";
			case Unit::Ms: return "ms"; case Unit::S: return "s"; case Unit::Deg: return "deg"; default: return "";
			}
			};
		switch (e.kind) {
		case XExprKind::Number: return num(e.num) + unitStr(e.unit);
		case XExprKind::Size: return num(e.num) + "x" + num(e.num2);
		case XExprKind::Bool: return e.bval ? "true" : "false";
		case XExprKind::String: return "\"" + std::string(in.view(e.name)) + "\"";
		case XExprKind::Color: { char b[16]; std::snprintf(b, sizeof b, "#%08x", e.color); return b; }
		case XExprKind::Name: return std::string(in.view(e.name));
		case XExprKind::Interp: {
			std::string s = "(interp";
			for (u32 i = 0; i < e.argCount; ++i) {
				const XInterpPart& p = d.parts[e.argFirst + i];
				s += p.expr == kNoExpr ? " \"" + std::string(in.view(p.lit)) + "\"" : " " + dumpExpr(d, in, p.expr);
			}
			return s + ")";
		}
		case XExprKind::Member: return "(. " + dumpExpr(d, in, e.a) + " " + std::string(in.view(e.name)) + ")";
		case XExprKind::Index: return "([] " + dumpExpr(d, in, e.a) + " " + dumpExpr(d, in, e.b) + ")";
		case XExprKind::Call: {
			std::string s = "(call " + dumpExpr(d, in, e.a);
			for (u32 i = 0; i < e.argCount; ++i) s += " " + dumpExpr(d, in, d.callArgs[e.argFirst + i]);
			return s + ")";
		}
		case XExprKind::Unary: return std::string("(") + (e.op == XOp::Neg ? "neg" : "!") + " " + dumpExpr(d, in, e.a) + ")";
		case XExprKind::Ternary: return "(? " + dumpExpr(d, in, e.a) + " " + dumpExpr(d, in, e.b) + " " + dumpExpr(d, in, e.c) + ")";
		case XExprKind::Range: return "(.. " + dumpExpr(d, in, e.a) + " " + dumpExpr(d, in, e.b) + ")";
		case XExprKind::Binary: {
			static const char* ops[] = { "", "neg", "!", "+", "-", "*", "/", "%", "==", "!=", "<", "<=", ">", ">=", "&&", "||" };
			return std::string("(") + ops[static_cast<int>(e.op)] + " " + dumpExpr(d, in, e.a) + " " + dumpExpr(d, in, e.b) + ")";
		}
		}
		return {};
	}
}
