// Long-running differential layout fuzz: incremental layout must equal a from-scratch layout.
// usage: fuzz_layout [firstSeed] [count] [stepsPerCase]
#include "../tests/layout_fuzz.hpp"
int main(int argc, char** argv) {
	unsigned long long first = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1, count = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 5000;
	int steps = argc > 3 ? std::atoi(argv[3]) : 10, failures = 0;
	for (unsigned long long s = first; s < first + count && failures < 5; ++s) {
		std::string e = layoutfuzz::runCase(s, steps);
		if (!e.empty()) { std::printf("seed %llu: %s\n", s, e.c_str()); ++failures; }
	}
	std::printf("seeds %llu..%llu x %d steps: %s\n", first, first + count - 1, steps, failures ? "MISMATCHES FOUND" : "incremental == full everywhere");
	return failures ? 1 : 0;
}
