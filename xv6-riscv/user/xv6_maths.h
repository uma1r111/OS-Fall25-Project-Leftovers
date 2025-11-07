// xv6_maths.h
// Mathematical functions for xv6 (user-level library)
// Implements floating-point math operations for llama2.c

#ifndef XV6_MATHS_H
#define XV6_MATHS_H

// Mathematical constants
#define XV6_PI    3.14159265358979323846f
#define XV6_E     2.71828182845904523536f
#define XV6_LN2   0.69314718055994530942f
#define XV6_LOG2E 1.44269504088896340736f

// Special floating-point values using IEEE 754 bit patterns
static inline float xv6_infinity(void) {
    unsigned int inf_bits = 0x7F800000u;
    return *(float*)&inf_bits;
}

static inline float xv6_nan(void) {
    unsigned int nan_bits = 0x7FC00000u;
    return *(float*)&nan_bits;
}

static inline int xv6_isnan(float x) {
    unsigned int bits = *(unsigned int*)&x;
    return ((bits & 0x7F800000u) == 0x7F800000u) && ((bits & 0x007FFFFFu) != 0);
}

static inline int xv6_isinf(float x) {
    unsigned int bits = *(unsigned int*)&x;
    return ((bits & 0x7FFFFFFFu) == 0x7F800000u);
}

// Basic mathematical functions
float xv6_fabsf(float x);
float xv6_sqrtf(float x);
float xv6_expf(float x);
float xv6_logf(float x);
float xv6_powf(float base, float exponent);
float xv6_sinf(float x);
float xv6_cosf(float x);
float xv6_tanhf(float x);

// Helper functions (optional - can be used internally)
float xv6_floorf(float x);
float xv6_fmodf(float x, float y);

#endif // XV6_MATHS_H