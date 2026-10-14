#include "xtest.hpp"
#include <cstdlib>
#include <new>
// Allocation counter so tests can assert "zero heap allocations" on steady-state paths.
// All new/delete overloads are replaced consistently (mixing replaced and default ones breaks sanitizers).
namespace xtest { std::size_t allocCount = 0; }
static void* xalloc(std::size_t n) noexcept { ++xtest::allocCount; return std::malloc(n ? n : 1); }
void* operator new(std::size_t n) { if (void* p = xalloc(n)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { if (void* p = xalloc(n)) return p; throw std::bad_alloc(); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return xalloc(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return xalloc(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // line-by-line output: a crash cannot hide its victim
    return xtest::runAll();
}
