from pathlib import Path
import socket
import string
import time
import csv



if __name__ == "__main__":
    scan_video_path: Path = Path("/home/spectre/TEMP_lidar_output_mesh/recording")
    scan_video_csv_path : Path = scan_video_path / "index.csv"

    host: string = "127.0.0.1"
    port: int = 5000

    frames: list = []
    with open(scan_video_csv_path, "r", newline="") as file:
        reader = csv.reader(file)

        for row in reader:
            frames.append(row)

    scans: list = []
    for frame in frames:
        file_name: string = frame[2]

        scan_path: Path = scan_video_path / file_name
        scans.append(scan_path.read_bytes())

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((host, port))

        for scan_id, scan in enumerate(scans):
            sock.sendall(scan)
            
            print(f"Sent scan {scan_id}")

            time.sleep(0.2)
