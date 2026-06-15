#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    int passed;
    int failed;
    int assertions;
} TestSuite;

static TestSuite current_suite = {0};
static int total_tests = 0;
static int total_passed = 0;
static int total_failed = 0;
static int total_assertions = 0;

#define TEST_SUITE_START(suite_name) \
    do { \
        current_suite.name = suite_name; \
        current_suite.passed = 0; \
        current_suite.failed = 0; \
        current_suite.assertions = 0; \
        printf("\n========== %s ==========\n", suite_name); \
    } while(0)

#define TEST_SUITE_END() \
    do { \
        printf("Suite: %s\n", current_suite.name); \
        printf("  Passed: %d, Failed: %d, Assertions: %d\n", \
               current_suite.passed, current_suite.failed, current_suite.assertions); \
        total_tests += (current_suite.passed + current_suite.failed); \
        total_passed += current_suite.passed; \
        total_failed += current_suite.failed; \
        total_assertions += current_suite.assertions; \
    } while(0)

#define TEST(test_name) \
    do { \
        printf("  [TEST] %s: ", test_name);

#define TEST_END() \
    } while(0)

#define ASSERT_TRUE(condition) \
    do { \
        current_suite.assertions++; \
        if (!(condition)) { \
            printf("FAIL\n"); \
            current_suite.failed++; \
        } else { \
            printf("PASS\n"); \
            current_suite.passed++; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQUAL(a, b) \
    do { \
        current_suite.assertions++; \
        if ((a) != (b)) { \
            printf("FAIL (expected %d, got %d)\n", (int)(b), (int)(a)); \
            current_suite.failed++; \
        } else { \
            printf("PASS\n"); \
            current_suite.passed++; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        current_suite.assertions++; \
        if ((ptr) == NULL) { \
            printf("FAIL (expected non-NULL pointer)\n"); \
            current_suite.failed++; \
        } else { \
            printf("PASS\n"); \
            current_suite.passed++; \
        } \
    } while(0)

#define ASSERT_NULL(ptr) \
    do { \
        current_suite.assertions++; \
        if ((ptr) != NULL) { \
            printf("FAIL (expected NULL pointer)\n"); \
            current_suite.failed++; \
        } else { \
            printf("PASS\n"); \
            current_suite.passed++; \
        } \
    } while(0)

#define ASSERT_STRING_EQUAL(a, b) \
    do { \
        current_suite.assertions++; \
        if (strcmp((a), (b)) != 0) { \
            printf("FAIL (expected '%s', got '%s')\n", (b), (a)); \
            current_suite.failed++; \
        } else { \
            printf("PASS\n"); \
            current_suite.passed++; \
        } \
    } while(0)

#define PRINT_SUMMARY() \
    do { \
        printf("\n\n"); \
        printf("=====================================\n"); \
        printf("Test Summary:\n"); \
        printf("=====================================\n"); \
        printf("Total Tests:   %d\n", total_tests); \
        printf("Tests Passed:  %d\n", total_passed); \
        printf("Tests Failed:  %d\n", total_failed); \
        printf("Assertions:    %d\n", total_assertions); \
        printf("Success Rate:  %.1f%%\n", total_tests > 0 ? (100.0 * total_passed / total_tests) : 0); \
    } while(0)

#endif // TEST_FRAMEWORK_H
