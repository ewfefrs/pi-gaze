// Minimal self-registering test harness (no downloads needed on the PC or the Pi).
#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testing {

struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
struct Registrar { Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };

inline void fail(const char* file, int line, const std::string& msg) {
    std::printf("    FAIL %s:%d  %s\n", file, line, msg.c_str());
    ++failures();
}

}  // namespace testing

#define TEST_CAT2(a, b) a##b
#define TEST_CAT(a, b) TEST_CAT2(a, b)
#define TEST(name)                                                                      \
    static void TEST_CAT(test_fn_, __LINE__)();                                         \
    static testing::Registrar TEST_CAT(test_reg_, __LINE__)(name, TEST_CAT(test_fn_, __LINE__)); \
    static void TEST_CAT(test_fn_, __LINE__)()

#define CHECK(cond) \
    do { if (!(cond)) testing::fail(__FILE__, __LINE__, #cond); } while (0)
#define CHECK_NEAR(a, b, tol)                                                           \
    do {                                                                                \
        double va_ = (a), vb_ = (b);                                                    \
        if (!(std::fabs(va_ - vb_) <= (tol))) {                                         \
            char buf_[256];                                                             \
            std::snprintf(buf_, sizeof buf_, "%s = %.4f, expected %.4f +- %.4f", #a, va_, vb_, (double)(tol)); \
            testing::fail(__FILE__, __LINE__, buf_);                                    \
        }                                                                               \
    } while (0)
#define REQUIRE(cond) \
    do { if (!(cond)) { testing::fail(__FILE__, __LINE__, #cond); return; } } while (0)
