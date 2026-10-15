#pragma once
// Minimal test harness: TEST(name){...}, CHECK(expr), CHECK_EQ(a,b). Registry is test-only global state.
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
namespace xtest {
	struct Case { const char* name; void (*fn)(); };
	inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
	inline int& failures() { static int f = 0; return f; }
	struct Reg { Reg(const char* n, void (*f)()) { registry().push_back({ n, f }); } };
	inline int runAll() {
		int failedCases = 0;
		for (const Case& c : registry()) {
			int before = failures();
			c.fn();
			bool ok = failures() == before;
			std::printf("[%s] %s\n", ok ? " OK " : "FAIL", c.name);
			std::fflush(stdout);   // keep per-test progress visible when a case crashes the process
			if (!ok) ++failedCases;
		}
		std::printf("\n%zu tests, %d failed\n", registry().size(), failedCases);
		return failedCases ? 1 : 0;
	}
}
#define TEST(name) static void name(); static xtest::Reg reg_##name(#name, name); static void name()
#define CHECK(e) do { if (!(e)) { ++xtest::failures(); std::printf("  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #e); } } while (0)
#define CHECK_EQ(a, b) do { auto va = (a); auto vb = (b); if (!(va == vb)) { ++xtest::failures(); std::printf("  %s:%d: CHECK_EQ(%s, %s) failed\n", __FILE__, __LINE__, #a, #b); } } while (0)
#define CHECK_NEAR(a, b, eps) do { double va = (a), vb = (b); if (!(va - vb < (eps) && vb - va < (eps))) { ++xtest::failures(); std::printf("  %s:%d: CHECK_NEAR(%s, %s) failed: %g vs %g\n", __FILE__, __LINE__, #a, #b, va, vb); } } while (0)
