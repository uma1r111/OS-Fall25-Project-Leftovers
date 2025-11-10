// Shared test utilities for Milestone 2 tests

#ifndef TEST_MILESTONE2_H
#define TEST_MILESTONE2_H

#include "kernel/types.h"
#include "user/user.h"

// Test result tracking
struct test_result {
  int passed;
  int failed;
  int total;
};

// Initialize test result structure
static inline void
init_test_result(struct test_result *tr)
{
  tr->passed = 0;
  tr->failed = 0;
  tr->total = 0;
}

// Record a test pass
static inline void
test_pass(struct test_result *tr, const char *test_name)
{
  tr->passed++;
  tr->total++;
  printf("  PASS: %s\n", test_name);
}

// Record a test failure
static inline void
test_fail(struct test_result *tr, const char *test_name, const char *reason)
{
  tr->failed++;
  tr->total++;
  printf("  FAIL: %s - %s\n", test_name, reason);
}

// Print test summary
static inline void
print_test_summary(const char *suite_name, struct test_result *tr)
{
  printf("\n=== %s Summary ===\n", suite_name);
  printf("Total: %d, Passed: %d, Failed: %d\n", 
         tr->total, tr->passed, tr->failed);
  if (tr->failed == 0)
    printf("ALL TESTS PASSED\n");
  else
    printf("SOME TESTS FAILED\n");
}

// Float comparison with epsilon tolerance
static inline int
float_eq(float a, float b, float epsilon)
{
  float diff = a - b;
  if (diff < 0)
    diff = -diff;
  return diff < epsilon;
}

#endif // TEST_MILESTONE2_H

