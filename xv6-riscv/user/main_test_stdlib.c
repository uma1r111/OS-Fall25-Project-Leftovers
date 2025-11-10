// main_test_stdlib.c
// Simple test runner that only calls test_stdlib

#include "kernel/types.h"
#include "user/user.h"
#include "test_milestone2.h"

// run_stdlib_tests is declared in test_milestone2.h

// Main runner
int
main(int argc, char *argv[])
{
  struct test_result total_tr;
  init_test_result(&total_tr);

  printf("====================================\n");
  printf("  XV6 Standard Library Test Suite\n");
  printf("====================================\n\n");

  run_stdlib_tests(&total_tr);

  printf("\n====================================\n");
  printf("  FINAL TEST SUMMARY\n");
  printf("====================================\n");
  printf("Total Tests: %d\n", total_tr.total);
  printf("Passed: %d\n", total_tr.passed);
  printf("Failed: %d\n", total_tr.failed);

  if (total_tr.failed == 0)
    printf("\n*** ALL TESTS PASSED ***\n");
  else
    printf("\n*** SOME TESTS FAILED ***\n");

  exit(total_tr.failed == 0 ? 0 : 1);
}

