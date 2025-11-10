// Standard library functions for xv6
// These functions provide essential utilities needed by llama2.c

// Memory allocation
void* calloc(uint nmemb, uint size);

// Searching and sorting
void* bsearch(const void *key, const void *base, uint nmemb, uint size,
              int (*compar)(const void *, const void *));
void qsort(void *base, uint nmemb, uint size,
           int (*compar)(const void *, const void *));

// String to number conversion
float atof(const char *nptr);

