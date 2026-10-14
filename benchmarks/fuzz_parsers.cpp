// Mutation fuzzer for the .xui and .xss front ends. Run under ASan/UBSan: any crash, sanitizer report or hang is a bug.
// Inputs are mutated copies of valid sources, so most reach deep into the parsers' error-recovery paths.
#include <xui/lang/xui.hpp>
#include <xui/style/xss.hpp>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
using namespace xui;
static const char* kXui[] = {
 "Window(title: \"Main Screen\", size: 800x600) {\n Column(gap: 8, pad: 16, align: Stretch) {\n Text(\"Enter your name\")\n TextBox[placeholder: \"Type here\", bind: userName, grow: 1]\n Row(gap: 8, justify: End) {\n Button[label: \"Click Me\", variant: Primary, press: onSubmit]\n }\n }\n}\n",
 "@import \"a.xui\"\ncomponent Card(title, note: \"x\") { Panel(title: title) { Slot() Slot(name: \"f\") } }\nColumn { Row[for: i, n in items, if: a && !b] { Text(\"{i + 1}: {user.name}\") } Card(\"x\") { Text(list[0].f(1, 2) ? \"a\" : \"b\") } }\n"};
static const char* kXss[] = {
 "tokens { color.bg = #0b0b0b\n color.primary = #3b82f6\n space.2 = 8px\n space.4 = space.2 * 2\n radius.md = 6px }\nWindow { resting: { bg: color.bg } }\nTextBox { resting: { bg: color.bg.darken(10%), radius: radius.md, pad: 8px 12px }\n focus: { border: 1px solid color.primary } }\nButton[variant: Primary] { resting: { bg: color.primary, pad: 8px 16px } hover: { bg: color.primary.lighten(5%) }\n transition: resting -> hover 120ms ease-out\n transition: hover -> pressed 60ms cubic-bezier(0.4, 0, 0.2, 1) }\n@theme dark { tokens { color.bg = #101010 } }\nkeyframes pulse { from { opacity: 1 } 50% { opacity: 0.5, blur: 2px } to { opacity: 1 } }\nSpinner { resting: { animation: pulse 1s ease-in-out infinite, transform: translate(0px, -2px) scale(1.1) } }\n"};
static std::string mutate(std::string s, std::mt19937& g) {
    static const char junk[] = "{}[]()\"'#@:;,.=+-*/%<>!?&| \n\t0123456789abcxyz\\";
    int n = 1 + int(g() % 6);
    for (int k = 0; k < n && !s.empty(); ++k) {
        std::size_t p = g() % s.size();
        switch (g() % 5) {
            case 0: s.erase(p, 1 + g() % 8); break;
            case 1: s.insert(p, 1, junk[g() % (sizeof junk - 1)]); break;
            case 2: s[p] = junk[g() % (sizeof junk - 1)]; break;
            case 3: s.resize(p); break;                                   // truncate
            default: { std::size_t q = g() % s.size(); s.insert(p, s.substr(q, 1 + g() % 12)); }   // duplicate a chunk
        }
    }
    return s;
}
static void save(const char* kind, const std::string& s) { if (FILE* f = std::fopen("fuzz_last.txt", "wb")) { std::fprintf(f, "%s\n", kind); std::fwrite(s.data(), 1, s.size(), f); std::fclose(f); } }
int main(int argc, char** argv) {
    int iters = argc > 1 ? std::atoi(argv[1]) : 20000;
    std::mt19937 g(argc > 2 ? unsigned(std::atoi(argv[2])) : 12345u);
    StringInterner in;
    std::size_t ok = 0, bad = 0; double worst = 0;
    for (int i = 0; i < iters; ++i) {
        for (const char* base : kXui) {
            std::string s = mutate(base, g); save("xui", s);
            auto t0 = std::chrono::steady_clock::now();
            auto r = parseXui(s, "f.xui", in);
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); if (ms > worst) worst = ms;
            if (r) { ++ok; } else { ++bad; (void)r.error().format(); }
        }
        for (const char* base : kXss) {
            std::string s = mutate(base, g); save("xss", s);
            auto t0 = std::chrono::steady_clock::now();
            auto r = compileXss(s, "f.xss", in);
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); if (ms > worst) worst = ms;
            if (r) { ++ok; } else { ++bad; (void)r.error().format(); }
        }
    }
    std::printf("%d iterations x 3 sources: %zu accepted, %zu rejected with diagnostics, 0 crashes. slowest single input: %.2f ms\n", iters, ok, bad, worst);
}
