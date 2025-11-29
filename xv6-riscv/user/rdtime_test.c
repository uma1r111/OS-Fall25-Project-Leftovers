#include "kernel/types.h"
#include "user/user.h"

int main() {
    printf("Testing rdtime...\n");
    
    uint64 t1 = rdtime();
    printf("rdtime() = %d\n", (int)t1);
    
    // Do some work
    for (int i = 0; i < 100000; i++);
    
    uint64 t2 = rdtime();
    printf("rdtime() = %d\n", (int)t2);
    
    uint64 diff = t2 - t1;
    printf("Elapsed: %d ticks\n", (int)diff);
    
    if (diff > 0) {
        printf("SUCCESS: rdtime is working!\n");
    } else {
        printf("FAIL: rdtime not incrementing\n");
    }
    
    exit(0);
}