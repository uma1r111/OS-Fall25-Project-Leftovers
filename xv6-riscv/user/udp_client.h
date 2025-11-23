#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

// Standard xv6 types
typedef unsigned int uint;
typedef unsigned char uchar;

// Function Prototypes
char* fetch_model_weights(int *size_out);
char* fetch_tokenizer(int *size_out);

#endif