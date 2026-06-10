import socket
import sys


def main() -> int:
    host = "127.0.0.1"
    port = 5000
    message = "hello from python"

    if len(sys.argv) >= 2:
        message = sys.argv[1]

    if len(sys.argv) >= 3:
        host = sys.argv[2]

    if len(sys.argv) >= 4:
        port = int(sys.argv[3])

    data = message.encode("utf-8")

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((host, port))
        sock.sendall(data)

    print(f"Sent {len(data)} bytes to {host}:{port}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
