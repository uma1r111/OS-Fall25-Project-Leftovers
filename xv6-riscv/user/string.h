#ifndef XV6_STRING_H
#define XV6_STRING_H

#include "kernel/types.h"

// Basic String Functions
void *xv6_memcpy(void *dest, const void *src, uint n);
void *xv6_memset(void *dest, int c, uint n);
int xv6_strcmp(const char *p, const char *q);
uint xv6_strlen(const char *s);
char *xv6_strcpy(char *dest, const char *src);
char *xv6_strcat(char *dest, const char *src);

// Character Classification
int xv6_isprint(int c);
int xv6_isspace(int c);
int xv6_isdigit(int c);

// Conversion
int xv6_atoi(const char *s);
float xv6_atof(const char *s);

// Sprintf Overloads
int xv6_sprintf_plain(char *buf, const char *fmt);
int xv6_sprintf_d(char *buf, const char *fmt, int val);
int xv6_sprintf_s(char *buf, const char *fmt, const char *val);
int xv6_sprintf_f(char *buf, const char *fmt, float val);
int xv6_sprintf_c(char *buf, const char *fmt, char val);
int xv6_sprintf_dd(char *buf, const char *fmt, int v1, int v2);
int xv6_sprintf_ds(char *buf, const char *fmt, int v1, const char *v2);
int xv6_sprintf_sd(char *buf, const char *fmt, const char *v1, int v2);
int xv6_sprintf_ss(char *buf, const char *fmt, const char *v1, const char *v2);
int xv6_sprintf_ff(char *buf, const char *fmt, float v1, float v2);
int xv6_sprintf_df(char *buf, const char *fmt, int v1, float v2);
int xv6_sprintf_sf(char *buf, const char *fmt, const char *v1, float v2);
int xv6_sprintf_ddd(char *buf, const char *fmt, int v1, int v2, int v3);
int xv6_sprintf_sdf(char *buf, const char *fmt, const char *v1, int v2, float v3);
int xv6_sprintf_sff(char *buf, const char *fmt, const char *v1, float v2, float v3);
int xv6_sprintf_dff(char *buf, const char *fmt, int v1, float v2, float v3);

// Sscanf Overloads
int xv6_sscanf_d(const char *str, const char *fmt, int *val);
int xv6_sscanf_s(const char *str, const char *fmt, char *val);
int xv6_sscanf_f(const char *str, const char *fmt, float *val);
int xv6_sscanf_dd(const char *str, const char *fmt, int *v1, int *v2);
int xv6_sscanf_ss(const char *str, const char *fmt, char *v1, char *v2);
int xv6_sscanf_ff(const char *str, const char *fmt, float *v1, float *v2);
int xv6_sscanf_ds(const char *str, const char *fmt, int *v1, char *v2);
int xv6_sscanf_df(const char *str, const char *fmt, int *v1, float *v2);
int xv6_sscanf_sf(const char *str, const char *fmt, char *v1, float *v2);

#endif // XV6_STRING_H