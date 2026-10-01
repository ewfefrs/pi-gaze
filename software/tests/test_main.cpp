#include <chrono>
#include <cstring>

#include "testing.hpp"

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    for (const auto& c : testing::registry()) {
        if (filter && !std::strstr(c.name, filter)) continue;
        const int before = testing::failures();
        const auto t0 = std::chrono::steady_clock::now();
        c.fn();
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::printf("%s %-60s %7.1f ms\n", testing::failures() == before ? "  ok  " : "  FAIL", c.name, ms);
        ++run;
    }
    std::printf("\n%d tests, %d failed checks\n", run, testing::failures());
    return testing::failures() == 0 && run > 0 ? 0 : 1;
}
