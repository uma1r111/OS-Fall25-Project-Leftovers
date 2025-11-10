#include "kernel/types.h"
#include "user/user.h"
#include "user/string.h"

// ==================== BASIC STRING FUNCTIONS ====================

void *xv6_memcpy(void *dest, const void *src, uint n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void *xv6_memset(void *dest, int c, uint n) {
    unsigned char *d = dest;
    while (n--) *d++ = (unsigned char)c;
    return dest;
}

int xv6_strcmp(const char *p, const char *q) {
    while (*p && *p == *q) { p++; q++; }
    return (unsigned char)*p - (unsigned char)*q;
}

uint xv6_strlen(const char *s) {
    uint n = 0;
    while (s[n]) n++;
    return n;
}

char *xv6_strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

int xv6_isprint(int c) {
    return (c >= 32 && c <= 126);
}

int xv6_isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' ||
            c == '\r' || c == '\v' || c == '\f');
}

int xv6_isdigit(int c) {
    return (c >= '0' && c <= '9');
}

// ==================== HELPER FUNCTIONS ====================

int xv6_atoi(const char *s) {
    int n = 0;
    int sign = 1;
    
    // Skip whitespace
    while (xv6_isspace(*s)) s++;
    
    // Handle sign
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    
    // Convert digits
    while (xv6_isdigit(*s)) {
        n = n * 10 + (*s - '0');
        s++;
    }
    
    return sign * n;
}

float xv6_atof(const char *s) {
    float result = 0.0;
    float sign = 1.0;
    int seen_dot = 0;
    float divisor = 10.0;
    
    // Skip whitespace
    while (xv6_isspace(*s)) s++;
    
    // Handle sign
    if (*s == '-') {
        sign = -1.0;
        s++;
    } else if (*s == '+') {
        s++;
    }
    
    // Convert number
    while (*s) {
        if (xv6_isdigit(*s)) {
            if (!seen_dot) {
                result = result * 10.0 + (*s - '0');
            } else {
                result += (*s - '0') / divisor;
                divisor *= 10.0;
            }
        } else if (*s == '.' && !seen_dot) {
            seen_dot = 1;
        } else {
            break;
        }
        s++;
    }
    
    return sign * result;
}

// ==================== SPRINTF HELPERS ====================

// Convert integer to string
static int itoa(int val, char *buf, int is_negative) {
    char tmp[32];
    int i = 0;
    int len = 0;
    uint uval;
    
    if (is_negative) {
        uval = -val;
    } else {
        uval = val;
    }
    
    if (uval == 0) {
        tmp[i++] = '0';
    } else {
        while (uval > 0) {
            tmp[i++] = '0' + (uval % 10);
            uval /= 10;
        }
    }
    
    if (is_negative) {
        buf[len++] = '-';
    }
    
    // Reverse
    while (i > 0) {
        buf[len++] = tmp[--i];
    }
    
    return len;
}

// Convert float to string with precision
static int ftoa(float val, char *buf, int precision) {
    int len = 0;
    int int_part;
    float frac_part;
    int is_negative = 0;
    
    if (val < 0) {
        is_negative = 1;
        val = -val;
    }
    
    // Integer part
    int_part = (int)val;
    frac_part = val - int_part;
    
    len = itoa(int_part, buf, is_negative);
    
    // Decimal point
    buf[len++] = '.';
    
    // Fractional part
    for (int i = 0; i < precision; i++) {
        frac_part *= 10;
        int digit = (int)frac_part;
        buf[len++] = '0' + digit;
        frac_part -= digit;
    }
    
    return len;
}

// Parse format specifier
typedef struct {
    int width;
    int precision;
    char type;
} FormatSpec;

static const char *parse_format(const char *fmt, FormatSpec *spec) {
    spec->width = -1;
    spec->precision = -1;
    spec->type = 0;
    
    if (*fmt != '%') return fmt;
    fmt++; // skip %
    
    // Check for %%
    if (*fmt == '%') {
        spec->type = '%';
        return fmt + 1;
    }
    
    // Parse width
    if (xv6_isdigit(*fmt)) {
        spec->width = 0;
        while (xv6_isdigit(*fmt)) {
            spec->width = spec->width * 10 + (*fmt - '0');
            fmt++;
        }
    }
    
    // Parse precision
    if (*fmt == '.') {
        fmt++;
        spec->precision = 0;
        while (xv6_isdigit(*fmt)) {
            spec->precision = spec->precision * 10 + (*fmt - '0');
            fmt++;
        }
    }
    
    // Parse type
    if (*fmt == 'd' || *fmt == 's' || *fmt == 'f' || *fmt == 'c') {
        spec->type = *fmt;
        fmt++;
    }
    
    return fmt;
}

// Generic sprintf implementation
static int sprintf_impl(char *buf, const char *fmt, 
                       int *int_args, const char **str_args, 
                       float *float_args, char *char_args,
                       int n_ints, int n_strs, int n_floats, int n_chars) {
    int len = 0;
    int int_idx = 0, str_idx = 0, float_idx = 0, char_idx = 0;
    
    while (*fmt) {
        if (*fmt != '%') {
            buf[len++] = *fmt++;
            continue;
        }
        
        FormatSpec spec;
        fmt = parse_format(fmt, &spec);
        
        if (spec.type == '%') {
            buf[len++] = '%';
        } else if (spec.type == 'd' && int_idx < n_ints) {
            int val = int_args[int_idx];
            int is_negative = (val < 0);
            int_idx++;
            len += itoa(val, buf + len, is_negative);
        } else if (spec.type == 's' && str_idx < n_strs) {
            const char *s = str_args[str_idx++];
            while (*s) {
                buf[len++] = *s++;
            }
        } else if (spec.type == 'f' && float_idx < n_floats) {
            int prec = (spec.precision >= 0) ? spec.precision : 6;
            len += ftoa(float_args[float_idx++], buf + len, prec);
        } else if (spec.type == 'c' && char_idx < n_chars) {
            buf[len++] = char_args[char_idx++];
        }
    }
    
    buf[len] = '\0';
    return len;
}

// ==================== SPRINTF OVERLOADED FUNCTIONS ====================

int xv6_sprintf_plain(char *buf, const char *fmt) {
    return sprintf_impl(buf, fmt, 0, 0, 0, 0, 0, 0, 0, 0);
}

int xv6_sprintf_d(char *buf, const char *fmt, int val) {
    int args[] = {val};
    return sprintf_impl(buf, fmt, args, 0, 0, 0, 1, 0, 0, 0);
}

int xv6_sprintf_s(char *buf, const char *fmt, const char *val) {
    const char *args[] = {val};
    return sprintf_impl(buf, fmt, 0, args, 0, 0, 0, 1, 0, 0);
}

int xv6_sprintf_f(char *buf, const char *fmt, float val) {
    float args[] = {val};
    return sprintf_impl(buf, fmt, 0, 0, args, 0, 0, 0, 1, 0);
}

int xv6_sprintf_c(char *buf, const char *fmt, char val) {
    char args[] = {val};
    return sprintf_impl(buf, fmt, 0, 0, 0, args, 0, 0, 0, 1);
}

int xv6_sprintf_dd(char *buf, const char *fmt, int v1, int v2) {
    int args[] = {v1, v2};
    return sprintf_impl(buf, fmt, args, 0, 0, 0, 2, 0, 0, 0);
}

int xv6_sprintf_ds(char *buf, const char *fmt, int v1, const char *v2) {
    int iargs[] = {v1};
    const char *sargs[] = {v2};
    return sprintf_impl(buf, fmt, iargs, sargs, 0, 0, 1, 1, 0, 0);
}

int xv6_sprintf_sd(char *buf, const char *fmt, const char *v1, int v2) {
    int iargs[] = {v2};
    const char *sargs[] = {v1};
    return sprintf_impl(buf, fmt, iargs, sargs, 0, 0, 1, 1, 0, 0);
}

int xv6_sprintf_ss(char *buf, const char *fmt, const char *v1, const char *v2) {
    const char *args[] = {v1, v2};
    return sprintf_impl(buf, fmt, 0, args, 0, 0, 0, 2, 0, 0);
}

int xv6_sprintf_ff(char *buf, const char *fmt, float v1, float v2) {
    float args[] = {v1, v2};
    return sprintf_impl(buf, fmt, 0, 0, args, 0, 0, 0, 2, 0);
}

int xv6_sprintf_df(char *buf, const char *fmt, int v1, float v2) {
    int iargs[] = {v1};
    float fargs[] = {v2};
    return sprintf_impl(buf, fmt, iargs, 0, fargs, 0, 1, 0, 1, 0);
}

int xv6_sprintf_sf(char *buf, const char *fmt, const char *v1, float v2) {
    const char *sargs[] = {v1};
    float fargs[] = {v2};
    return sprintf_impl(buf, fmt, 0, sargs, fargs, 0, 0, 1, 1, 0);
}

int xv6_sprintf_ddd(char *buf, const char *fmt, int v1, int v2, int v3) {
    int args[] = {v1, v2, v3};
    return sprintf_impl(buf, fmt, args, 0, 0, 0, 3, 0, 0, 0);
}

int xv6_sprintf_sdf(char *buf, const char *fmt, const char *v1, int v2, float v3) {
    int iargs[] = {v2};
    const char *sargs[] = {v1};
    float fargs[] = {v3};
    return sprintf_impl(buf, fmt, iargs, sargs, fargs, 0, 1, 1, 1, 0);
}

int xv6_sprintf_sff(char *buf, const char *fmt, const char *v1, float v2, float v3) {
    const char *sargs[] = {v1};
    float fargs[] = {v2, v3};
    return sprintf_impl(buf, fmt, 0, sargs, fargs, 0, 0, 1, 2, 0);
}

int xv6_sprintf_dff(char *buf, const char *fmt, int v1, float v2, float v3) {
    int iargs[] = {v1};
    float fargs[] = {v2, v3};
    return sprintf_impl(buf, fmt, iargs, 0, fargs, 0, 1, 0, 2, 0);
}

// ==================== SSCANF IMPLEMENTATION ====================

// Skip whitespace in input string
static const char *skip_whitespace(const char *s) {
    while (xv6_isspace(*s)) s++;
    return s;
}

// Match literal characters in format
__attribute__((unused)) static const char *match_literal(const char *str, const char *fmt) {
    while (*fmt && *fmt != '%') {
        if (xv6_isspace(*fmt)) {
            str = skip_whitespace(str);
            fmt++;
        } else if (*str == *fmt) {
            str++;
            fmt++;
        } else {
            return 0; // Mismatch
        }
    }
    return str;
}

// Generic sscanf implementation
static int sscanf_impl(const char *str, const char *fmt,
                       int **int_ptrs, char **str_ptrs,
                       float **float_ptrs,
                       int n_ints, int n_strs, int n_floats) {
    int matches = 0;
    int int_idx = 0, str_idx = 0, float_idx = 0;
    
    while (*fmt && *str) {
        // Skip whitespace in format
        if (xv6_isspace(*fmt)) {
            fmt++;
            str = skip_whitespace(str);
            continue;
        }
        
        // Handle format specifier
        if (*fmt == '%') {
            fmt++;
            
            if (*fmt == 'd' && int_idx < n_ints) {
                str = skip_whitespace(str);
                *int_ptrs[int_idx++] = xv6_atoi(str);
                matches++;
                
                // Skip past the number
                if (*str == '-' || *str == '+') str++;
                while (xv6_isdigit(*str)) str++;
                
            } else if (*fmt == 's' && str_idx < n_strs) {
                str = skip_whitespace(str);
                char *dest = str_ptrs[str_idx++];
                
                // Copy until whitespace
                while (*str && !xv6_isspace(*str)) {
                    *dest++ = *str++;
                }
                *dest = '\0';
                matches++;
                
            } else if (*fmt == 'f' && float_idx < n_floats) {
                str = skip_whitespace(str);
                *float_ptrs[float_idx++] = xv6_atof(str);
                matches++;
                
                // Skip past the number
                if (*str == '-' || *str == '+') str++;
                while (xv6_isdigit(*str) || *str == '.') str++;
            }
            fmt++;
            
        } else {
            // Match literal character
            if (*fmt == *str) {
                fmt++;
                str++;
            } else {
                break;
            }
        }
    }
    
    return matches;
}

// ==================== SSCANF OVERLOADED FUNCTIONS ====================

int xv6_sscanf_d(const char *str, const char *fmt, int *val) {
    int *ptrs[] = {val};
    return sscanf_impl(str, fmt, ptrs, 0, 0, 1, 0, 0);
}

int xv6_sscanf_s(const char *str, const char *fmt, char *val) {
    char *ptrs[] = {val};
    return sscanf_impl(str, fmt, 0, ptrs, 0, 0, 1, 0);
}

int xv6_sscanf_f(const char *str, const char *fmt, float *val) {
    float *ptrs[] = {val};
    return sscanf_impl(str, fmt, 0, 0, ptrs, 0, 0, 1);
}

int xv6_sscanf_dd(const char *str, const char *fmt, int *v1, int *v2) {
    int *ptrs[] = {v1, v2};
    return sscanf_impl(str, fmt, ptrs, 0, 0, 2, 0, 0);
}

int xv6_sscanf_ss(const char *str, const char *fmt, char *v1, char *v2) {
    char *ptrs[] = {v1, v2};
    return sscanf_impl(str, fmt, 0, ptrs, 0, 0, 2, 0);
}

int xv6_sscanf_ff(const char *str, const char *fmt, float *v1, float *v2) {
    float *ptrs[] = {v1, v2};
    return sscanf_impl(str, fmt, 0, 0, ptrs, 0, 0, 2);
}

int xv6_sscanf_ds(const char *str, const char *fmt, int *v1, char *v2) {
    int *iptrs[] = {v1};
    char *sptrs[] = {v2};
    return sscanf_impl(str, fmt, iptrs, sptrs, 0, 1, 1, 0);
}

int xv6_sscanf_df(const char *str, const char *fmt, int *v1, float *v2) {
    int *iptrs[] = {v1};
    float *fptrs[] = {v2};
    return sscanf_impl(str, fmt, iptrs, 0, fptrs, 1, 0, 1);
}

int xv6_sscanf_sf(const char *str, const char *fmt, char *v1, float *v2) {
    char *sptrs[] = {v1};
    float *fptrs[] = {v2};
    return sscanf_impl(str, fmt, 0, sptrs, fptrs, 0, 1, 1);
}