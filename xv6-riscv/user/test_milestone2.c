#define TEST_MILESTONE2_MAIN
#include "kernel/types.h"
#include "user/user.h"
#include "xv6_stdlib.h"
#include "test_milestone2.h"

// Include test functions from test_stdlib.c
// (test_stdlib.c will provide the implementations)
extern void test_calloc(struct test_result *);
extern void test_qsort(struct test_result *);
extern void test_bsearch(struct test_result *);
extern void test_atoi(struct test_result *);
extern void test_atof(struct test_result *);

// Master test runner for Milestone 2
int
main(int argc, char *argv[])
{
  struct test_result total_tr;
  init_test_result(&total_tr);

  printf("\n");
  printf("========================================\n");
  printf("  Milestone 2 Test Suite\n");
  printf("========================================\n\n");

  // Run stdlib tests
  printf("Running Standard Library Tests...\n");
  struct test_result stdlib_tr;
  init_test_result(&stdlib_tr);

  test_calloc(&stdlib_tr);
  test_qsort(&stdlib_tr);
  test_bsearch(&stdlib_tr);
  test_atoi(&stdlib_tr);
  test_atof(&stdlib_tr);

  print_test_summary("Standard Library", &stdlib_tr);

  // Aggregate results
  total_tr.passed += stdlib_tr.passed;
  total_tr.failed += stdlib_tr.failed;
  total_tr.total += stdlib_tr.total;

  // Print final summary
  printf("\n");
  printf("========================================\n");
  printf("  Final Test Summary\n");
  printf("========================================\n");
  printf("Total Tests: %d\n", total_tr.total);
  printf("Passed: %d\n", total_tr.passed);
  printf("Failed: %d\n", total_tr.failed);

  if (total_tr.failed == 0) {
    printf("\n*** ALL TESTS PASSED ***\n");
    exit(0);
  } else {
    printf("\n*** SOME TESTS FAILED ***\n");
    exit(1);
  }
}

