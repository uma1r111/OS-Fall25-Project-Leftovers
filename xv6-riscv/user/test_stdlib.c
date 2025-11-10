#include "kernel/types.h"
#include "user/user.h"
#include "xv6_stdlib.h"
#include "test_milestone2.h"

// Test calloc
void test_calloc(struct test_result *tr)
{
  printf("\n=== Testing calloc ===\n");

  // Test 1: Normal allocation and zero initialization
  int *arr = (int *)calloc(10, sizeof(int));
  if (arr == 0) {
    test_fail(tr, "calloc normal allocation", "allocation failed");
    return;
  }

  int all_zero = 1;
  for (int i = 0; i < 10; i++) {
    if (arr[i] != 0) {
      all_zero = 0;
      break;
    }
  }

  if (all_zero)
    test_pass(tr, "calloc zero initialization");
  else
    test_fail(tr, "calloc zero initialization", "memory not zeroed");

  free(arr);

  // Test 2: Zero size allocation
  void *ptr = calloc(0, sizeof(int));
  if (ptr == 0)
    test_pass(tr, "calloc zero size returns NULL");
  else {
    test_fail(tr, "calloc zero size", "should return NULL");
    free(ptr);
  }

  // Test 3: Large allocation
  char *large = (char *)calloc(100, sizeof(char));
  if (large == 0) {
    test_fail(tr, "calloc large allocation", "allocation failed");
    return;
  }

  int all_zero_large = 1;
  for (int i = 0; i < 100; i++) {
    if (large[i] != 0) {
      all_zero_large = 0;
      break;
    }
  }

  if (all_zero_large)
    test_pass(tr, "calloc large allocation");
  else
    test_fail(tr, "calloc large allocation", "memory not zeroed");

  free(large);
}

int int_compare(const void *a, const void *b)
{
  if (a == 0 || b == 0)
    return 0;
    
  int ia = *(const int *)a;
  int ib = *(const int *)b;
  
  if (ia < ib) return -1;
  if (ia > ib) return 1;
  return 0;
}

// Float comparison function for qsort
int float_compare(const void *a, const void *b)
{
  float fa = *(const float *)a;
  float fb = *(const float *)b;
  if (fa < fb) return -1;
  if (fa > fb) return 1;
  return 0;
}

// Test qsort
void test_qsort(struct test_result *tr)
{
  printf("\n=== Testing qsort ===\n");

  // Test 1: Sort integers
  int arr1[] = {5, 2, 8, 1, 9, 3};
  int expected1[] = {1, 2, 3, 5, 8, 9};
  qsort(arr1, 6, sizeof(int), int_compare);

  int sorted = 1;
  for (int i = 0; i < 6; i++) {
    if (arr1[i] != expected1[i]) {
      sorted = 0;
      break;
    }
  }

  if (sorted)
    test_pass(tr, "qsort integers");
  else
    test_fail(tr, "qsort integers", "array not sorted correctly");

  // Test 2: Sort floats with custom comparator
  float arr2[] = {3.5f, 1.2f, 4.8f, 2.1f};
  float expected2[] = {1.2f, 2.1f, 3.5f, 4.8f};
  qsort(arr2, 4, sizeof(float), float_compare);

  int sorted_float = 1;
  for (int i = 0; i < 4; i++) {
    if (!float_eq(arr2[i], expected2[i], 0.001f)) {
      sorted_float = 0;
      break;
    }
  }

  if (sorted_float)
    test_pass(tr, "qsort floats");
  else
    test_fail(tr, "qsort floats", "array not sorted correctly");

  // Test 3: Already sorted array
  int arr3[] = {1, 2, 3, 4, 5};
  int arr3_copy[] = {1, 2, 3, 4, 5};
  qsort(arr3, 5, sizeof(int), int_compare);

  int unchanged = 1;
  for (int i = 0; i < 5; i++) {
    if (arr3[i] != arr3_copy[i]) {
      unchanged = 0;
      break;
    }
  }

  if (unchanged)
    test_pass(tr, "qsort already sorted");
  else
    test_fail(tr, "qsort already sorted", "array modified incorrectly");

  // Test 4: Reverse sorted array
  int arr4[] = {5, 4, 3, 2, 1};
  int expected4[] = {1, 2, 3, 4, 5};
  qsort(arr4, 5, sizeof(int), int_compare);

  int reversed_sorted = 1;
  for (int i = 0; i < 5; i++) {
    if (arr4[i] != expected4[i]) {
      reversed_sorted = 0;
      break;
    }
  }

  if (reversed_sorted)
    test_pass(tr, "qsort reverse sorted");
  else
    test_fail(tr, "qsort reverse sorted", "array not sorted correctly");

  // Test 5: Single element
  int arr5[] = {42};
  qsort(arr5, 1, sizeof(int), int_compare);

  if (arr5[0] == 42)
    test_pass(tr, "qsort single element");
  else
    test_fail(tr, "qsort single element", "element changed");
}

// Test bsearch
void test_bsearch(struct test_result *tr)
{
  printf("\n=== Testing bsearch ===\n");

  // Test 1: Find existing element
  int arr[] = {1, 3, 5, 7, 9, 11, 13};
  int key = 7;
  int *found = (int *)bsearch(&key, arr, 7, sizeof(int), int_compare);

  if (found != 0 && *found == 7)
    test_pass(tr, "bsearch find existing");
  else
    test_fail(tr, "bsearch find existing", "element not found");

  // Test 2: Search for non-existent element
  int key2 = 6;
  int *found2 = (int *)bsearch(&key2, arr, 7, sizeof(int), int_compare);

  if (found2 == 0)
    test_pass(tr, "bsearch non-existent");
  else
    test_fail(tr, "bsearch non-existent", "should return NULL");

  // Test 3: Single element array
  int arr3[] = {42};
  int key3 = 42;
  int *found3 = (int *)bsearch(&key3, arr3, 1, sizeof(int), int_compare);

  if (found3 != 0 && *found3 == 42)
    test_pass(tr, "bsearch single element");
  else
    test_fail(tr, "bsearch single element", "element not found");

  // Test 4: Empty array
  int arr4[] = {1};
  int key4 = 5;
  int *found4 = (int *)bsearch(&key4, arr4, 0, sizeof(int), int_compare);

  if (found4 == 0)
    test_pass(tr, "bsearch empty array");
  else
    test_fail(tr, "bsearch empty array", "should return NULL");
}

// Test atoi
void test_atoi(struct test_result *tr)
{
  printf("\n=== Testing atoi ===\n");

  // Test 1: Positive number
  int result1 = atoi("123");
  if (result1 == 123)
    test_pass(tr, "atoi positive number");
  else
    test_fail(tr, "atoi positive number", "incorrect result");

  // Test 2: Negative number
  int result2 = atoi("-456");
  if (result2 == -456)
    test_pass(tr, "atoi negative number");
  else
    test_fail(tr, "atoi negative number", "incorrect result");

  // Test 3: Leading whitespace
  int result3 = atoi("   789");
  if (result3 == 789)
    test_pass(tr, "atoi leading whitespace");
  else
    test_fail(tr, "atoi leading whitespace", "whitespace not skipped");

  // Test 4: Invalid input
  int result4 = atoi("abc");
  if (result4 == 0)
    test_pass(tr, "atoi invalid input");
  else
    test_fail(tr, "atoi invalid input", "should return 0");

  // Test 5: Number with sign
  int result5 = atoi("+42");
  if (result5 == 42)
    test_pass(tr, "atoi with plus sign");
  else
    test_fail(tr, "atoi with plus sign", "incorrect result");
}

// Test atof
void test_atof(struct test_result *tr)
{
  printf("\n=== Testing atof ===\n");

  // Test 1: Integer as float
  float result1 = atof("42");
  if (float_eq(result1, 42.0f, 0.001f))
    test_pass(tr, "atof integer");
  else
    test_fail(tr, "atof integer", "incorrect result");

  // Test 2: Fractional number
  float result2 = atof("3.14");
  if (float_eq(result2, 3.14f, 0.001f))
    test_pass(tr, "atof fractional");
  else
    test_fail(tr, "atof fractional", "incorrect result");

  // Test 3: Scientific notation
  float result3 = atof("1.5e-3");
  if (float_eq(result3, 0.0015f, 0.0001f))
    test_pass(tr, "atof scientific notation");
  else
    test_fail(tr, "atof scientific notation", "incorrect result");

  // Test 4: Negative number
  float result4 = atof("-2.5");
  if (float_eq(result4, -2.5f, 0.001f))
    test_pass(tr, "atof negative");
  else
    test_fail(tr, "atof negative", "incorrect result");

  // Test 5: Leading whitespace
  float result5 = atof("   1.23");
  if (float_eq(result5, 1.23f, 0.001f))
    test_pass(tr, "atof leading whitespace");
  else
    test_fail(tr, "atof leading whitespace", "whitespace not skipped");
}

// Main test function for stdlib (standalone)
int
test_stdlib_main(int argc, char *argv[])
{
  struct test_result tr;
  init_test_result(&tr);

  printf("=== Standard Library Functions Test Suite ===\n");

  test_calloc(&tr);
  test_qsort(&tr);
  test_bsearch(&tr);
  test_atoi(&tr);
  test_atof(&tr);

  print_test_summary("Standard Library Tests", &tr);

  return tr.failed == 0 ? 0 : 1;
}

// Standalone main for test_stdlib program
// Only compile this if test_milestone2 is not being built
#ifndef TEST_MILESTONE2_MAIN
int
main(int argc, char *argv[])
{
  exit(test_stdlib_main(argc, argv));
}
#endif

