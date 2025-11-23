#include "kernel/types.h"
#include "user/user.h"
#include "user/udp_client.h"
#include "user/protocol.h"

int
main(int argc, char *argv[])
{
  int file_size = 0;
  
  printf("----------------------------------------\n");
  printf("TEST: Starting download of stories15M.bin\n");
  printf("----------------------------------------\n");

  // Call the wrapper function you implemented in udp_client.c
  char *buffer = fetch_model_weights(&file_size);

  if(buffer == 0) {
    printf("TEST FAILED: fetch_model_weights returned NULL.\n");
    printf("Possible causes:\n");
    printf("1. Python server not running\n");
    printf("2. Packet loss/Timeout\n");
    printf("3. SHA-256 Checksum Mismatch\n");
    exit(1);
  }

  printf("\nSUCCESS: Downloaded %d bytes.\n", file_size);
  
  // Optional: Print the address where the file is stored in heap
  printf("Buffer located at memory address: %p\n", buffer);

  // Free the memory to prevent leaks (though OS cleans up on exit)
  free(buffer);

  // Download tokenizer.bin
  printf("\n----------------------------------------\n");
  printf("TEST: Starting download of tokenizer.bin\n");
  printf("----------------------------------------\n");

  int tokenizer_size = 0;
  char *tokenizer_buffer = fetch_tokenizer(&tokenizer_size);

  if(tokenizer_buffer == 0) {
    printf("TEST FAILED: fetch_tokenizer returned NULL.\n");
    printf("Possible causes:\n");
    printf("1. Python server not running\n");
    printf("2. Packet loss/Timeout\n");
    printf("3. SHA-256 Checksum Mismatch\n");
    exit(1);
  }

  printf("\nSUCCESS: Downloaded tokenizer.bin (%d bytes).\n", tokenizer_size);
  printf("Tokenizer buffer located at memory address: %p\n", tokenizer_buffer);

  // Free the memory to prevent leaks (though OS cleans up on exit)
  free(tokenizer_buffer);

  exit(0);
}