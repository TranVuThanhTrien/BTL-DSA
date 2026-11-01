// Khung kiểm thử tối giản, không cần cài thư viện. Dùng chung cho mọi tệp test.
#pragma once
#include <iostream>

static int g_failed = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cout << "  FAIL  " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
            ++g_failed;                                                      \
        }                                                                    \
    } while (0)

#define RUN(fn)                                                              \
    do {                                                                     \
        int before = g_failed;                                               \
        try { fn(); } catch (const std::exception& e) {                      \
            std::cout << "  FAIL  " #fn " nem loi: " << e.what() << "\n"; ++g_failed; } \
        std::cout << (g_failed == before ? "  ok    " : "  loi   ") << #fn << "\n"; \
    } while (0)

#define TEST_MAIN_END() return g_failed == 0 ? 0 : 1


