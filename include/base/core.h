#include "spdlog/spdlog.h"

#include <chrono>

#define internal static

#define PERF(A, ...)                                                                          \
	static auto start_##A = std::chrono::high_resolution_clock::now();                        \
	__VA_ARGS__;                                                                              \
	static auto							 end_##A = std::chrono::high_resolution_clock::now(); \
	static std::chrono::duration<double> duration_##A = end_##A - start_##A;                  \
	SPDLOG_INFO("PERF({}): {:.6f} seconds", #A, duration_##A.count())