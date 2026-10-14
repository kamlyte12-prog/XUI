// .xui parse benchmark: a large synthetic UI (hot-reload budget: < 30 ms).
#include <xui/lang/xui.hpp>
#include <chrono>
#include <cstdio>
#include <string>
int main() {
    std::string src = "Window(title: \"Big\", size: 1920x1080) {\n";
    for (int i = 0; i < 400; ++i) {
        src += "  Panel(title: \"Panel " + std::to_string(i) + "\") {\n    Row(gap: 8, justify: End) {\n"
               "      Text(\"Item {index} of {items.count() + 1}\")\n      NumberField[value: obj.x, bind: obj.pos[" + std::to_string(i % 3) + "], min: 0, max: 100]\n"
               "      Button[label: \"Go\", variant: Primary, press: run, if: enabled && !busy]\n    }\n  }\n";
    }
    src += "}\n";
    xui::StringInterner in;
    auto t0 = std::chrono::steady_clock::now();
    auto r = xui::parseXui(src, "big.xui", in);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (!r) { std::printf("%s", r.error().format().c_str()); return 1; }
    std::printf("source: %zu bytes, %zu nodes, %zu exprs\nparse: %.2f ms  (budget: hot reload < 30 ms)\n", src.size(), r->nodes.size(), r->exprs.size(), ms);
    std::printf("Document footprint (vectors): ~%.0f KB\n", (r->nodes.size() * sizeof(xui::XNode) + r->exprs.size() * sizeof(xui::XExpr) + r->props.size() * sizeof(xui::XProp) + r->args.size() * sizeof(xui::XArg)) / 1024.0);
}
