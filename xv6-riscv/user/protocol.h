#ifndef PROTOCOL_H
#define PROTOCOL_H

// Packet Types
#define TYPE_META_REQ  1  // Client asks for file info
#define TYPE_META_RES  2  // Server sends file info
#define TYPE_DATA_REQ  3  // Client asks for a specific block
#define TYPE_DATA_RES  4  // Server sends the block
#define TYPE_ERROR     5  // Something went wrong

// Constants
#define BLOCK_SIZE     1024  // Data bytes per packet
#define HEADER_SIZE    9     // 1 byte type + 4 bytes seq + 4 bytes len
#define SERVER_PORT    9999  // Port python server listens on
#define CLIENT_PORT    1234  // Port xv6 listens on

// Structure Layout (Logical View - we will pack/unpack manually)
// [ 1 byte Type ] [ 4 bytes Sequence Num ] [ 4 bytes Payload Len ] [ ... Data ... ]

#endif