#pragma once
/// @file arena.hpp  Bump allocator reset once per frame.
#include <xui/core/types.hpp>
#include <cassert>
namespace xui {
	/// Fixed-capacity bump allocator. One heap allocation at construction, none afterwards.
	/// alloc() returns nullptr on overflow (and counts it) rather than throwing.
	class FrameArena {
	public:
		explicit FrameArena(std::size_t bytes) : m_base(new std::byte[bytes]), m_cap(bytes) {}
		~FrameArena() { delete[] m_base; }
		FrameArena(const FrameArena&) = delete;
		FrameArena& operator=(const FrameArena&) = delete;

		void* alloc(std::size_t size, std::size_t align = alignof(std::max_align_t)) {
			assert(align != 0 && (align & (align - 1)) == 0 && align <= alignof(std::max_align_t));
			std::size_t p = (m_top + align - 1) & ~(align - 1);
			if (p + size > m_cap) { ++m_overflows; return nullptr; }
			m_top = p + size;
			return m_base + p;
		}
		/// Uninitialised array of trivially-destructible T.
		template <class T> T* allocArray(std::size_t n) { return static_cast<T*>(alloc(sizeof(T) * n, alignof(T))); }
		void reset() { m_top = 0; }
		std::size_t used() const { return m_top; }
		std::size_t capacity() const { return m_cap; }
		std::size_t overflows() const { return m_overflows; }
	private:
		std::byte* m_base; std::size_t m_cap, m_top = 0, m_overflows = 0;
	};
}
