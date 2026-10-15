// M2 micro-benchmarks: .xss compile time (hot-reload budget < 30 ms) and per-node resolve cost.
#include <xui/style/xss.hpp>
#include <chrono>
#include <cstdio>
#include <string>
using Clock = std::chrono::steady_clock;
static double ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }
int main() {
	// Synthesize a large stylesheet: 500 rules x 4 states, ~6 declarations each.
	std::string src = "tokens { color.a = #3b82f6\n space.1 = 4px\n radius.md = 6px }\n";
	for (int i = 0; i < 500; ++i) {
		src += "W" + std::to_string(i) + "[variant: V] {\n resting: { bg: color.a.darken(10%), fg: #fff, pad: space.1 8px, radius: radius.md, gap: space.1, opacity: 1 }\n"
			" hover: { bg: color.a.lighten(5%) }\n pressed: { bg: color.a.darken(20%) }\n disabled: { opacity: 0.6 }\n transition: resting -> hover 120ms ease-out\n}\n";
	}
	xui::StringInterner in;
	auto t0 = Clock::now();
	auto r = xui::compileXss(src, "bench.xss", in);
	auto t1 = Clock::now();
	if (!r) { std::printf("%s", r.error().format().c_str()); return 1; }
	std::printf("source: %zu bytes, %zu rules, %zu decls\n", src.size(), r->ruleCount(), r->declCount());
	std::printf("compile: %.2f ms  (budget: hot reload < 30 ms)\n", ms(t0, t1));

	struct L { xui::StrId p, v; } l{ in.intern("variant"), in.intern("V") };
	xui::MatchInput mi; mi.type = in.intern("W250"); mi.ctx = &l;
	mi.lookup = [](void* c, xui::StrId p, xui::Atom& out) { auto* x = static_cast<L*>(c); if (p != x->p) return false; out = xui::Atom::ident(x->v); return true; };
	t0 = Clock::now();
	xui::StyleSetId s;
	for (int i = 0; i < 1000; ++i) s = r->match(mi);   // cached after first
	t1 = Clock::now();
	std::printf("match (cached hit): %.0f ns/call\n", ms(t0, t1) * 1e6 / 1000);

	static xui::PropTable t; const int N = 1'000'000; unsigned sink = 0;
	t0 = Clock::now();
	for (int i = 0; i < N; ++i) { r->resolve(s, (i & 1) ? xui::stateBit(xui::State::Hover) : xui::stateBit(xui::State::Pressed), 0, t); sink += t.get(xui::PropId::Bg).atoms[0].bits; }
	t1 = Clock::now();
	std::printf("resolve: %.0f ns/node  -> 1000 nodes = %.3f ms  (sink %u)\n", ms(t0, t1) * 1e6 / N, ms(t0, t1) * 1e6 / N * 1000 / 1e6, sink);
	std::printf("sizeof(Atom)=%zu sizeof(StyleValue)=%zu sizeof(PropTable)=%zu\n", sizeof(xui::Atom), sizeof(xui::StyleValue), sizeof(xui::PropTable));
}
