#include "kernel/types.h"
#include "user/user.h"

int main() {
  float a = 1.5f, b = 2.0f;
  float c = a * b + 0.25f;
  int *p = (int*)&c; // interpret float bits as int
  printf("float bits: 0x%x\n", *p);
  exit(0);
}

