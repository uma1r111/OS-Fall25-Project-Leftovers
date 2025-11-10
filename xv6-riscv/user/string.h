#ifndef XV6_STRING_H
#define XV6_STRING_H

#include "kernel/types.h"

// Basic string functions
void *xv6_memcpy(void *dest, const void *src, uint n);
void *xv6_memset(void *dest, int c, uint n);
int   xv6_strcmp(const char *p, const char *q);
uint  xv6_strlen(const char *s);
char *xv6_strcpy(char *dest, const char *src);
int   xv6_isprint(int c);
int   xv6_isspace(int c);
int   xv6_isdigit(int c);

// sprintf overloaded functions (no variadic support in xv6)
// Based on common llama2.c patterns:

// Single argument versions
int xv6_sprintf_d(char *buf, const char *fmt, int val);           // %d
int xv6_sprintf_s(char *buf, const char *fmt, const char *val);   // %s
int xv6_sprintf_f(char *buf, const char *fmt, float val);         // %f
int xv6_sprintf_c(char *buf, const char *fmt, char val);          // %c

// Two argument versions
int xv6_sprintf_dd(char *buf, const char *fmt, int v1, int v2);
int xv6_sprintf_ds(char *buf, const char *fmt, int v1, const char *v2);
int xv6_sprintf_sd(char *buf, const char *fmt, const char *v1, int v2);
int xv6_sprintf_ss(char *buf, const char *fmt, const char *v1, const char *v2);
int xv6_sprintf_ff(char *buf, const char *fmt, float v1, float v2);
int xv6_sprintf_df(char *buf, const char *fmt, int v1, float v2);
int xv6_sprintf_sf(char *buf, const char *fmt, const char *v1, float v2);

// Three argument versions (common in llama2.c for progress reporting)
int xv6_sprintf_ddd(char *buf, const char *fmt, int v1, int v2, int v3);
int xv6_sprintf_sdf(char *buf, const char *fmt, const char *v1, int v2, float v3);
int xv6_sprintf_sff(char *buf, const char *fmt, const char *v1, float v2, float v3);
int xv6_sprintf_dff(char *buf, const char *fmt, int v1, float v2, float v3);

// No argument version (plain string)
int xv6_sprintf_plain(char *buf, const char *fmt);

// sscanf overloaded functions
int xv6_sscanf_d(const char *str, const char *fmt, int *val);
int xv6_sscanf_s(const char *str, const char *fmt, char *val);
int xv6_sscanf_f(const char *str, const char *fmt, float *val);

int xv6_sscanf_dd(const char *str, const char *fmt, int *v1, int *v2);
int xv6_sscanf_ss(const char *str, const char *fmt, char *v1, char *v2);
int xv6_sscanf_ff(const char *str, const char *fmt, float *v1, float *v2);
int xv6_sscanf_ds(const char *str, const char *fmt, int *v1, char *v2);
int xv6_sscanf_df(const char *str, const char *fmt, int *v1, float *v2);
int xv6_sscanf_sf(const char *str, const char *fmt, char *v1, float *v2);

// Helper functions
int xv6_atoi(const char *s);
float xv6_atof(const char *s);

#endif /* XV6_STRING_H */