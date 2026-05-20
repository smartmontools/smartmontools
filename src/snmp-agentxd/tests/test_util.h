// test_util.h — minimal test harness (no external dependencies)

#pragma once
#include <cstdio>
#include <cstring>
#include <string>

static int s_pass = 0, s_fail = 0;

#define CHECK(expr) \
    do { \
        if (expr) { \
            ++s_pass; \
        } else { \
            ++s_fail; \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        } \
    } while (0)

#define CHECK_EQ(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (_a == _b) { \
            ++s_pass; \
        } else { \
            ++s_fail; \
            fprintf(stderr, "FAIL %s:%d: %s == %s  (lhs=%lld, rhs=%lld)\n", \
                    __FILE__, __LINE__, #a, #b, \
                    (long long)_a, (long long)_b); \
        } \
    } while (0)

#define CHECK_STR(a, b) \
    do { \
        std::string _a = (a); std::string _b = (b); \
        if (_a == _b) { \
            ++s_pass; \
        } else { \
            ++s_fail; \
            fprintf(stderr, "FAIL %s:%d: \"%s\" == \"%s\"  (got \"%s\")\n", \
                    __FILE__, __LINE__, #a, #b, _a.c_str()); \
        } \
    } while (0)

#define SECTION(name) fprintf(stdout, "\n  [%s]\n", name)

static int test_summary() {
    fprintf(stdout, "\n%d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
