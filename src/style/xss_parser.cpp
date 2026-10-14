#include "xss_ast.hpp"
#include <xui/core/color.hpp>
#include <charconv>
#include <string>
namespace xui::xss {
namespace {

enum class Tok : u8 { End, Ident, Number, String, Hash, LBrace, RBrace, LBracket, RBracket, LParen, RParen,
                      Colon, Comma, Semi, Dot, Eq, Arrow, Plus, Minus, Star, Slash, At };
struct Token { Tok k = Tok::End; u32 off = 0, len = 0; f32 num = 0; Unit unit = Unit::None; };

bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool isIdChar(char c) { return isAlpha(c) || isDigit(c) || c == '-'; }

bool unitFromName(std::string_view s, Unit& u) {
    struct E { std::string_view n; Unit u; };
    static constexpr E t[] = {{"px", Unit::Px}, {"em", Unit::Em}, {"rem", Unit::Rem}, {"vw", Unit::Vw}, {"vh", Unit::Vh},
                              {"dp", Unit::Dp}, {"sp", Unit::Sp}, {"ms", Unit::Ms}, {"s", Unit::S}, {"deg", Unit::Deg}};
    for (const E& e : t) if (e.n == s) { u = e.u; return true; }
    return false;
}

class Lexer {
public:
    Lexer(std::string_view s, DiagnosticList& d) : m_s(s), m_d(d) {}
    void run(std::vector<Token>& out) {
        for (;;) {
            skipTrivia();
            if (m_i >= m_s.size()) { out.push_back({Tok::End, u32(m_i), 0}); return; }
            char c = m_s[m_i]; u32 st = u32(m_i);
            auto one = [&](Tok k) { ++m_i; out.push_back({k, st, 1}); };
            if (isAlpha(c)) {
                while (m_i < m_s.size() && isIdChar(m_s[m_i])) {
                    if (m_s[m_i] == '-' && m_i + 1 < m_s.size() && m_s[m_i + 1] == '>') break;   // `a->b`
                    ++m_i;
                }
                out.push_back({Tok::Ident, st, u32(m_i - st)});
            } else if (isDigit(c) || (c == '-' && m_i + 1 < m_s.size() && isDigit(m_s[m_i + 1]) && negativeOk(m_i))) {
                number(out);
            } else if (c == '-' && m_i + 1 < m_s.size() && m_s[m_i + 1] == '>') { m_i += 2; out.push_back({Tok::Arrow, st, 2}); }
            else if (c == '"') string(out);
            else if (c == '#') { ++m_i; while (m_i < m_s.size() && isIdChar(m_s[m_i])) ++m_i; out.push_back({Tok::Hash, st, u32(m_i - st)}); }
            else switch (c) {
                case '{': one(Tok::LBrace); break;   case '}': one(Tok::RBrace); break;
                case '[': one(Tok::LBracket); break; case ']': one(Tok::RBracket); break;
                case '(': one(Tok::LParen); break;   case ')': one(Tok::RParen); break;
                case ':': one(Tok::Colon); break;    case ',': one(Tok::Comma); break;
                case ';': one(Tok::Semi); break;     case '.': one(Tok::Dot); break;
                case '=': one(Tok::Eq); break;       case '+': one(Tok::Plus); break;
                case '-': one(Tok::Minus); break;    case '*': one(Tok::Star); break;
                case '/': one(Tok::Slash); break;    case '@': one(Tok::At); break;
                default:
                    m_d.error(st, 1, std::string("unexpected character '") + c + "'");
                    ++m_i; break;
            }
        }
    }
private:
    // `-2px` is a negative literal only where a value can start; `a - b` and `a-b` stay operators.
    bool negativeOk(std::size_t i) const {
        if (i == 0) return true;
        char p = m_s[i - 1];
        return p == ' ' || p == '\t' || p == '\n' || p == '\r' || p == '(' || p == ',' || p == ':' || p == '=';
    }
    void skipTrivia() {
        while (m_i < m_s.size()) {
            char c = m_s[m_i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++m_i;
            else if (c == '/' && m_i + 1 < m_s.size() && m_s[m_i + 1] == '/') { while (m_i < m_s.size() && m_s[m_i] != '\n') ++m_i; }
            else if (c == '/' && m_i + 1 < m_s.size() && m_s[m_i + 1] == '*') {
                std::size_t st = m_i; m_i += 2;
                while (m_i + 1 < m_s.size() && !(m_s[m_i] == '*' && m_s[m_i + 1] == '/')) ++m_i;
                if (m_i + 1 >= m_s.size()) { m_d.error(u32(st), 2, "unterminated block comment", "close it with */"); m_i = m_s.size(); }
                else m_i += 2;
            } else break;
        }
    }
    void number(std::vector<Token>& out) {
        u32 st = u32(m_i);
        std::size_t b = m_i;
        if (m_s[m_i] == '-') ++m_i;
        while (m_i < m_s.size() && isDigit(m_s[m_i])) ++m_i;
        if (m_i + 1 < m_s.size() && m_s[m_i] == '.' && isDigit(m_s[m_i + 1])) { ++m_i; while (m_i < m_s.size() && isDigit(m_s[m_i])) ++m_i; }
        f32 v = 0;
        std::from_chars(m_s.data() + b, m_s.data() + m_i, v);
        Unit u = Unit::None;
        if (m_i < m_s.size() && m_s[m_i] == '%') { u = Unit::Percent; ++m_i; }
        else if (m_i < m_s.size() && isAlpha(m_s[m_i])) {
            std::size_t us = m_i;
            while (m_i < m_s.size() && isAlpha(m_s[m_i])) ++m_i;
            if (!unitFromName(m_s.substr(us, m_i - us), u))
                m_d.error(u32(us), u32(m_i - us), "unknown unit '" + std::string(m_s.substr(us, m_i - us)) + "'", "valid units: px % em rem vw vh dp sp ms s deg");
        }
        out.push_back({Tok::Number, st, u32(m_i - st), v, u});
    }
    void string(std::vector<Token>& out) {
        u32 st = u32(m_i); ++m_i;
        while (m_i < m_s.size() && m_s[m_i] != '"' && m_s[m_i] != '\n') { if (m_s[m_i] == '\\') ++m_i; ++m_i; }
        if (m_i >= m_s.size() || m_s[m_i] != '"') { m_d.error(st, 1, "unterminated string"); }
        else ++m_i;
        out.push_back({Tok::String, st, u32(m_i - st)});
    }
    std::string_view m_s; DiagnosticList& m_d; std::size_t m_i = 0;
};

class Parser {
public:
    Parser(std::string_view s, StringInterner& in, DiagnosticList& d, Ast& a) : m_s(s), m_in(in), m_d(d), m_a(a) {}
    void parseFile() {
        Lexer(m_s, m_d).run(m_t);
        if (m_a.themes.empty()) m_a.themes.push_back({m_in.intern("default"), {}});
        while (peek().k != Tok::End) {
            const Token& t = peek();
            if (t.k == Tok::Ident && text(t) == "tokens" && peek(1).k == Tok::LBrace) { next(); next(); parseTokenBody(m_a.themes[0].tokens); expect(Tok::RBrace, "'}'"); }
            else if (t.k == Tok::At) parseTheme();
            else if (t.k == Tok::Ident && text(t) == "keyframes" && peek(1).k == Tok::Ident) parseKeyframes();
            else parseRule();
        }
    }
private:
    // ---- token helpers ---------------------------------------------------------------------
    const Token& peek(std::size_t n = 0) const { std::size_t i = m_p + n; return m_t[i < m_t.size() ? i : m_t.size() - 1]; }
    Token next() { Token t = peek(); if (m_p + 1 < m_t.size()) ++m_p; return t; }
    bool accept(Tok k) { if (peek().k == k) { next(); return true; } return false; }
    std::string_view text(const Token& t) const { return m_s.substr(t.off, t.len); }
    void err(const Token& t, std::string msg, std::string hint = {}) { m_d.error(t.off, t.len, std::move(msg), std::move(hint)); }
    bool expect(Tok k, const char* what) {
        if (peek().k == k) { next(); return true; }
        err(peek(), std::string("expected ") + what + ", found " + describe(peek()));
        return false;
    }
    std::string describe(const Token& t) const {
        if (t.k == Tok::End) return "end of file";
        return "'" + std::string(text(t)) + "'";
    }
    // Skip to the end of the current declaration without consuming the closing brace.
    void syncDecl() {
        int depth = 0;
        for (;;) {
            Tok k = peek().k;
            if (k == Tok::End) return;
            if (depth == 0 && (k == Tok::Comma || k == Tok::Semi || k == Tok::RBrace)) return;
            if (k == Tok::LParen || k == Tok::LBracket || k == Tok::LBrace) ++depth;
            if (k == Tok::RParen || k == Tok::RBracket || k == Tok::RBrace) --depth;
            next();
        }
    }
    // Skip a whole `{ ... }` block (used when a rule header is broken).
    void skipBlock(int depth = 0) {
        for (;;) {
            Token t = next();
            if (t.k == Tok::End) return;
            if (t.k == Tok::LBrace) ++depth;
            if (t.k == Tok::RBrace && --depth <= 0) return;
        }
    }

    // ---- expressions -----------------------------------------------------------------------
    u32 push(const Expr& e) { m_a.exprs.push_back(e); return u32(m_a.exprs.size() - 1); }
    static bool startsExpr(Tok k) { return k == Tok::Number || k == Tok::Ident || k == Tok::Hash || k == Tok::String || k == Tok::LParen; }

    u32 parseExpr() {
        DepthGuard g(m_depth);
        if (!g.ok) { err(peek(), "expression is nested too deeply"); return kNone; }
        u32 l = parseTerm();
        while (l != kNone && (peek().k == Tok::Plus || peek().k == Tok::Minus)) {
            Token o = next(); u32 r = parseTerm();
            if (r == kNone) return kNone;
            Expr e; e.kind = ExprKind::Binary; e.op = o.k == Tok::Plus ? '+' : '-'; e.a = l; e.b = r;
            e.off = m_a.exprs[l].off; e.len = m_a.exprs[r].off + m_a.exprs[r].len - e.off;
            l = push(e);
        }
        return l;
    }
    u32 parseTerm() {
        u32 l = parseUnary();
        while (l != kNone && (peek().k == Tok::Star || peek().k == Tok::Slash)) {
            Token o = next(); u32 r = parseUnary();
            if (r == kNone) return kNone;
            Expr e; e.kind = ExprKind::Binary; e.op = o.k == Tok::Star ? '*' : '/'; e.a = l; e.b = r;
            e.off = m_a.exprs[l].off; e.len = m_a.exprs[r].off + m_a.exprs[r].len - e.off;
            l = push(e);
        }
        return l;
    }
    u32 parseUnary() {
        DepthGuard g(m_depth);
        if (!g.ok) { err(peek(), "expression is nested too deeply"); return kNone; }
        if (peek().k == Tok::Minus) {
            Token m = next(); u32 o = parseUnary();
            if (o == kNone) return kNone;
            Expr e; e.kind = ExprKind::Neg; e.a = o; e.off = m.off; e.len = m_a.exprs[o].off + m_a.exprs[o].len - m.off;
            return push(e);
        }
        return parsePostfix();
    }
    u32 parsePostfix() {
        u32 e = parsePrimary();
        while (e != kNone && peek().k == Tok::Dot && (peek(1).k == Tok::Ident || peek(1).k == Tok::Number)) {
            bool method = peek(1).k == Tok::Ident && peek(2).k == Tok::LParen;
            if (method) {
                next(); Token nm = next(); next();
                std::vector<u32> args;
                if (!parseArgs(args)) return kNone;
                Token close = m_t[m_p - 1];
                Expr m; m.kind = ExprKind::Method; m.a = e; m.name = m_in.intern(text(nm));
                m.off = m_a.exprs[e].off; m.len = close.off + close.len - m.off;
                m.argFirst = u32(m_a.args.size()); m.argCount = u32(args.size());
                m_a.args.insert(m_a.args.end(), args.begin(), args.end());
                e = push(m);
            } else if (m_a.exprs[e].kind == ExprKind::Path) {
                next(); Token seg = next();
                Expr& p = m_a.exprs[e];
                p.name = m_in.intern(std::string(m_in.view(p.name)) + "." + std::string(text(seg)));
                p.len = seg.off + seg.len - p.off;
            } else break;
        }
        return e;
    }
    bool parseArgs(std::vector<u32>& out) {   // after '(' ; consumes ')'
        if (accept(Tok::RParen)) return true;
        for (;;) {
            u32 a = parseExpr();
            if (a == kNone) return false;
            out.push_back(a);
            if (accept(Tok::Comma)) continue;
            return expect(Tok::RParen, "',' or ')'");
        }
    }
    u32 parsePrimary() {
        Token t = peek();
        Expr e; e.off = t.off; e.len = t.len;
        switch (t.k) {
            case Tok::Number: next(); e.kind = ExprKind::Number; e.num = t.num; e.unit = t.unit; return push(e);
            case Tok::Hash: {
                next(); Color c;
                if (!Color::parseHex(text(t), c)) { err(t, "invalid colour '" + std::string(text(t)) + "'", "use #rgb, #rgba, #rrggbb or #rrggbbaa"); return kNone; }
                e.kind = ExprKind::Color; e.color = c.toRGBA8(); return push(e);
            }
            case Tok::String: {
                next(); e.kind = ExprKind::String;
                std::string v; std::string_view raw = text(t);
                for (std::size_t i = 1; i + 1 < raw.size(); ++i) {
                    if (raw[i] == '\\' && i + 2 < raw.size()) { ++i; v += raw[i] == 'n' ? '\n' : raw[i]; } else v += raw[i];
                }
                e.name = m_in.intern(v); return push(e);
            }
            case Tok::Ident: {
                next();
                if (peek().k == Tok::LParen) {
                    next(); e.kind = ExprKind::Call; e.name = m_in.intern(text(t));
                    std::vector<u32> args;
                    if (!parseArgs(args)) return kNone;
                    e.len = m_t[m_p - 1].off + 1 - e.off;
                    e.argFirst = u32(m_a.args.size()); e.argCount = u32(args.size());
                    m_a.args.insert(m_a.args.end(), args.begin(), args.end());
                    return push(e);
                }
                e.kind = ExprKind::Path; e.name = m_in.intern(text(t)); return push(e);
            }
            case Tok::LParen: {
                next(); u32 inner = parseExpr();
                if (inner == kNone || !expect(Tok::RParen, "')'")) return kNone;
                return inner;
            }
            default:
                err(t, "expected a value, found " + describe(t));
                return kNone;
        }
    }

    // ---- declarations ----------------------------------------------------------------------
    bool parseDecl(std::vector<DeclAst>& out) {
        Token name = next();
        if (peek().k != Tok::Colon) { err(peek(), "expected ':' after property name '" + std::string(text(name)) + "'"); return false; }
        next();
        DeclAst d; d.off = name.off;
        if (!findProp(text(name), d.prop)) {
            std::string s = didYouMean(text(name), propNames());
            err(name, "unknown property '" + std::string(text(name)) + "'", s.empty() ? "" : "did you mean '" + s + "'?");
            return false;
        }
        while (startsExpr(peek().k)) {
            u32 e = parseExpr();
            if (e == kNone) return false;
            d.values.push_back(e);
        }
        if (d.values.empty()) { err(peek(), "expected a value for '" + std::string(text(name)) + "', found " + describe(peek())); return false; }
        const Token& last = m_t[m_p - 1];
        d.len = last.off + last.len - d.off;
        Tok k = peek().k;
        if (k != Tok::Comma && k != Tok::Semi && k != Tok::RBrace && k != Tok::End) {
            err(peek(), "expected ',' or '}' after value, found " + describe(peek()), "separate declarations with ','");
            return false;
        }
        out.push_back(std::move(d));
        return true;
    }
    void parseDeclList(std::vector<DeclAst>& out) {   // until '}' (not consumed)
        while (peek().k != Tok::RBrace && peek().k != Tok::End) {
            if (accept(Tok::Comma) || accept(Tok::Semi)) continue;
            if (peek().k != Tok::Ident) { err(peek(), "expected a property name, found " + describe(peek())); syncDecl(); if (peek().k != Tok::RBrace) accept(Tok::Comma); continue; }
            if (!parseDecl(out)) syncDecl();
        }
    }

    // ---- tokens / themes -------------------------------------------------------------------
    bool looksLikeTokenStart(std::size_t i) const {
        auto at = [&](std::size_t j) -> const Token& { return m_t[j < m_t.size() ? j : m_t.size() - 1]; };
        if (at(i).k != Tok::Ident) return false;
        ++i;
        while (at(i).k == Tok::Dot && (at(i + 1).k == Tok::Ident || at(i + 1).k == Tok::Number)) i += 2;
        return at(i).k == Tok::Eq;
    }
    void parseTokenBody(std::vector<TokenDef>& out) {   // until '}' (not consumed)
        while (peek().k != Tok::RBrace && peek().k != Tok::End) {
            if (accept(Tok::Comma) || accept(Tok::Semi)) continue;
            if (!looksLikeTokenStart(m_p)) {
                err(peek(), "expected a token definition like `color.bg = #000`, found " + describe(peek()));
                do next(); while (peek().k != Tok::End && peek().k != Tok::RBrace && !looksLikeTokenStart(m_p));
                continue;
            }
            Token first = next(); std::string name(text(first)); Token last = first;
            while (peek().k == Tok::Dot) { next(); last = next(); name += "."; name += text(last); }
            next();   // '='
            u32 e = parseExpr();
            if (e == kNone) { do next(); while (peek().k != Tok::End && peek().k != Tok::RBrace && !looksLikeTokenStart(m_p)); continue; }
            out.push_back({m_in.intern(name), e, first.off, last.off + last.len - first.off});
        }
    }
    void parseTheme() {
        next();   // '@'
        Token kw = next();
        if (kw.k != Tok::Ident || text(kw) != "theme") { err(kw, "unknown at-rule " + describe(kw), "the only at-rule is @theme"); skipBlock(); return; }
        Token nm = peek();
        if (nm.k != Tok::Ident) { err(nm, "expected a theme name after @theme"); skipBlock(); return; }
        next();
        StrId id = m_in.intern(text(nm));
        std::size_t idx = m_a.themes.size();
        for (std::size_t i = 0; i < m_a.themes.size(); ++i) if (m_a.themes[i].name == id) idx = i;
        if (idx == m_a.themes.size()) m_a.themes.push_back({id, {}});
        if (!expect(Tok::LBrace, "'{'")) { skipBlock(); return; }
        while (peek().k != Tok::RBrace && peek().k != Tok::End) {
            if (accept(Tok::Comma) || accept(Tok::Semi)) continue;
            if (peek().k == Tok::Ident && text(peek()) == "tokens" && peek(1).k == Tok::LBrace) {
                next(); next(); parseTokenBody(m_a.themes[idx].tokens); expect(Tok::RBrace, "'}'");
            } else parseTokenBody(m_a.themes[idx].tokens);   // bare entries are accepted too
        }
        expect(Tok::RBrace, "'}'");
    }

    // ---- selectors / rules -----------------------------------------------------------------
    bool parseSelector(Selector& s) {
        bool any = false;
        if (peek().k == Tok::Ident) { s.type = m_in.intern(text(next())); any = true; }
        else if (peek().k == Tok::Star) { next(); any = true; }
        for (;;) {
            if (peek().k == Tok::Dot && peek(1).k == Tok::Ident) { next(); s.classes.push_back(m_in.intern(text(next()))); any = true; }
            else if (peek().k == Tok::Hash) {
                Token h = next(); if (h.len < 2) { err(h, "empty #id"); return false; }
                s.id = m_in.intern(text(h).substr(1)); any = true;
            } else if (peek().k == Tok::LBracket) {
                next();
                for (;;) {
                    Token pn = peek();
                    if (pn.k != Tok::Ident) { err(pn, "expected a property name in filter, found " + describe(pn)); return false; }
                    next();
                    if (!expect(Tok::Colon, "':'")) return false;
                    Token v = next(); SelectorFilter f; f.prop = m_in.intern(text(pn));
                    if (v.k == Tok::Ident) f.value = Atom::ident(m_in.intern(text(v)));
                    else if (v.k == Tok::Number) f.value = Atom::number(v.num, v.unit);
                    else if (v.k == Tok::String) f.value = Atom::string(m_in.intern(text(v).substr(1, v.len >= 2 ? v.len - 2 : 0)));
                    else { err(v, "filter value must be an identifier, number or string, found " + describe(v)); return false; }
                    s.filters.push_back(f);
                    if (accept(Tok::Comma)) continue;
                    if (!expect(Tok::RBracket, "']'")) return false;
                    break;
                }
                any = true;
            } else break;
        }
        if (!any) { err(peek(), "expected a selector, found " + describe(peek())); return false; }
        return true;
    }
    static StateBlockAst& blockFor(RuleAst& r, State s) {
        for (StateBlockAst& b : r.blocks) if (b.state == s) return b;
        r.blocks.push_back({s, {}}); return r.blocks.back();
    }
    void parseRule() {
        RuleAst rule;
        for (;;) {
            Selector s;
            if (!parseSelector(s)) { skipBlock(); return; }
            rule.sels.push_back(std::move(s));
            if (accept(Tok::Comma)) continue;
            break;
        }
        if (!expect(Tok::LBrace, "'{'")) { skipBlock(); return; }
        while (peek().k != Tok::RBrace && peek().k != Tok::End) {
            if (accept(Tok::Comma) || accept(Tok::Semi)) continue;
            const Token& t = peek();
            if (t.k != Tok::Ident) { err(t, "expected a property, state or 'transition', found " + describe(t)); syncDecl(); continue; }
            if (text(t) == "transition" && peek(1).k == Tok::Colon) { next(); next(); parseTransition(rule); continue; }
            if (peek(1).k == Tok::Colon && peek(2).k == Tok::LBrace) {
                Token sn = next(); next(); next();
                State st;
                if (!findState(text(sn), st)) {
                    std::string s = didYouMean(text(sn), stateNames());
                    err(sn, "unknown state '" + std::string(text(sn)) + "'", s.empty() ? "" : "did you mean '" + s + "'?");
                    skipBlock(1);   // '{' already consumed
                    continue;
                }
                parseDeclList(blockFor(rule, st).decls);
                expect(Tok::RBrace, "'}'");
            } else if (!parseDecl(blockFor(rule, State::Resting).decls)) syncDecl();
        }
        expect(Tok::RBrace, "'}'");
        m_a.rules.push_back(std::move(rule));
    }

    // ---- transitions / keyframes -----------------------------------------------------------
    bool timeMs(const Token& t, f32& out) {
        if (t.unit == Unit::Ms) out = t.num; else if (t.unit == Unit::S) out = t.num * 1000.f;
        else { err(t, "expected a time with unit ms or s"); return false; }
        return true;
    }
    bool namedEase(std::string_view n, Easing& e) {
        if (n == "linear") e.kind = EaseKind::Linear; else if (n == "ease-in") e.kind = EaseKind::EaseIn;
        else if (n == "ease-out") e.kind = EaseKind::EaseOut; else if (n == "ease-in-out" || n == "ease") e.kind = EaseKind::EaseInOut;
        else return false;
        return true;
    }
    void parseTransition(RuleAst& rule) {
        TransAst tr; Token a = next(), arrow, b;
        if (a.k != Tok::Ident || !findState(text(a), tr.from)) { err(a, "expected a state name, found " + describe(a), "states: resting hover pressed focus disabled ..."); syncDecl(); return; }
        arrow = next();
        if (arrow.k != Tok::Arrow) { err(arrow, "expected '->' between states, found " + describe(arrow)); syncDecl(); return; }
        b = next();
        if (b.k != Tok::Ident || !findState(text(b), tr.to)) { err(b, "expected a state name, found " + describe(b)); syncDecl(); return; }
        bool haveDur = false, haveDelay = false;
        for (;;) {
            const Token& t = peek();
            if (t.k == Tok::Comma || t.k == Tok::Semi || t.k == Tok::RBrace || t.k == Tok::End) break;
            if (t.k == Tok::Ident && peek(1).k == Tok::Colon) break;   // next declaration, separator forgotten
            if (t.k == Tok::Number) {
                Token n = next(); f32 ms;
                if (!timeMs(n, ms)) { syncDecl(); return; }
                if (!haveDur) { tr.dur = ms; haveDur = true; } else if (!haveDelay) { tr.delay = ms; haveDelay = true; }
                else { err(n, "too many times in transition"); syncDecl(); return; }
            } else if (t.k == Tok::Ident) {
                Token n = next(); std::string_view nm = text(n);
                if (namedEase(nm, tr.ease)) continue;
                if (nm == "cubic-bezier" || nm == "spring") {
                    if (!expect(Tok::LParen, "'('")) { syncDecl(); return; }
                    f32 v[4] = {0, 0, 0, 0}; int cnt = 0;
                    while (peek().k == Tok::Number && cnt < 4) { v[cnt++] = next().num; if (!accept(Tok::Comma)) break; }
                    if (!expect(Tok::RParen, "')'")) { syncDecl(); return; }
                    if (nm == "cubic-bezier") {
                        if (cnt != 4) { err(n, "cubic-bezier() takes 4 numbers"); syncDecl(); return; }
                        tr.ease.kind = EaseKind::CubicBezier; for (int i = 0; i < 4; ++i) tr.ease.p[i] = v[i];
                    } else {
                        tr.ease.kind = EaseKind::Spring; tr.ease.p[0] = cnt > 0 ? v[0] : 170.f; tr.ease.p[1] = cnt > 1 ? v[1] : 26.f; tr.ease.p[2] = cnt > 2 ? v[2] : 1.f;
                    }
                } else if (nm == "all") { tr.all = true; }
                else if (findProp(nm, tr.prop)) { tr.all = false; }
                else {
                    std::string s = didYouMean(nm, propNames());
                    err(n, "expected a property, easing or time, found '" + std::string(nm) + "'", s.empty() ? "easings: linear ease-in ease-out ease-in-out cubic-bezier() spring()" : "did you mean '" + s + "'?");
                    syncDecl(); return;
                }
            } else { err(t, "unexpected " + describe(t) + " in transition"); syncDecl(); return; }
        }
        if (!haveDur) { err(b, "transition needs a duration (e.g. 120ms)"); return; }
        rule.trans.push_back(tr);
    }
    void parseKeyframes() {
        next(); Token nm = next();
        KeyframesAst k; k.name = m_in.intern(text(nm)); k.off = nm.off; k.len = nm.len;
        if (!expect(Tok::LBrace, "'{'")) { skipBlock(); return; }
        while (peek().k != Tok::RBrace && peek().k != Tok::End) {
            if (accept(Tok::Comma) || accept(Tok::Semi)) continue;
            Token t = next(); FrameAst f;
            if (t.k == Tok::Number && t.unit == Unit::Percent) f.t = t.num / 100.f;
            else if (t.k == Tok::Ident && text(t) == "from") f.t = 0.f;
            else if (t.k == Tok::Ident && text(t) == "to") f.t = 1.f;
            else { err(t, "expected a keyframe offset like 50%, 'from' or 'to', found " + describe(t)); skipBlock(); continue; }
            if (!expect(Tok::LBrace, "'{'")) { skipBlock(); continue; }
            parseDeclList(f.decls);
            expect(Tok::RBrace, "'}'");
            k.frames.push_back(std::move(f));
        }
        expect(Tok::RBrace, "'}'");
        m_a.keyframes.push_back(std::move(k));
    }

    // Recursive descent must not overflow the stack on hostile input: every recursive entry point holds a DepthGuard.
    static constexpr int kMaxDepth = 512;   // guarded frames: ~170 nested parentheses (3 guards each) or 512 nested widgets
    int m_depth = 0;
    struct DepthGuard {
        int& d; bool ok;
        explicit DepthGuard(int& depth) : d(depth), ok(++depth <= kMaxDepth) {}
        ~DepthGuard() { --d; }
    };
    std::string_view m_s; StringInterner& m_in; DiagnosticList& m_d; Ast& m_a;
    std::vector<Token> m_t; std::size_t m_p = 0;
};
}
bool parse(std::string_view src, StringInterner& in, DiagnosticList& diags, Ast& ast) {
    std::size_t before = diags.items().size();
    Parser(src, in, diags, ast).parseFile();
    return diags.items().size() == before;
}
}
