#pragma once

// A deliberately tiny test framework so the tests build anywhere with just a
// C++17 compiler (no downloads, no openFrameworks).
//
//   TEST(name) { CHECK(a == b); CHECK_NEAR(x, 0.5f, 1e-6f); }

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
	const char * name;
	std::function<void()> body;
};

inline std::vector<TestCase> & registry() {
	static std::vector<TestCase> tests;
	return tests;
}

inline int & failureCount() {
	static int failures = 0;
	return failures;
}

struct Registrar {
	Registrar(const char * name, std::function<void()> body) {
		registry().push_back({ name, std::move(body) });
	}
};

inline void fail(const char * file, int line, const std::string & message) {
	std::printf("  FAIL %s:%d  %s\n", file, line, message.c_str());
	++failureCount();
}

inline int runAll() {
	int failedTests = 0;
	for (const TestCase & test : registry()) {
		int before = failureCount();
		test.body();
		bool ok = failureCount() == before;
		std::printf("%s %s\n", ok ? "[ ok ]" : "[FAIL]", test.name);
		if (!ok) ++failedTests;
	}
	std::printf("\n%zu tests, %d failed\n", registry().size(), failedTests);
	return failedTests == 0 ? 0 : 1;
}

} // namespace testing

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)
#define TEST(name)                                                                   \
	static void TEST_CONCAT(test_, name)();                                          \
	static testing::Registrar TEST_CONCAT(registrar_, name)(#name, TEST_CONCAT(test_, name)); \
	static void TEST_CONCAT(test_, name)()

#define CHECK(condition)                                                 \
	do {                                                                 \
		if (!(condition)) testing::fail(__FILE__, __LINE__, #condition); \
	} while (0)

#define CHECK_NEAR(actual, expected, tolerance)                                          \
	do {                                                                                 \
		double a_ = (actual), e_ = (expected);                                           \
		if (std::fabs(a_ - e_) > (tolerance))                                            \
			testing::fail(__FILE__, __LINE__, std::string(#actual) + " = " + std::to_string(a_) \
				+ ", expected " + std::to_string(e_));                                   \
	} while (0)
