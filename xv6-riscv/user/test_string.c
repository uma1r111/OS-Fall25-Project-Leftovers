#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "string.h"

// Helper to print PASS / FAIL
static void print_result(char *name, int ok) {
    if (ok)
        printf("%s: PASS\n", name);
    else
        printf("%s: FAIL\n", name);
}

// Check null termination
__attribute__((unused)) static int is_null_terminated(char *buf, int max_len) {
    for (int i = 0; i < max_len; i++) {
        if (buf[i] == '\0') return 1;
    }
    return 0;
}


// Prevent overflow by checking unexpected writes
static int buffer_safe(char *buf, int size, char guard) {
    return buf[size] == guard;
}

/***********************
 * memcpy TESTS
 ***********************/
int test_memcpy(void) {
    int pass = 1;
    char guard = 'Z';

    // normal copy
    char src1[] = "hello";
    char dst1[10];
    xv6_memset(dst1, 0, sizeof(dst1));   // clear buffer
    dst1[9] = guard;

    xv6_memcpy(dst1, src1, 6);           // includes '\0'
    if (xv6_strcmp(dst1, "hello") != 0)
        pass = 0;
    if (dst1[9] != guard)            // guard still intact
        pass = 0;

    // overlapping (undefined) -- just ensure no crash, guard intact
    char buf2[10] = "abcdef";
    buf2[9] = guard;
    xv6_memcpy(buf2 + 2, buf2, 3);
    if (buf2[9] != guard)
        pass = 0;

    // zero length
    char dst3[5];
    xv6_memset(dst3, 0, sizeof(dst3));
    dst3[4] = guard;
    xv6_memcpy(dst3, "xyz", 0);          // should do nothing
    if (dst3[4] != guard)
        pass = 0;

    return pass;
}


/***********************
 * memset TESTS
 ***********************/
int test_memset() {
    int pass = 1;

    char guard = 'Z';

    // normal
    char buf1[10]; buf1[9] = guard;
    xv6_memset(buf1, 'A', 5);
    if (!(buf1[0]=='A' && buf1[4]=='A') || !buffer_safe(buf1, 9, guard))
        pass = 0;

    // set to zero
    char buf2[10]; buf2[9] = guard;
    xv6_memset(buf2, 0, 5);
    for (int i=0;i<5;i++) if (buf2[i] != 0) pass = 0;

    // zero length
    char buf3[10]; buf3[9] = guard;
    xv6_memset(buf3, 'X', 0);
    if (!buffer_safe(buf3, 9, guard))
        pass = 0;

    return pass;
}

/***********************
 * strcmp TESTS
 ***********************/
int test_strcmp() {
    int pass = 1;

    if (xv6_strcmp("abc", "abc") != 0) pass = 0;
    if (xv6_strcmp("abc", "abd") >= 0) pass = 0;
    if (xv6_strcmp("abd", "abc") <= 0) pass = 0;
    if (xv6_strcmp("", "") != 0) pass = 0;

    return pass;
}

/***********************
 * strlen TESTS
 ***********************/
int test_strlen() {
    int pass = 1;

    if (xv6_strlen("hello") != 5) pass = 0;
    if (xv6_strlen("") != 0) pass = 0;

    // long string
    char buf[50];
    for (int i=0;i<49;i++) buf[i]='a';
    buf[49]=0;

    if (xv6_strlen(buf) != 49) pass = 0;

    return pass;
}

/***********************
 * strcpy TESTS
 ***********************/
int test_strcpy(void) {
    int pass = 1;
    char guard = 'Z';

    // normal copy
    char src[] = "copy_this";
    char dst[20];
    xv6_memset(dst, 0, sizeof(dst));
    dst[19] = guard;

    xv6_strcpy(dst, src);
    if (xv6_strcmp(dst, src) != 0) pass = 0;
    if (dst[19] != guard) pass = 0;          // overflow check
    if (dst[xv6_strlen(src)] != '\0') pass = 0;  // null termination

    // empty string
    char dst2[10];
    xv6_memset(dst2, 'X', sizeof(dst2));
    dst2[9] = guard;
    strcpy(dst2, "");
    if (dst2[0] != '\0') pass = 0;
    if (dst2[9] != guard) pass = 0;

    // longer destination than source
    char src3[] = "short";
    char dst3[20];
    xv6_memset(dst3, 'A', sizeof(dst3));
    dst3[19] = guard;
    xv6_strcpy(dst3, src3);
    if (xv6_strcmp(dst3, "short") != 0) pass = 0;
    if (dst3[19] != guard) pass = 0;

    return pass;
}

/***********************
 * isprint TESTS
 ***********************/
int test_isprint(void) {
    int pass = 1;

    // printable characters
    for (int c = 32; c <= 126; c++) {
        if (!xv6_isprint(c)) pass = 0;
    }

    // non-printable characters
    for (int c = 0; c < 32; c++) {
        if (xv6_isprint(c)) pass = 0;
    }

    // edge
    if (xv6_isprint(127)) pass = 0;

    return pass;
}

/***********************
 * isspace TESTS
 ***********************/
int test_isspace(void) {
    int pass = 1;

    // space, tab, newline, carriage return, vertical tab, form feed
    char spaces[] = { ' ', '\t', '\n', '\r', '\v', '\f' };
    for (int i = 0; i < 6; i++) {
        if (!xv6_isspace(spaces[i])) pass = 0;
    }

    // non-space printable character
    if (xv6_isspace('A')) pass = 0;
    if (xv6_isspace('1')) pass = 0;
    if (xv6_isspace('!')) pass = 0;

    return pass;
}


static int float_eq(float a, float b, float epsilon) {
    float diff = a - b;
    if (diff < 0) diff = -diff;
    return diff < epsilon;
}

/***********************
 * SPRINTF TESTS
 ***********************/
int test_sprintf_basic(void) {
    int pass = 1;
    char buf[128];

    // Test %d
    xv6_sprintf_d(buf, "Number: %d", 42);
    if (xv6_strcmp(buf, "Number: 42") != 0) {
        printf("Expected 'Number: 42', got '%s'\n", buf);
        pass = 0;
    }

    // Test negative %d
    xv6_sprintf_d(buf, "Negative: %d", -100);
    if (xv6_strcmp(buf, "Negative: -100") != 0) {
        printf("Expected 'Negative: -100', got '%s'\n", buf);
        pass = 0;
    }

    // Test %s
    xv6_sprintf_s(buf, "Hello %s", "world");
    if (xv6_strcmp(buf, "Hello world") != 0) {
        printf("Expected 'Hello world', got '%s'\n", buf);
        pass = 0;
    }

    // Test %c
    xv6_sprintf_c(buf, "Char: %c", 'A');
    if (xv6_strcmp(buf, "Char: A") != 0) {
        printf("Expected 'Char: A', got '%s'\n", buf);
        pass = 0;
    }

    // Test %%
    xv6_sprintf_plain(buf, "Percent: %%");
    if (xv6_strcmp(buf, "Percent: %") != 0) {
        printf("Expected 'Percent: %%', got '%s'\n", buf);
        pass = 0;
    }

    return pass;
}

int test_sprintf_float(void) {
    int pass = 1;
    char buf[128];

    // Test basic float
    xv6_sprintf_f(buf, "Pi: %f", 3.14159);
    // Default precision is 6
    if (xv6_strcmp(buf, "Pi: 3.141590") != 0) {
        printf("Expected 'Pi: 3.141590', got '%s'\n", buf);
        pass = 0;
    }

    // Test float with precision
    xv6_sprintf_f(buf, "Value: %.2f", 1.5678);
    if (xv6_strcmp(buf, "Value: 1.56") != 0) {
        printf("Expected 'Value: 1.56', got '%s'\n", buf);
        pass = 0;
    }

    // Test negative float
    xv6_sprintf_f(buf, "Neg: %.3f", -2.718);
    if (xv6_strcmp(buf, "Neg: -2.718") != 0) {
        printf("Expected 'Neg: -2.718', got '%s'\n", buf);
        pass = 0;
    }

    return pass;
}

int test_sprintf_multiple(void) {
    int pass = 1;
    char buf[128];

    // Test two integers
    xv6_sprintf_dd(buf, "%d + %d", 10, 20);
    if (xv6_strcmp(buf, "10 + 20") != 0) {
        printf("Expected '10 + 20', got '%s'\n", buf);
        pass = 0;
    }

    // Test string and int
    xv6_sprintf_sd(buf, "%s scored %d", "Alice", 95);
    if (xv6_strcmp(buf, "Alice scored 95") != 0) {
        printf("Expected 'Alice scored 95', got '%s'\n", buf);
        pass = 0;
    }

    // Test string and float
    xv6_sprintf_sf(buf, "%s: %.2f", "Temperature", 98.6);
    if (xv6_strcmp(buf, "Temperature: 98.60") != 0) {
        printf("Expected 'Temperature: 98.60', got '%s'\n", buf);
        pass = 0;
    }

    // Test three arguments
    xv6_sprintf_ddd(buf, "Progress: %d/%d (%d%%)", 45, 100, 45);
    if (xv6_strcmp(buf, "Progress: 45/100 (45%)") != 0) {
        printf("Expected 'Progress: 45/100 (45%%)', got '%s'\n", buf);
        pass = 0;
    }

    return pass;
}

/***********************
 * SSCANF TESTS
 ***********************/
int test_sscanf_basic(void) {
    int pass = 1;
    int ival;
    char sval[64];
    float fval;

    // Test %d
    xv6_sscanf_d("42", "%d", &ival);
    if (ival != 42) {
        printf("Expected 42, got %d\n", ival);
        pass = 0;
    }

    // Test negative %d
    xv6_sscanf_d("-123", "%d", &ival);
    if (ival != -123) {
        printf("Expected -123, got %d\n", ival);
        pass = 0;
    }

    // Test %s
    xv6_sscanf_s("hello", "%s", sval);
    if (xv6_strcmp(sval, "hello") != 0) {
        printf("Expected 'hello', got '%s'\n", sval);
        pass = 0;
    }

    // Test %f
    xv6_sscanf_f("3.14", "%f", &fval);
    if (!float_eq(fval, 3.14, 0.01)) {
        printf("Expected 3.14, got %f\n", (double)fval);
        pass = 0;
    }

    return pass;
}

int test_sscanf_format(void) {
    int pass = 1;
    int ival;
    char sval[64];

    // Test with literal text
    xv6_sscanf_d("Number: 42", "Number: %d", &ival);
    if (ival != 42) {
        printf("Expected 42, got %d\n", ival);
        pass = 0;
    }

    // Test with whitespace
    xv6_sscanf_d("  123  ", "%d", &ival);
    if (ival != 123) {
        printf("Expected 123, got %d\n", ival);
        pass = 0;
    }

    // Test string with whitespace
    xv6_sscanf_s("  world  ", "%s", sval);
    if (xv6_strcmp(sval, "world") != 0) {
        printf("Expected 'world', got '%s'\n", sval);
        pass = 0;
    }

    return pass;
}

int test_sscanf_multiple(void) {
    int pass = 1;
    int v1, v2;
    char s1[64], s2[64];
    float f1, f2;

    // Test two integers
    xv6_sscanf_dd("10 20", "%d %d", &v1, &v2);
    if (v1 != 10 || v2 != 20) {
        printf("Expected 10, 20; got %d, %d\n", v1, v2);
        pass = 0;
    }

    // Test two strings
    xv6_sscanf_ss("hello world", "%s %s", s1, s2);
    if (xv6_strcmp(s1, "hello") != 0 || xv6_strcmp(s2, "world") != 0) {
        printf("Expected 'hello', 'world'; got '%s', '%s'\n", s1, s2);
        pass = 0;
    }

    // Test two floats
    xv6_sscanf_ff("1.5 2.5", "%f %f", &f1, &f2);
    if (!float_eq(f1, 1.5, 0.01) || !float_eq(f2, 2.5, 0.01)) {
        printf("Expected 1.5, 2.5; got %f, %f\n", (double)f1, (double)f2);
        pass = 0;
    }

    // Test int and string
    xv6_sscanf_ds("42 test", "%d %s", &v1, s1);
    if (v1 != 42 || xv6_strcmp(s1, "test") != 0) {
        printf("Expected 42, 'test'; got %d, '%s'\n", v1, s1);
        pass = 0;
    }

    return pass;
}

int test_sscanf_complex(void) {
    int pass = 1;
    int score;
    char name[64];
    float percentage;

    // Test format like "Alice scored 95"
    xv6_sscanf_ds("Alice scored 95", "%s scored %d", &score, name);
    if (xv6_strcmp(name, "Alice") != 0 || score != 95) {
        printf("Expected 'Alice', 95; got '%s', %d\n", name, score);
        pass = 0;
    }

    // Test format like "Temperature: 98.6"
    xv6_sscanf_sf("Temperature: 98.6", "%s %f", name, &percentage);
    if (xv6_strcmp(name, "Temperature:") != 0 || !float_eq(percentage, 98.6, 0.01)) {
        printf("Expected 'Temperature:', 98.6; got '%s', %f\n", name, (double)percentage);
        pass = 0;
    }

    return pass;
}

/***********************
 * HELPER FUNCTION TESTS
 ***********************/
int test_atoi(void) {
    int pass = 1;

    if (xv6_atoi("123") != 123) pass = 0;
    if (xv6_atoi("-456") != -456) pass = 0;
    if (xv6_atoi("0") != 0) pass = 0;
    if (xv6_atoi("  789  ") != 789) pass = 0;

    return pass;
}

int test_atof(void) {
    int pass = 1;

    if (!float_eq(xv6_atof("3.14"), 3.14, 0.01)) pass = 0;
    if (!float_eq(xv6_atof("-2.5"), -2.5, 0.01)) pass = 0;
    if (!float_eq(xv6_atof("0.0"), 0.0, 0.01)) pass = 0;
    if (!float_eq(xv6_atof("  1.5  "), 1.5, 0.01)) pass = 0;

    return pass;
}

/***********************
 * ROUND-TRIP TESTS
 ***********************/
int test_roundtrip(void) {
    int pass = 1;
    char buf[128];
    int ival;
    char sval[64];
    float fval;

    // Integer round-trip
    xv6_sprintf_d(buf, "%d", 42);
    xv6_sscanf_d(buf, "%d", &ival);
    if (ival != 42) pass = 0;

    // String round-trip
    xv6_sprintf_s(buf, "%s", "test");
    xv6_sscanf_s(buf, "%s", sval);
    if (xv6_strcmp(sval, "test") != 0) pass = 0;

    // Float round-trip (with tolerance)
    xv6_sprintf_f(buf, "%.2f", 3.14);
    xv6_sscanf_f(buf, "%f", &fval);
    if (!float_eq(fval, 3.14, 0.01)) pass = 0;

    return pass;
}
/***********************
 * MASTER RUNNER
 ***********************/
int main() {
    printf("---- STRING TESTS ----\n");

    print_result("memcpy", test_memcpy());
    print_result("memset", test_memset());
    print_result("strcmp", test_strcmp());
    print_result("strlen", test_strlen());
    print_result("strcpy", test_strcpy());
    print_result("isprint", test_isprint());
    print_result("isspace", test_isspace());
    // print_result("sprintf", test_sprintf());
    printf("==== SPRINTF/SSCANF TESTS ====\n");

    printf("\n--- SPRINTF Tests ---\n");
    print_result("sprintf_basic", test_sprintf_basic());
    print_result("sprintf_float", test_sprintf_float());
    print_result("sprintf_multiple", test_sprintf_multiple());

    printf("\n--- SSCANF Tests ---\n");
    print_result("sscanf_basic", test_sscanf_basic());
    print_result("sscanf_format", test_sscanf_format());
    print_result("sscanf_multiple", test_sscanf_multiple());
    print_result("sscanf_complex", test_sscanf_complex());

    printf("\n--- Helper Tests ---\n");
    print_result("atoi", test_atoi());
    print_result("atof", test_atof());

    printf("\n--- Round-trip Tests ---\n");
    print_result("roundtrip", test_roundtrip());

    printf("\n==============================\n");

    printf("-----------------------\n");
    exit(0);
}
