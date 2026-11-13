// Long-running check of the property that makes draw-call batching legal: reordering never changes a pixel.
// usage: fuzz_paint [firstSeed] [count] [primsPerScene]
#include "../tests/paint_fuzz.hpp"
int main(int argc, char** argv) {
	const unsigned long long first = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1, count = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 2000;
	const int prims = argc > 3 ? std::atoi(argv[3]) : 70;
	paintfuzz::Env e(200, 150); int failures = 0; unsigned long long on = 0, off = 0;
	for (unsigned long long s = first; s < first + count && failures < 5; ++s) {
		xui::u32 a = 0, b = 0; const std::string err = paintfuzz::reorderCase(e, s, prims, &a, &b);
		if (!err.empty()) { std::printf("seed %llu: %s\n", s, err.c_str()); ++failures; }
		on += a; off += b;
	}
	std::printf("seeds %llu..%llu x %d prims: %s   (draw calls %llu reordered vs %llu naive)\n", first, first + count - 1, prims, failures ? "MISMATCHES FOUND" : "reordered == naive pixel-for-pixel", on, off);
	return failures ? 1 : 0;
}
