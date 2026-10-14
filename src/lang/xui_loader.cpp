#include <xui/lang/xui.hpp>
#include <map>
#include <unordered_set>
namespace xui {

const XComponent* Program::findComponent(StrId name, const Document** owner) const {
    auto it = components.find(name);
    if (it == components.end()) return nullptr;
    if (owner) *owner = &files[it->second.file];
    return &files[it->second.file].components[it->second.index];
}
std::string XuiError::format() const {
    std::string s;
    for (const DiagnosticList& l : lists) s += l.format();
    return s;
}

namespace {
std::string resolvePath(std::string_view importer, std::string_view rel) {
    std::string joined;
    if (!rel.empty() && rel[0] == '/') joined = std::string(rel);
    else { auto slash = importer.rfind('/'); joined = (slash == std::string_view::npos ? std::string() : std::string(importer.substr(0, slash + 1))) + std::string(rel); }
    bool abs = !joined.empty() && joined[0] == '/';
    std::vector<std::string> seg; std::size_t i = 0;
    while (i <= joined.size()) {
        std::size_t j = joined.find('/', i); if (j == std::string::npos) j = joined.size();
        std::string part = joined.substr(i, j - i);
        if (part == "..") { if (!seg.empty() && seg.back() != "..") seg.pop_back(); else if (!abs) seg.push_back(part); }
        else if (!part.empty() && part != ".") seg.push_back(part);
        i = j + 1;
    }
    std::string out = abs ? "/" : "";
    for (std::size_t k = 0; k < seg.size(); ++k) { if (k) out += '/'; out += seg[k]; }
    return out;
}

class Loader {
public:
    Loader(ReadFileFn r, void* c, StringInterner& in) : m_read(r), m_ctx(c), m_in(in) {}
    Result<Program, XuiError> run(std::string_view entry) {
        load(std::string(entry), kNone, 0, 0);
        if (m_failed.empty() && m_importErrs.empty()) return std::move(m_prog);
        XuiError e; e.lists = std::move(m_failed);
        for (auto& [idx, list] : m_importErrs) e.lists.push_back(std::move(list));
        return fail(std::move(e));
    }
private:
    static constexpr u32 kNone = 0xFFFFFFFFu, kFailed = 0xFFFFFFFEu;
    DiagnosticList& listFor(u32 file) {
        auto it = m_importErrs.find(file);
        if (it == m_importErrs.end()) it = m_importErrs.emplace(file, m_prog.files[file].diagnostics()).first;
        return it->second;
    }
    void reportAt(u32 importer, u32 off, u32 len, std::string msg, std::string hint = {}) {
        if (importer == kNone) { DiagnosticList l("<entry>", ""); l.error(0, 1, std::move(msg)); m_failed.push_back(std::move(l)); }
        else listFor(importer).error(off, len, std::move(msg), std::move(hint));
    }
    void load(const std::string& path, u32 importer, u32 off, u32 len) {
        if (auto it = m_byPath.find(path); it != m_byPath.end()) {
            if (m_stack.count(path)) {
                std::string chain; bool on = false;
                for (const std::string& p : m_order) { if (p == path) on = true; if (on) chain += p + " -> "; }
                reportAt(importer, off, len, "import cycle: " + chain + path);
            }
            return;
        }
        std::optional<std::string> src = m_read ? m_read(m_ctx, path) : std::nullopt;
        if (!src) { reportAt(importer, off, len, "cannot read '" + path + "'", "imports resolve relative to the importing file"); m_byPath[path] = kFailed; return; }
        auto parsed = parseXui(*src, path, m_in);
        if (!parsed) { m_failed.push_back(std::move(parsed.error())); m_byPath[path] = kFailed; return; }
        u32 idx = u32(m_prog.files.size());
        m_prog.files.push_back(std::move(*parsed));
        m_byPath[path] = idx; m_stack.insert(path); m_order.push_back(path);
        const Document& d = m_prog.files[idx];
        for (u32 ci = 0; ci < d.components.size(); ++ci) {
            const XComponent& c = d.components[ci];
            auto [it, fresh] = m_prog.components.emplace(c.name, Program::CompRef{idx, ci});
            if (!fresh) listFor(idx).error(c.off, c.len, "component '" + std::string(m_in.view(c.name)) + "' is already defined in " + m_prog.files[it->second.file].file);
        }
        std::vector<XImport> imports = m_prog.files[idx].imports;   // copy: files may reallocate while recursing
        for (const XImport& imp : imports) load(resolvePath(path, m_in.view(imp.path)), idx, imp.off, imp.len);
        m_stack.erase(path); m_order.pop_back();
    }
    ReadFileFn m_read; void* m_ctx; StringInterner& m_in;
    Program m_prog;
    std::unordered_map<std::string, u32> m_byPath;
    std::unordered_set<std::string> m_stack; std::vector<std::string> m_order;
    std::vector<DiagnosticList> m_failed;
    std::map<u32, DiagnosticList> m_importErrs;
};
}

Result<Program, XuiError> loadXui(std::string_view entryPath, ReadFileFn read, void* ctx, StringInterner& in) {
    return Loader(read, ctx, in).run(entryPath);
}
}
