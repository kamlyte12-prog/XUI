#pragma once
/// @file interner.hpp  String interning: compare/hash strings as 32-bit ids.
#include <xui/core/types.hpp>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
namespace xui {
/// Interned string id. Id 0 is always the empty string.
struct StrId {
    u32 v = 0;
    friend constexpr bool operator==(StrId, StrId) = default;
    friend constexpr auto operator<=>(StrId, StrId) = default;
};
class StringInterner {
public:
    StringInterner() { m_store.emplace_back(); m_map.emplace(std::string_view(m_store.back()), 0u); }
    StringInterner(const StringInterner&) = delete;
    StringInterner& operator=(const StringInterner&) = delete;
    StrId intern(std::string_view s) {
        if (auto it = m_map.find(s); it != m_map.end()) return StrId{it->second};
        m_store.emplace_back(s);
        u32 id = static_cast<u32>(m_store.size() - 1);
        m_map.emplace(std::string_view(m_store.back()), id);
        return StrId{id};
    }
    /// @return the id if already interned, else StrId{0} with found=false.
    StrId find(std::string_view s, bool* found = nullptr) const {
        auto it = m_map.find(s);
        if (found) *found = it != m_map.end();
        return it == m_map.end() ? StrId{} : StrId{it->second};
    }
    /// View stays valid for the interner's lifetime (deque storage never relocates).
    std::string_view view(StrId id) const { return id.v < m_store.size() ? std::string_view(m_store[id.v]) : std::string_view(); }
    std::size_t size() const { return m_store.size(); }
private:
    std::deque<std::string> m_store;
    std::unordered_map<std::string_view, u32> m_map;
};
}
template <> struct std::hash<xui::StrId> { std::size_t operator()(xui::StrId s) const noexcept { return s.v * 2654435761u; } };
