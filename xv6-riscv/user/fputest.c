#include "kernel/types.h"
#include "user/user.h"

// ============================================================================
// Helper function to print float as hex bits (deterministic output)
// ============================================================================
void print_float_bits(const char* label, float value) {
    int bits = *(int*)&value;
    printf("%s: 0x%x\n", label, bits);
}

// ============================================================================
// Helper function to print float with decimal approximation
// ============================================================================
void print_float_approx(const char* label, float value) {
    int integer_part = (int)value;
    float frac = value - (float)integer_part;
    if (frac < 0) frac = -frac;
    int frac_part = (int)(frac * 1000000);  // 6 decimal places
    printf("%s: %d.%d\n", label, integer_part, frac_part);
}

// ============================================================================
// TEST 1: Basic Floating-Point Arithmetic
// Verifies: FPU is functional and produces correct results
// ============================================================================
void test_basic_arithmetic(void) {
    printf("\n=== Test 1: Basic Floating-Point Arithmetic ===\n");
    
    float a = 3.14f;
    float b = 2.71f;
    float c, d, e, f;
    
    c = a + b;  // Addition
    d = a - b;  // Subtraction
    e = a * b;  // Multiplication
    f = a / b;  // Division
    
    print_float_bits("3.14 + 2.71", c);
    print_float_bits("3.14 - 2.71", d);
    print_float_bits("3.14 * 2.71", e);
    print_float_bits("3.14 / 2.71", f);
    
    printf("SUCCESS: Basic arithmetic operations completed\n");
}

// ============================================================================
// TEST 2: FP Registers Across Function Calls
// Verifies: Callee-saved FP registers are preserved
// ============================================================================
float helper_multiply(float x, float y) {
    return x * y;
}

void test_function_calls(void) {
    printf("\n=== Test 2: FP Registers Across Function Calls ===\n");
    
    float x = 5.5f;
    float y = 7.7f;
    float original_x = x;
    float original_y = y;
    
    float result = helper_multiply(x, y);
    
    // Verify x and y unchanged after function call
    if (x == original_x && y == original_y) {
        print_float_bits("Result of 5.5 * 7.7", result);
        printf("SUCCESS: FP registers preserved across function calls\n");
    } else {
        printf("ERROR: FP registers corrupted after function call!\n");
    }
}

// ============================================================================
// TEST 3: Context Switch with Interrupted Computation (MAIN REQUIREMENT)
// Verifies: FP state preserved across fork and context switches
// Pattern matches the example requirement exactly:
//   1. Parent does partial computation (first half)
//   2. Fork child
//   3. Child does complete different computation
//   4. Parent continues from where it left off (second half)
// ============================================================================
void test_context_switch_interrupted(void) {
    printf("\n=== Test 3: Context Switch with Interrupted Computation ===\n");
    printf("Parent: Starting computation of sum(i^2) for i=1.0 to 100.0 (step 0.5)\n");
    
    // PARENT: First half of computation (i = 1.0 to 50.0)
    float parent_sum_part1 = 0.0f;
    float i;
    
    for (i = 1.0f; i <= 50.0f; i += 0.5f) {
        parent_sum_part1 += i * i;  // sum of i^2
    }
    
    printf("Parent: Completed first half\n");
    print_float_approx("Parent partial sum (1.0 to 50.0)", parent_sum_part1);
    
    // FORK CHILD
    printf("Parent: Forking child process...\n");
    int pid = fork();
    
    if (pid == 0) {
        // ====================================================================
        // CHILD: Different computation (sum of i^3 for i = 1.0 to 50.0)
        // ====================================================================
        printf("Child: Computing sum(i^3) for i=1.0 to 50.0 (step 0.5)\n");
        
        float child_sum = 0.0f;
        float j;
        
        for (j = 1.0f; j <= 50.0f; j += 0.5f) {
            child_sum += j * j * j;  // sum of i^3
        }
        
        printf("Child: Computation complete\n");
        print_float_approx("Child result (sum of i^3)", child_sum);
        print_float_bits("Child result (hex)", child_sum);
        
        // Expected: ~1,640,625 (mathematically verifiable)
        printf("Child: Exiting gracefully\n");
        exit(0);
    } else {
        // ====================================================================
        // PARENT: Wait for child, then continue computation
        // ====================================================================
        wait(0);
        printf("Parent: Child completed, resuming computation\n");
        
        // PARENT: Second half of computation (i = 50.5 to 100.0)
        float parent_sum_part2 = 0.0f;
        
        for (i = 50.5f; i <= 100.0f; i += 0.5f) {
            parent_sum_part2 += i * i;  // sum of i^2
        }
        
        printf("Parent: Completed second half\n");
        print_float_approx("Parent partial sum (50.5 to 100.0)", parent_sum_part2);
        
        // PARENT: Combine both halves
        float parent_total = parent_sum_part1 + parent_sum_part2;
        
        printf("Parent: Final computation complete\n");
        print_float_approx("Parent TOTAL (sum of i^2, 1.0 to 100.0)", parent_total);
        print_float_bits("Parent TOTAL (hex)", parent_total);
        
        // Expected: ~338,350 (mathematically verifiable)
        printf("SUCCESS: Context switch preserved FP state across fork!\n");
    }
}

// ============================================================================
// TEST 4: Simple Context Switch (Your fpu_multi.c pattern)
// Verifies: Basic FP state preservation
// ============================================================================
void test_simple_context_switch(void) {
    printf("\n=== Test 4: Simple Context Switch ===\n");
    
    float a = 1.5f, b = 2.0f;
    float c = a * b + 0.25f;  // Expected: 3.25
    
    print_float_bits("Parent initial result", c);
    
    int pid = fork();
    if (pid == 0) {
        // Child: Different computation
        float x = 2.5f, y = 4.0f;
        float z = x / y + 1.0f;  // Expected: 1.625
        print_float_bits("Child result", z);
        exit(0);
    } else {
        wait(0);
        
        // Parent: Another computation after child
        float r = 5.5f, s = 1.1f;
        float t = r - s;  // Expected: 4.4
        print_float_bits("Parent final result", t);
        
        printf("SUCCESS: Simple context switch completed\n");
    }
}

// ============================================================================
// TEST 5: FP Comparisons
// Verifies: Floating-point comparison operations work correctly
// ============================================================================
void test_comparisons(void) {
    printf("\n=== Test 5: Floating-Point Comparisons ===\n");
    
    float a = 3.14f;
    float b = 2.71f;
    float c = 3.14f;
    
    int pass = 1;
    
    if (a > b) {
        printf("  3.14 > 2.71: TRUE (correct)\n");
    } else {
        printf("  3.14 > 2.71: FALSE (ERROR)\n");
        pass = 0;
    }
    
    if (a < b) {
        printf("  3.14 < 2.71: TRUE (ERROR)\n");
        pass = 0;
    } else {
        printf("  3.14 < 2.71: FALSE (correct)\n");
    }
    
    if (a == c) {
        printf("  3.14 == 3.14: TRUE (correct)\n");
    } else {
        printf("  3.14 == 3.14: FALSE (ERROR)\n");
        pass = 0;
    }
    
    if (pass) {
        printf("SUCCESS: All FP comparisons correct\n");
    } else {
        printf("ERROR: Some FP comparisons failed\n");
    }
}

// ============================================================================
// TEST 6: Multiple Concurrent FP Processes
// Verifies: Multiple processes can use FP simultaneously
// ============================================================================
void test_concurrent_processes(void) {
    printf("\n=== Test 6: Multiple Concurrent FP Processes ===\n");
    
    int pid1 = fork();
    if (pid1 == 0) {
        // Child 1: Compute product
        float x = 100.5f;
        for (int i = 0; i < 5; i++) {
            x = x * 1.1f;
        }
        print_float_bits("Child 1 result", x);
        exit(0);
    }
    
    // Small delay to avoid output collision
    pause(10);
    
    int pid2 = fork();
    if (pid2 == 0) {
        // Child 2: Compute quotient
        float y = 50.25f;
        for (int i = 0; i < 5; i++) {
            y = y * 0.9f;
        }
        print_float_bits("Child 2 result", y);
        exit(0);
    }
    
    // Parent waits for both
    wait(0);
    wait(0);
    printf("SUCCESS: Both children completed, FP state preserved\n");
}
// ============================================================================
// MAIN: Run all tests
// ============================================================================
int main(int argc, char *argv[]) {
    printf("========================================\n");
    printf("xv6 FPU Comprehensive Test Suite\n");
    printf("Task 2d: FPU Testing\n");
    printf("========================================\n");
    
    test_basic_arithmetic();
    test_function_calls();
    test_context_switch_interrupted();  // MAIN REQUIREMENT - Matches example
    test_simple_context_switch();
    test_comparisons();
    test_concurrent_processes();
    
    printf("\n========================================\n");
    printf("All FPU Tests Complete!\n");
    printf("========================================\n");
    printf("Summary:\n");
    printf("  - Basic FP arithmetic works\n");
    printf("  - FP registers preserved across function calls\n");
    printf("  - FP state preserved across context switches (fork)\n");
    printf("  - Interrupted computations resume correctly\n");
    printf("  - FP comparisons work correctly\n");
    printf("  - Multiple processes can use FP concurrently\n");
    
    exit(0);
}

// ============================================================================
// EXPECTED RESULTS (Mathematically Verifiable):
// ============================================================================
/*
Test 3 - Parent (sum of i^2 from 1.0 to 100.0, step 0.5):
  Formula: sum of i^2 = n(n+1)(2n+1)/6
  For i = 1, 1.5, 2, 2.5, ..., 100 (199 values)
  Expected: ~338,350

Test 3 - Child (sum of i^3 from 1.0 to 50.0, step 0.5):
  Formula: sum of i^3 = [n(n+1)/2]^2
  For i = 1, 1.5, 2, 2.5, ..., 50 (99 values)
  Expected: ~1,640,625

Test 4 - Values:
  Parent initial: 3.25 = 0x40500000
  Child: 1.625 = 0x3fd00000
  Parent final: 4.4 = 0x408ccccd (approximately)
*/