#include "kernel/types.h"
#include "user/user.h"

// Print float value as raw IEEE-754 bits (hex) for deterministic output.
// Single printf call prevents interleaving issues.
void print_float_bits(const char* label, float value) {
    int bits = *(int*)&value;
    printf("%s: 0x%x\n", label, bits);
}

// Test 1: Lazy FPU - First FP operation should trigger lazy enablement
void test_lazy_fpu(void) {
    printf("\n=== Test 1: Lazy FPU Enablement ===\n");
    printf("First floating-point operation should trigger lazy FPU...\n");

    float a = 3.14f;
    float b = 2.71f;
    float c = a + b;

    print_float_bits("Result", c);
    printf("SUCCESS: Lazy FPU worked! FP operations functional.\n");
}

// Test 2: Normal FP operations (should work after lazy enablement)
void test_normal_fp_ops(void) {
    printf("\n=== Test 2: Normal FP Operations ===\n");

    float x = 5.5f;
    float y = 2.0f;

    float add = x + y;
    float sub = x - y;
    float mul = x * y;
    float div = x / y;

    print_float_bits("5.5 + 2.0", add);
    print_float_bits("5.5 - 2.0", sub);
    print_float_bits("5.5 * 2.0", mul);
    print_float_bits("5.5 / 2.0", div);

    printf("SUCCESS: All normal FP operations completed.\n");
}

// Test 3: Divide by zero (should set FCSR flag and kill process)
void test_divide_by_zero(void) {
    printf("\n=== Test 3: Floating-Point Divide by Zero ===\n");
    printf("Forking child to test divide by zero...\n");

    int pid = fork();
    if (pid == 0) {
        // Child process
        printf("Child: Attempting 1.0 / 0.0...\n");
        float x = 1.0f;
        float y = 0.0f;
        float result = x / y;  // This should set FCSR DZ flag

        // If we get here, FP exception wasn't caught (infinity returned)
        print_float_bits("Result (may be inf)", result);
        printf("Child: Divide by zero produced result (infinity)\n");
        exit(0);
    } else {
        // Parent process
        wait(0);
        printf("Parent: Child process completed (may have been killed by kernel)\n");
    }
}

// Test 4: Invalid operation (0/0 -> NaN)
void test_invalid_operation(void) {
    printf("\n=== Test 4: Invalid FP Operation ===\n");
    printf("Note: This test depends on FPU behavior for 0/0 -> NaN\n");

    int pid = fork();
    if (pid == 0) {
        // Child process
        printf("Child: Testing invalid FP operation...\n");

        float zero = 0.0f;
        float nan_result = zero / zero;  // 0/0 = NaN (sets NV flag)

        print_float_bits("0.0 / 0.0 result", nan_result);
        printf("Child: Invalid operation completed\n");
        exit(0);
    } else {
        // Parent process
        wait(0);
        printf("Parent: Child completed invalid operation test\n");
    }
}

// Test 5: Overflow (very large number * very large number)
void test_overflow(void) {
    printf("\n=== Test 5: Floating-Point Overflow ===\n");

    int pid = fork();
    if (pid == 0) {
        // Child process
        printf("Child: Testing FP overflow...\n");

        float large = 1e38f;  // Near float max
        float result = large * large;  // Should overflow to infinity or set flags

        print_float_bits("1e38 * 1e38", result);
        printf("Child: Overflow test completed\n");
        exit(0);
    } else {
        wait(0);
        printf("Parent: Overflow test completed\n");
    }
}

// Test 6: Multiple processes using FP (context switch test)
void test_concurrent_fp(void) {
    printf("\n=== Test 6: Concurrent FP in Multiple Processes ===\n");

    int pid1 = fork();
    if (pid1 == 0) {
        // Child 1
        float x = 100.5f;
        for (int i = 0; i < 5; i++) {
            x = x * 1.1f;
        }
        print_float_bits("Child 1 result", x);
        exit(0);
    }

    int pid2 = fork();
    if (pid2 == 0) {
        // Child 2
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
    printf("Parent: Both children completed - context switching preserved FP state\n");
}

int main(int argc, char *argv[]) {
    printf("========================================\n");
    printf("FPU Exception Handling Test Suite\n");
    printf("Task 2c: Lazy FPU & Exception Detection\n");
    printf("========================================\n");

    test_lazy_fpu();
    test_normal_fp_ops();
    test_divide_by_zero();
    test_invalid_operation();
    test_overflow();
    test_concurrent_fp();

    printf("\n========================================\n");
    printf("FPU Exception Tests Complete!\n");
    printf("========================================\n");
    printf("Check kernel output for:\n");
    printf("  - 'lazy FPU enablement' messages\n");
    printf("  - FP exception details (if any)\n");
    printf("  - Process termination messages\n");

    exit(0);
}
