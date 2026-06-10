import socket
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: python3 send_lidar_scan_file.py <scan.bin> [host] [port]")
        return 1

    scan_path = Path(sys.argv[1])
    host = sys.argv[2] if len(sys.argv) >= 3 else "127.0.0.1"
    port = int(sys.argv[3]) if len(sys.argv) >= 4 else 5000

    data = scan_path.read_bytes()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((host, port))
        sock.sendall(data)

    print(f"Sent {len(data)} bytes from {scan_path} to {host}:{port}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
