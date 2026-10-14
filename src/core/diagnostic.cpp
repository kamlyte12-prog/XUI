#include <xui/core/diagnostic.hpp>
#include <algorithm>
namespace xui {
void DiagnosticList::add(Severity s, u32 offset, u32 length, std::string message, std::string hint) {
    for (const Diagnostic& d : m_items) if (d.offset == offset && d.message == message) return;
    Diagnostic d;
    d.severity = s; d.offset = std::min<u32>(offset, static_cast<u32>(m_source.size()));
    d.length = std::max<u32>(1, length);
    d.message = std::move(message); d.hint = std::move(hint);
    u32 line = 1, col = 1;
    for (u32 i = 0; i < d.offset; ++i) { if (m_source[i] == '\n') { ++line; col = 1; } else ++col; }
    d.line = line; d.col = col;
    m_items.push_back(std::move(d));
}
bool DiagnosticList::hasErrors() const {
    return std::any_of(m_items.begin(), m_items.end(), [](const Diagnostic& d) { return d.severity == Severity::Error; });
}
std::string DiagnosticList::format(const Diagnostic& d) const {
    const char* sev = d.severity == Severity::Error ? "error" : d.severity == Severity::Warning ? "warning" : "note";
    std::string out = m_file + ":" + std::to_string(d.line) + ":" + std::to_string(d.col) + ": " + sev + ": " + d.message + "\n";
    // Extract the source line containing the offset.
    std::size_t ls = d.offset; while (ls > 0 && m_source[ls - 1] != '\n') --ls;
    std::size_t le = d.offset; while (le < m_source.size() && m_source[le] != '\n') ++le;
    std::string text = m_source.substr(ls, le - ls);
    if (!text.empty() && text.back() == '\r') text.pop_back();
    std::string num = std::to_string(d.line);
    std::string gutter(num.size(), ' ');
    out += " " + num + " | " + text + "\n";
    std::size_t width = std::min<std::size_t>(d.length, text.size() > d.col - 1 ? text.size() - (d.col - 1) : 1);
    out += " " + gutter + " | " + std::string(d.col - 1, ' ') + "^" + std::string(width > 1 ? width - 1 : 0, '~') + "\n";
    if (!d.hint.empty()) out += " " + gutter + " = " + d.hint + "\n";
    return out;
}
std::string DiagnosticList::format() const {
    std::vector<const Diagnostic*> sorted;   // source order, regardless of which compiler phase found them
    for (const Diagnostic& d : m_items) sorted.push_back(&d);
    std::stable_sort(sorted.begin(), sorted.end(), [](const Diagnostic* a, const Diagnostic* b) { return a->offset < b->offset; });
    std::string out;
    for (const Diagnostic* d : sorted) out += format(*d);
    return out;
}
std::string didYouMean(std::string_view w, std::span<const std::string_view> cands) {
    std::size_t best = 1000; std::string_view bestS;
    for (std::string_view c : cands) {
        std::size_t n = w.size(), m = c.size();
        if ((n > m ? n - m : m - n) > 3) continue;
        std::vector<std::size_t> prev(m + 1), cur(m + 1);
        for (std::size_t j = 0; j <= m; ++j) prev[j] = j;
        for (std::size_t i = 1; i <= n; ++i) {
            cur[0] = i;
            for (std::size_t j = 1; j <= m; ++j)
                cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (w[i - 1] == c[j - 1] ? 0u : 1u)});
            std::swap(prev, cur);
        }
        if (prev[m] < best) { best = prev[m]; bestS = c; }
    }
    std::size_t limit = std::max<std::size_t>(2, w.size() / 3);
    return best <= limit ? std::string(bestS) : std::string();
}
}
