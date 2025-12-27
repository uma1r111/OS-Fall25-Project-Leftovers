#include "kernel/types.h"
#include "user/user.h"
#include "user/sha256.h"
#include "user/protocol.h"

#define TIMEOUT_CYCLES 10000000 

// Network Helper: Swap Endianness (Big <-> Little)
uint bswap(uint x) {
    return ((x >> 24) & 0xff) | ((x << 8) & 0xff0000) |
           ((x >> 8) & 0xff00) | ((x << 24) & 0xff000000);
}

// Serialize Header
void pack_header(uchar *buf, uchar type, uint seq, uint len) {
    buf[0] = type;
    uint n_seq = bswap(seq);
    uint n_len = bswap(len);
    memmove(buf + 1, &n_seq, 4); 
    memmove(buf + 5, &n_len, 4);
}

// Unpack Header
void unpack_header(uchar *buf, uchar *type, uint *seq, uint *len) {
    *type = buf[0];
    uint n_seq;
    uint n_len;
    memmove(&n_seq, buf + 1, 4);
    memmove(&n_len, buf + 5, 4);
    *seq = bswap(n_seq);
    *len = bswap(n_len);
}

// Print hex byte with zero-padding (xv6 printf doesn't support %02x)
void print_hex_byte(uchar b) {
    char digits[] = "0123456789abcdef";
    printf("%c", digits[(b >> 4) & 0xf]);
    printf("%c", digits[b & 0xf]);
}

// QEMU Host IP (10.0.2.2)
// This integer value (0x0A000202 = 167772674) is suitable for the 'int dst' argument in send()
int DST_IP = 0x0A000202; 


char* fetch_file(char *filename, int *size_out) {
    int sock_port = CLIENT_PORT; 
    
    if (bind(sock_port) < 0) {
        printf("Error: bind failed\n");
        return 0;
    }

    // 2. Request Metadata
    uchar req_buf[BLOCK_SIZE];
    pack_header(req_buf, TYPE_META_REQ, 0, strlen(filename));
    memmove(req_buf + HEADER_SIZE, filename, strlen(filename));

    // Send Request
    printf("Sending metadata request for: %s\n", filename);
    int send_result = send(sock_port, DST_IP, SERVER_PORT, (char*)req_buf, HEADER_SIZE + strlen(filename));
    if (send_result < 0) {
        printf("Error: send failed\n");
        return 0;
    }
    printf("Request sent, waiting for response...\n");

    // Receive Metadata Response with retry mechanism
    uchar res_buf[BLOCK_SIZE + 20];
    
    // --- FIX START: Use correct types for pointers ---
    uint32 src_ip;    // Changed from int to uint32
    uint16 src_port;  // Changed from short to uint16
    // -------------------------------------------------

    // Try receiving with multiple attempts (in case of packet loss)
    int n = -1;
    int attempts = 0;
    int max_attempts = 10;
    
    while (n < 0 && attempts < max_attempts) {
        // Send request again if this is a retry
        if (attempts > 0) {
            printf("Retrying request (attempt %d/%d)...\n", attempts + 1, max_attempts);
            send_result = send(sock_port, DST_IP, SERVER_PORT, (char*)req_buf, HEADER_SIZE + strlen(filename));
            if (send_result < 0) {
                printf("Error: retry send failed\n");
                return 0;
            }
        }
        
        // recv expects pointers to unsigned types
        n = recv(sock_port, &src_ip, &src_port, (char*)res_buf, BLOCK_SIZE);
        
        if (n < 0) {
            attempts++;
            // Small delay before retry (xv6 doesn't have usleep, but we can use pause)
            pause(100); // Pause for a short time
        }
    }
    
    printf("Received %d bytes after %d attempts\n", n, attempts + 1);
    if (n < 0) { 
        printf("Error: recv meta failed after %d attempts\n", max_attempts); 
        return 0; 
    }

    // Parse Metadata
    uchar type;
    uint total_blocks, file_size;
    unpack_header(res_buf, &type, &total_blocks, &file_size);

    if (type != TYPE_META_RES) {
        printf("Error: Invalid meta response type %d\n", type);
        return 0;
    }

    uchar expected_hash[32];
    memmove(expected_hash, res_buf + HEADER_SIZE, 32);

    printf("Starting download: %s (%d bytes, %d blocks)\n", filename, file_size, total_blocks);

    // 3. Allocate Memory
    char *file_buffer = malloc(file_size);
    if (!file_buffer) {
        printf("Error: Out of memory\n");
        return 0;
    }

    // 4. Download Loop
    for (int i = 0; i < total_blocks; i++) {
        int retry = 0;
        int received = 0;

        while (retry < 5 && !received) {
            // Send Request for Block i
            pack_header(req_buf, TYPE_DATA_REQ, i, 0);
            send(sock_port, DST_IP, SERVER_PORT, (char*)req_buf, HEADER_SIZE);

            // Wait for response
            // We reuse src_ip and src_port (now correctly typed)
            n = recv(sock_port, &src_ip, &src_port, (char*)res_buf, BLOCK_SIZE + HEADER_SIZE);

            if (n > 0) {
                uchar r_type;
                uint r_seq, r_len;
                unpack_header(res_buf, &r_type, &r_seq, &r_len);

                if (r_type == TYPE_DATA_RES && r_seq == i) {
                    memmove(file_buffer + (i * BLOCK_SIZE), res_buf + HEADER_SIZE, r_len);
                    received = 1;
                    if (i % 100 == 0) printf("."); 
                }
            }
            retry++;
        }

        if (!received) {
            printf("\nError: Failed to fetch block %d\n", i);
            free(file_buffer);
            return 0;
        }
    }
    printf("\nDownload complete. Verifying integrity...\n");

    // 5. Verify SHA-256
    uchar computed_hash[32];
    sha256_hash((uchar*)file_buffer, file_size, computed_hash);

    int mismatch = 0;
    for(int k=0; k<32; k++) {
        if(computed_hash[k] != expected_hash[k]) mismatch = 1;
    }

    if (mismatch) {
        printf("Error: Checksum mismatch!\n");
        printf("Computed hash: ");
        for(int k=0; k<32; k++) {
            print_hex_byte(computed_hash[k]);
        }
        printf("\n");
        printf("Expected hash: ");
        for(int k=0; k<32; k++) {
            print_hex_byte(expected_hash[k]);
        }
        printf("\n");
        free(file_buffer);
        return 0;
    }

    printf("Success: File verified.\n");
    *size_out = file_size;

    printf("Unbinding port %d\n", sock_port);
    if(unbind(sock_port) < 0) {
        printf("Error: unbind failed\n");
        return 0;
    }
    return file_buffer;
}

char* fetch_model_weights(int *size_out) {
    return fetch_file("stories15M.bin", size_out);
}

char* fetch_tokenizer(int *size_out) {
    return fetch_file("tokenizer.bin", size_out);
}