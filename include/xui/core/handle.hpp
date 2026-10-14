#pragma once
/// @file handle.hpp  Generational handles and a chunked slot pool.
#include <xui/core/types.hpp>
#include <memory>
#include <new>
#include <utility>
#include <vector>
namespace xui {

/// Index + generation. A stale handle (slot freed/reused) is detected by get() returning nullptr.
template <class Tag> struct Handle {
    u32 index = 0xFFFFFFFFu;
    u32 gen = 0;
    constexpr bool valid() const { return index != 0xFFFFFFFFu; }
    friend constexpr bool operator==(Handle, Handle) = default;
};
using NodeId = Handle<struct NodeTag>;
using TextureHandle = Handle<struct TextureTag>;

/// Pool with stable element addresses (chunks never move) and O(1) create/destroy.
/// Steady state (create/destroy churn below the high-water mark) performs no heap allocation.
template <class T, class Tag, u32 ChunkSize = 256>
class SlotPool {
public:
    using Id = Handle<Tag>;
    SlotPool() = default;
    SlotPool(const SlotPool&) = delete;
    SlotPool& operator=(const SlotPool&) = delete;
    ~SlotPool() { clear(); }

    template <class... A> Id create(A&&... args) {
        u32 idx;
        if (!m_free.empty()) { idx = m_free.back(); m_free.pop_back(); }
        else {
            idx = m_count;
            if (idx / ChunkSize >= m_chunks.size()) m_chunks.push_back(std::make_unique<Chunk>());
            ++m_count;
        }
        Slot& s = slot(idx);
        ::new (static_cast<void*>(s.storage)) T(std::forward<A>(args)...);
        s.alive = true;
        ++m_live;
        return Id{idx, s.gen};
    }
    bool destroy(Id id) {
        Slot* s = find(id);
        if (!s) return false;
        std::launder(reinterpret_cast<T*>(s->storage))->~T();
        s->alive = false; ++s->gen; --m_live;
        m_free.push_back(id.index);
        return true;
    }
    /// @return pointer valid until the next destroy() of this id, or nullptr for stale/invalid ids.
    T* get(Id id) { Slot* s = find(id); return s ? std::launder(reinterpret_cast<T*>(s->storage)) : nullptr; }
    bool alive(Id id) const { return const_cast<SlotPool*>(this)->find(id) != nullptr; }
    u32 size() const { return m_live; }
    void clear() {
        for (u32 i = 0; i < m_count; ++i) {
            Slot& s = slot(i);
            if (s.alive) { std::launder(reinterpret_cast<T*>(s.storage))->~T(); s.alive = false; ++s.gen; }
        }
        m_free.clear(); m_free.reserve(m_count);
        for (u32 i = m_count; i-- > 0;) m_free.push_back(i);
        m_live = 0;
    }
private:
    struct Slot { alignas(T) unsigned char storage[sizeof(T)]; u32 gen = 1; bool alive = false; };
    struct Chunk { Slot slots[ChunkSize]; };
    Slot& slot(u32 i) { return m_chunks[i / ChunkSize]->slots[i % ChunkSize]; }
    Slot* find(Id id) {
        if (id.index >= m_count) return nullptr;
        Slot& s = slot(id.index);
        return (s.alive && s.gen == id.gen) ? &s : nullptr;
    }
    std::vector<std::unique_ptr<Chunk>> m_chunks;
    std::vector<u32> m_free;
    u32 m_count = 0, m_live = 0;
};
}
