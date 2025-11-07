// mathtest.c
// Test program for xv6 math library
// Verifies accuracy of all mathematical functions

#include "kernel/types.h"
#include "user/user.h"
#include "user/xv6_maths.h"

// Helper function to print float (since printf in xv6 doesn't support %f)
void print_float(const char* label, float value) {
    int integer_part = (int)value;
    float frac = value - (float)integer_part;
    if (frac < 0) frac = -frac;
    int frac_part = (int)(frac * 1000000);  // 6 decimal places
    
    printf("%s: %d.%d\n", label, integer_part, frac_part);
}

// Test fabsf
void test_fabsf(void) {
    printf("\n=== Testing xv6_fabsf ===\n");
    
    float test_vals[] = {5.5f, -5.5f, 0.0f, -123.456f, 999.999f};
    const char* labels[] = {"fabsf(5.5)", "fabsf(-5.5)", "fabsf(0)", 
                            "fabsf(-123.456)", "fabsf(999.999)"};
    
    for (int i = 0; i < 5; i++) {
        float result = xv6_fabsf(test_vals[i]);
        print_float(labels[i], result);
    }
}

// Test sqrtf
void test_sqrtf(void) {
    printf("\n=== Testing xv6_sqrtf ===\n");
    
    float test_vals[] = {0.0f, 1.0f, 4.0f, 9.0f, 16.0f, 25.0f, 2.0f, 10.0f};
    const char* labels[] = {"sqrt(0)", "sqrt(1)", "sqrt(4)", "sqrt(9)", 
                            "sqrt(16)", "sqrt(25)", "sqrt(2)", "sqrt(10)"};
    
    for (int i = 0; i < 8; i++) {
        float result = xv6_sqrtf(test_vals[i]);
        print_float(labels[i], result);
    }
    
    // Test negative input (should return NaN)
    float neg_result = xv6_sqrtf(-1.0f);
    if (xv6_isnan(neg_result)) {
        printf("sqrt(-1): NaN (correct)\n");
    } else {
        printf("sqrt(-1): ERROR - should be NaN\n");
    }
}

// Test expf
void test_expf(void) {
    printf("\n=== Testing xv6_expf ===\n");
    
    float test_vals[] = {0.0f, 1.0f, 2.0f, -1.0f, 0.5f, 5.0f};
    const char* labels[] = {"exp(0)", "exp(1)", "exp(2)", 
                            "exp(-1)", "exp(0.5)", "exp(5)"};
    float expected[] = {1.0f, 2.71828f, 7.38906f, 0.36788f, 1.64872f, 148.413f};
    
    for (int i = 0; i < 6; i++) {
        float result = xv6_expf(test_vals[i]);
        print_float(labels[i], result);
        
        // Calculate relative error
        float error = xv6_fabsf(result - expected[i]) / expected[i];
        int error_pct = (int)(error * 100000);  // Error in 1/1000 percent
        printf("  Error: 0.%d%%\n", error_pct);
    }
}

// Test logf
void test_logf(void) {
    printf("\n=== Testing xv6_logf ===\n");
    
    float test_vals[] = {1.0f, 2.71828f, 2.0f, 10.0f, 0.5f};
    const char* labels[] = {"log(1)", "log(e)", "log(2)", "log(10)", "log(0.5)"};
    float expected[] = {0.0f, 1.0f, 0.69315f, 2.30259f, -0.69315f};
    
    for (int i = 0; i < 5; i++) {
        float result = xv6_logf(test_vals[i]);
        print_float(labels[i], result);
        
        // Calculate absolute error
        float error = xv6_fabsf(result - expected[i]);
        int error_val = (int)(error * 100000);
        printf("  Error: 0.%d\n", error_val);
    }
}

// Test powf
void test_powf(void) {
    printf("\n=== Testing xv6_powf ===\n");
    
    struct {
        float base;
        float exp;
        const char* label;
        float expected;
    } tests[] = {
        {2.0f, 3.0f, "pow(2, 3)", 8.0f},
        {3.0f, 2.0f, "pow(3, 2)", 9.0f},
        {10.0f, 2.0f, "pow(10, 2)", 100.0f},
        {2.0f, 0.5f, "pow(2, 0.5)", 1.41421f},
        {0.5f, 2.0f, "pow(0.5, 2)", 0.25f},
        {1.0f, 100.0f, "pow(1, 100)", 1.0f},
    };
    
    for (int i = 0; i < 6; i++) {
        float result = xv6_powf(tests[i].base, tests[i].exp);
        print_float(tests[i].label, result);
        
        float error = xv6_fabsf(result - tests[i].expected);
        int error_val = (int)(error * 100000);
        printf("  Error: 0.%d\n", error_val);
    }
}

// Test sinf
void test_sinf(void) {
    printf("\n=== Testing xv6_sinf ===\n");
    
    float test_vals[] = {0.0f, XV6_PI / 6.0f, XV6_PI / 4.0f, 
                         XV6_PI / 2.0f, XV6_PI, 2.0f * XV6_PI};
    const char* labels[] = {"sin(0)", "sin(π/6)", "sin(π/4)", 
                            "sin(π/2)", "sin(π)", "sin(2π)"};
    float expected[] = {0.0f, 0.5f, 0.70711f, 1.0f, 0.0f, 0.0f};
    
    for (int i = 0; i < 6; i++) {
        float result = xv6_sinf(test_vals[i]);
        print_float(labels[i], result);
        
        float error = xv6_fabsf(result - expected[i]);
        int error_val = (int)(error * 100000);
        printf("  Error: 0.%d\n", error_val);
    }
}

// Test cosf
void test_cosf(void) {
    printf("\n=== Testing xv6_cosf ===\n");
    
    float test_vals[] = {0.0f, XV6_PI / 6.0f, XV6_PI / 4.0f, 
                         XV6_PI / 2.0f, XV6_PI};
    const char* labels[] = {"cos(0)", "cos(π/6)", "cos(π/4)", 
                            "cos(π/2)", "cos(π)"};
    float expected[] = {1.0f, 0.86603f, 0.70711f, 0.0f, -1.0f};
    
    for (int i = 0; i < 5; i++) {
        float result = xv6_cosf(test_vals[i]);
        print_float(labels[i], result);
        
        float error = xv6_fabsf(result - expected[i]);
        int error_val = (int)(error * 100000);
        printf("  Error: 0.%d\n", error_val);
    }
}

// Test tanhf
void test_tanhf(void) {
    printf("\n=== Testing xv6_tanhf ===\n");
    
    float test_vals[] = {0.0f, 1.0f, -1.0f, 2.0f, 5.0f, -5.0f};
    const char* labels[] = {"tanh(0)", "tanh(1)", "tanh(-1)", 
                            "tanh(2)", "tanh(5)", "tanh(-5)"};
    float expected[] = {0.0f, 0.76159f, -0.76159f, 0.96403f, 0.99991f, -0.99991f};
    
    for (int i = 0; i < 6; i++) {
        float result = xv6_tanhf(test_vals[i]);
        print_float(labels[i], result);
        
        float error = xv6_fabsf(result - expected[i]);
        int error_val = (int)(error * 100000);
        printf("  Error: 0.%d\n", error_val);
    }
}

int main(int argc, char *argv[]) {
    printf("xv6 Math Library Test Suite\n");
    printf("============================\n");
    printf("Testing accuracy of mathematical functions\n");
    
    test_fabsf();
    test_sqrtf();
    test_expf();
    test_logf();
    test_powf();
    test_sinf();
    test_cosf();
    test_tanhf();
    
    printf("\n=== All tests complete ===\n");
    printf("Check errors - should be < 1e-5 for most functions\n");
    
    exit(0);
}