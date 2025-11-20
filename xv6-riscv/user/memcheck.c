#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    printf("Starting memory check...\n");

    // allocate in chunks to avoid fragmentation
    const int chunk = 1024 * 1024; // 1 MB
    int total = 0;

    while (1) {
        char *p = malloc(chunk);
        if (p == 0) {
            printf("Allocation failed after %d MB\n", total / (1024*1024));
            break;
        }

        // touch memory so compiler doesn't eliminate malloc
        p[0] = 'A';

        total += chunk;
        printf("Allocated: %d MB\n", total / (1024*1024));
    }

    printf("Total memory available to user programs: %d MB\n", total / (1024*1024));

    exit(0);
}
