#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  float a = 1.5f, b = 2.0f;
  float c = a * b + 0.25f;    // Expected result = 3.25
  int *p = (int*)&c;          // Interpret float bits as int for printing

  printf("Initial result (parent): 0x%x\n", *p);

  int pid = fork();
  if (pid == 0) {
    // Child performs different FP computation
    float x = 2.5f, y = 4.0f;
    float z = x / y + 1.0f;   // Expected result = 1.625
    int *q = (int*)&z;
    printf("Child result: 0x%x\n", *q);
    exit(0);
  } else {
    wait(0);
    // Parent does another FP computation to verify FPU context restore
    float r = 5.5f, s = 1.1f;
    float t = r - s;          // Expected result = 4.4
    int *u = (int*)&t;
    printf("Final result (parent after child): 0x%x\n", *u);
  }

  exit(0);
}
