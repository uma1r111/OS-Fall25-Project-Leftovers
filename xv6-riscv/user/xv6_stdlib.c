#include "kernel/types.h"
#include "user/user.h"
#include "xv6_stdlib.h"

// Allocate and zero-initialize memory
// Returns NULL if nmemb or size is 0, or if allocation fails
void*
calloc(uint nmemb, uint size)
{
  uint total_size;
  void *ptr;

  // Handle zero-size allocation
  if (nmemb == 0 || size == 0)
    return 0;

  // Check for overflow
  total_size = nmemb * size;
  if (total_size / nmemb != size)
    return 0; // Overflow occurred

  ptr = malloc(total_size);
  if (ptr == 0)
    return 0;

  // Zero-initialize the allocated memory
  memset(ptr, 0, total_size);
  return ptr;
}

// Binary search in sorted array
// Returns pointer to matching element, or NULL if not found
void*
bsearch(const void *key, const void *base, uint nmemb, uint size,
        int (*compar)(const void *, const void *))
{
  const char *base_ptr = (const char *)base;
  uint left, right, mid;
  int cmp;

  if (base == 0 || key == 0 || compar == 0 || nmemb == 0)
    return 0;

  left = 0;
  right = nmemb;

  while (left < right) {
    mid = left + (right - left) / 2;
    cmp = compar(key, base_ptr + mid * size);

    if (cmp < 0)
      right = mid;
    else if (cmp > 0)
      left = mid + 1;
    else
      return (void *)(base_ptr + mid * size);
  }

  return 0; // Not found
}

// Helper function for qsort: swap two elements - MORE CAREFUL VERSION
static void
swap(char *a, char *b, uint size)
{
  uint i;
  char tmp;
  
  if (a == b)  // Don't swap with self
    return;
    
  for (i = 0; i < size; i++) {
    tmp = a[i];
    a[i] = b[i];
    b[i] = tmp;
  }
}

void
qsort(void *base, uint nmemb, uint size,
      int (*compar)(const void *, const void *))
{
  char *base_ptr = (char *)base;
  uint i, j, min_idx;

  if (base == 0 || compar == 0 || nmemb <= 1 || size == 0)
    return;

  // Selection sort
  for (i = 0; i < nmemb - 1; i++) {
    min_idx = i;
    for (j = i + 1; j < nmemb; j++) {
      if (compar(base_ptr + j * size, base_ptr + min_idx * size) < 0) {
        min_idx = j;
      }
    }
    if (min_idx != i) {
      swap(base_ptr + i * size, base_ptr + min_idx * size, size);
    }
  }
}
/*
// Lomuto partition scheme - simpler and more reliable
static int
lomuto_partition(char *base, uint nmemb, uint size,
                 int (*compar)(const void *, const void *))
{
  char *pivot;
  int i, j;
  
  if (nmemb <= 1)
    return 0;
  
  // Use last element as pivot
  pivot = base + (nmemb - 1) * size;
  i = -1;
  
  for (j = 0; j < (int)nmemb - 1; j++) {
    if (compar(base + j * size, pivot) <= 0) {
      i++;
      if (i != j)
        swap(base + i * size, base + j * size, size);
    }
  }
  
  // Place pivot in correct position
  i++;
  swap(base + i * size, pivot, size);
  
  return i;
}

// Quicksort implementation using Lomuto partition
void
qsort(void *base, uint nmemb, uint size,
      int (*compar)(const void *, const void *))
{
  char *base_ptr = (char *)base;
  int pivot_idx;

  if (base == 0 || compar == 0 || nmemb <= 1 || size == 0)
    return;

  // Partition the array
  pivot_idx = lomuto_partition(base_ptr, nmemb, size, compar);

  // Recursively sort left partition (before pivot)
  if (pivot_idx > 0)
    qsort(base_ptr, pivot_idx, size, compar);
  
  // Recursively sort right partition (after pivot)
  if (pivot_idx + 1 < (int)nmemb)
    qsort(base_ptr + (pivot_idx + 1) * size, nmemb - pivot_idx - 1, size, compar);
}*/
// Convert string to float
// Handles: whitespace, +/- signs, integer part, decimal point, 
// fractional part, and scientific notation (e/E)
float
atof(const char *nptr)
{
  float result = 0.0f;
  float fraction = 0.1f;
  int sign = 1;
  int has_dot = 0;
  int exponent = 0;
  int exp_sign = 1;
  int has_exp = 0;

  if (nptr == 0)
    return 0.0f;

  // Skip leading whitespace
  while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n')
    nptr++;

  // Handle sign
  if (*nptr == '+') {
    nptr++;
  } else if (*nptr == '-') {
    sign = -1;
    nptr++;
  }

  // Parse integer and fractional parts
  while (*nptr != '\0') {
    if (*nptr >= '0' && *nptr <= '9') {
      if (has_dot) {
        result += (*nptr - '0') * fraction;
        fraction *= 0.1f;
      } else {
        result = result * 10.0f + (*nptr - '0');
      }
    } else if (*nptr == '.') {
      has_dot = 1;
    } else if (*nptr == 'e' || *nptr == 'E') {
      has_exp = 1;
      nptr++;
      // Handle exponent sign
      if (*nptr == '+') {
        nptr++;
      } else if (*nptr == '-') {
        exp_sign = -1;
        nptr++;
      }
      // Parse exponent digits
      while (*nptr >= '0' && *nptr <= '9') {
        exponent = exponent * 10 + (*nptr - '0');
        nptr++;
      }
      break;
    } else {
      break;
    }
    nptr++;
  }

  // Apply sign
  result *= sign;

  // Apply exponent if present
  if (has_exp) {
    float exp_mult = 1.0f;
    int i;
    for (i = 0; i < exponent; i++) {
      if (exp_sign > 0)
        exp_mult *= 10.0f;
      else
        exp_mult *= 0.1f;
    }
    result *= exp_mult;
  }

  return result;
}

