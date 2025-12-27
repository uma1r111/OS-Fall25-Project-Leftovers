import socket
import struct   
import os
import hashlib
import math

# Configuration
# Try both 0.0.0.0 and 127.0.0.1 - QEMU forwards to host
SERVER_IP = '0.0.0.0' # Listen on all interfaces
SERVER_PORT = 9999
BLOCK_SIZE = 1024

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# File Paths (Make sure these exist in your root folder!)
FILES = {
    'stories15M.bin': os.path.join(BASE_DIR, 'stories15M.bin'),
    'tokenizer.bin': os.path.join(BASE_DIR, 'tokenizer.bin')
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
    # Allow socket reuse to avoid binding issues
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind((SERVER_IP, SERVER_PORT))
        print(f"🚀 Server listening on {SERVER_IP}:{SERVER_PORT}")
    except OSError as e:
        print(f"❌ Failed to bind to {SERVER_IP}:{SERVER_PORT}: {e}")
        # Try binding to localhost instead
        try:
            sock.bind(('127.0.0.1', SERVER_PORT))
            print(f"🚀 Server listening on 127.0.0.1:{SERVER_PORT}")
        except OSError as e2:
            print(f"❌ Failed to bind to 127.0.0.1:{SERVER_PORT}: {e2}")
            return

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
    
    print("Waiting for requests...")
    print(f"Server socket info: {sock.getsockname()}")
    print("Ready to receive UDP packets on port", SERVER_PORT)
    
    # Test: Try to receive with a short timeout to see if socket is working
    sock.settimeout(1.0)
    try:
        test_data, test_addr = sock.recvfrom(1024)
        print(f"TEST: Received test packet from {test_addr}")
        sock.settimeout(None)  # Remove timeout
    except socket.timeout:
        print("No test packet (this is normal)")
        sock.settimeout(None)  # Remove timeout for blocking recv
    except Exception as e:
        print(f"Socket test error: {e}")
        sock.settimeout(None)
    
    while True:
        try:
            data, addr = sock.recvfrom(2048)
            print(f"Received {len(data)} bytes from {addr}")
            print(f"First 20 bytes (hex): {data[:20].hex()}")
            
            # Unpack Header: ! = Network Endian, B = uchar, I = uint, I = uint
            # Header: [Type (1)] [Seq_Num (4)] [Payload_Len (4)]
            if len(data) < 9:
                print(f"Packet too short: {len(data)} bytes (need at least 9)")
                continue
            
            msg_type, seq_num, payload_len = struct.unpack('!BII', data[:9])
            print(f"Parsed: type={msg_type}, seq={seq_num}, payload_len={payload_len}")
            
            if len(data) < 9 + payload_len:
                print(f"Packet incomplete: have {len(data)} bytes, need {9 + payload_len}")
                continue
                
            payload = data[9:9+payload_len].decode('utf-8', errors='ignore').strip('\x00')
            print(f"Payload: '{payload}'")
        except Exception as e:
            print(f"Error processing packet: {e}")
            continue

        if msg_type == 1: # METADATA REQUEST
            filename = payload
            print(f"✓ METADATA REQUEST for '{filename}' from {addr}")
            
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
                response = header + f_hash
                print(f"  Sending response: {len(response)} bytes (blocks={num_blocks}, size={f_size})")
                sock.sendto(response, addr)
                print(f"  Response sent to {addr}")
            else:
                print(f"  ✗ File '{filename}' not found!")
                sock.sendto(struct.pack('!BII', 5, 0, 0), addr) # Error

        elif msg_type == 3: # DATA REQUEST
            # Use the file that this client requested in their metadata request
            target_file = client_files.get(addr, 'stories15M.bin') # Default fallback
            print(f"✓ DATA REQUEST for block {seq_num} from {addr} (file: {target_file})")
            
            if target_file in file_data:
                start = seq_num * BLOCK_SIZE
                end = min(start + BLOCK_SIZE, len(file_data[target_file]))
                chunk = file_data[target_file][start:end]
                
                # Header: [TYPE_DATA_RES] [Block ID] [Data Len]
                header = struct.pack('!BII', 4, seq_num, len(chunk))
                response = header + chunk
                sock.sendto(response, addr)
                if seq_num % 100 == 0:
                    print(f"  Sent block {seq_num} ({len(chunk)} bytes)")
            else:
                print(f"  ✗ File '{target_file}' not found for block request!")
        else:
            print(f"  ? Unknown message type: {msg_type}")

if __name__ == '__main__':
    start_server()