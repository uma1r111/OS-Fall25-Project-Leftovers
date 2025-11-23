import socket
import struct
import os
import hashlib
import math

# Configuration
SERVER_IP = '0.0.0.0' # Listen on all interfaces
SERVER_PORT = 9999
BLOCK_SIZE = 1024

# File Paths (Make sure these exist in your root folder!)
FILES = {
    'stories15M.bin': 'stories15M.bin',
    'tokenizer.bin': 'tokenizer.bin'
}

def compute_sha256(filepath):
    print(f"Computing SHA256 for {filepath}...")
    sha256 = hashlib.sha256()
    with open(filepath, 'rb') as f:
        while True:
            data = f.read(65536)
            if not data: break
            sha256.update(data)
    return sha256.digest()

def start_server():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((SERVER_IP, SERVER_PORT))
    print(f"🚀 Server listening on {SERVER_IP}:{SERVER_PORT}")

    # Pre-load files into memory (Speed optimization)
    file_data = {}
    file_hashes = {}
    
    for fname, fpath in FILES.items():
        if os.path.exists(fpath):
            with open(fpath, 'rb') as f:
                file_data[fname] = f.read()
                file_hashes[fname] = hashlib.sha256(file_data[fname]).digest()
            print(f"Loaded {fname}: {len(file_data[fname])} bytes")
        else:
            print(f"⚠️ Warning: {fname} not found.")

    # Track which file each client is currently downloading
    client_files = {}
    
    while True:
        data, addr = sock.recvfrom(2048)
        
        # Unpack Header: ! = Network Endian, B = uchar, I = uint, I = uint
        # Header: [Type (1)] [Seq_Num (4)] [Payload_Len (4)]
        if len(data) < 9: continue
        
        msg_type, seq_num, payload_len = struct.unpack('!BII', data[:9])
        payload = data[9:].decode('utf-8', errors='ignore').strip('\x00')

        if msg_type == 1: # METADATA REQUEST
            filename = payload
            print(f"Request for {filename} from {addr}")
            
            if filename in file_data:
                # Track which file this client is downloading
                client_files[addr] = filename
                
                f_size = len(file_data[filename])
                f_hash = file_hashes[filename]
                num_blocks = math.ceil(f_size / BLOCK_SIZE)
                
                # Response: [TYPE_META_RES] [Total Blocks] [File Size] [32 bytes Hash]
                # Note: We re-purpose the header fields slightly for the Meta Response
                # Seq = Total Blocks, Payload_Len = File Size
                header = struct.pack('!BII', 2, num_blocks, f_size)
                sock.sendto(header + f_hash, addr)
            else:
                sock.sendto(struct.pack('!BII', 5, 0, 0), addr) # Error

        elif msg_type == 3: # DATA REQUEST
            # Use the file that this client requested in their metadata request
            target_file = client_files.get(addr, 'stories15M.bin') # Default fallback
            
            if target_file in file_data:
                start = seq_num * BLOCK_SIZE
                end = min(start + BLOCK_SIZE, len(file_data[target_file]))
                chunk = file_data[target_file][start:end]
                
                # Header: [TYPE_DATA_RES] [Block ID] [Data Len]
                header = struct.pack('!BII', 4, seq_num, len(chunk))
                sock.sendto(header + chunk, addr)

if __name__ == '__main__':
    start_server()