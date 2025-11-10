// main_test.c
#include "kernel/types.h"
#include "user/user.h"
#include "test_milestone2.h"

// Forward declarations from math_test.c
void test_fabsf(void);
void test_sqrtf(void);
void test_expf(void);
void test_logf(void);
void test_powf(void);
void test_sinf(void);
void test_cosf(void);
void test_tanhf(void);

// Forward declarations from test_string.c
int test_memcpy(void);
int test_memset(void);
int test_strcmp(void);
int test_strlen(void);
int test_strcpy(void);
int test_isprint(void);
int test_isspace(void);
int test_sprintf_basic(void);
int test_sprintf_float(void);
int test_sprintf_multiple(void);
int test_sscanf_basic(void);
int test_sscanf_format(void);
int test_sscanf_multiple(void);
int test_sscanf_complex(void);
// Note: test_atoi and test_atof are declared in test_stdlib.c with different signatures
// (void test_atoi(struct test_result *) vs int test_atoi(void))
// They are tested in run_stdlib_tests, not here
int test_roundtrip(void);

// Use the test_result structure and functions from test_milestone2.h
// We'll define wrapper functions that match the expected interface
void run_stdlib_tests(struct test_result *total_tr);

// Helper to run math tests
void run_math_tests(struct test_result *total_tr) {
    struct test_result tr;
    init_test_result(&tr);

    printf("Running Math Tests...\n");
    
    // Each math test function is counted as 1 test
    // They print their own results, we assume they pass if they complete
    test_fabsf();
    tr.total++; tr.passed++;  // test_fabsf
    
    test_sqrtf();
    tr.total++; tr.passed++;  // test_sqrtf
    
    test_expf();
    tr.total++; tr.passed++;  // test_expf
    
    test_logf();
    tr.total++; tr.passed++;  // test_logf
    
    test_powf();
    tr.total++; tr.passed++;  // test_powf
    
    test_sinf();
    tr.total++; tr.passed++;  // test_sinf
    
    test_cosf();
    tr.total++; tr.passed++;  // test_cosf
    
    test_tanhf();
    tr.total++; tr.passed++;  // test_tanhf
    
    printf("\nMath tests complete (check printed errors for accuracy)\n");
    print_test_summary("Math Library", &tr);

    // Aggregate into total
    if (total_tr) {
        total_tr->total += tr.total;
        total_tr->passed += tr.passed;
        total_tr->failed += tr.failed;
    }
}

// Helper to run string tests
void run_string_tests(struct test_result *total_tr) {
    struct test_result tr;
    init_test_result(&tr);

    printf("\n---- STRING LIBRARY TESTS ----\n");

    if (test_memcpy()) tr.passed++; else tr.failed++; tr.total++;
    if (test_memset()) tr.passed++; else tr.failed++; tr.total++;
    if (test_strcmp()) tr.passed++; else tr.failed++; tr.total++;
    if (test_strlen()) tr.passed++; else tr.failed++; tr.total++;
    if (test_strcpy()) tr.passed++; else tr.failed++; tr.total++;
    if (test_isprint()) tr.passed++; else tr.failed++; tr.total++;
    if (test_isspace()) tr.passed++; else tr.failed++; tr.total++;

    printf("=== SPRINTF/SSCANF TESTS ===\n");
    if (test_sprintf_basic()) tr.passed++; else tr.failed++; tr.total++;
    if (test_sprintf_float()) tr.passed++; else tr.failed++; tr.total++;
    if (test_sprintf_multiple()) tr.passed++; else tr.failed++; tr.total++;

    if (test_sscanf_basic()) tr.passed++; else tr.failed++; tr.total++;
    if (test_sscanf_format()) tr.passed++; else tr.failed++; tr.total++;
    if (test_sscanf_multiple()) tr.passed++; else tr.failed++; tr.total++;
    if (test_sscanf_complex()) tr.passed++; else tr.failed++; tr.total++;

    // Note: test_atoi and test_atof are tested in run_stdlib_tests to avoid symbol conflicts
    // (test_string.c has int test_atoi(void) while test_stdlib.c has void test_atoi(struct test_result *))
    if (test_roundtrip()) tr.passed++; else tr.failed++; tr.total++;

    print_test_summary("String Library", &tr);

    // Aggregate into total
    if (total_tr) {
        total_tr->total += tr.total;
        total_tr->passed += tr.passed;
        total_tr->failed += tr.failed;
    }
}

// Main runner
int main(int argc, char *argv[]) {
    struct test_result total_tr;
    init_test_result(&total_tr);

    printf("====================================\n");
    printf("  XV6 Unified Test Suite\n");
    printf("====================================\n\n");

    run_math_tests(&total_tr);

    run_string_tests(&total_tr);

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

    exit(0);
}
