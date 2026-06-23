#!/usr/bin/env python3

import socket
import struct
import threading
import time

import rclpy
from ackermann_msgs.msg import AckermannDrive
from rclpy.node import Node


class VehicleCommandReceiver(Node):

    def __init__(self):
        super().__init__('vehicle_command_receiver')

        self.host = self.declare_parameter('host', '0.0.0.0').value
        self.port = self.declare_parameter('port', 5001).value
        self.command_timeout = self.declare_parameter(
            'command_timeout', 0.25
        ).value

        self.publisher = self.create_publisher(
            AckermannDrive, '/car_control', 10
        )
        self.command_lock = threading.Lock()
        self.latest_command = (0.0, 0.0)
        self.latest_command_time = 0.0
        self.stopping = threading.Event()
        self.server_socket = None
        self.client_socket = None

        self.create_timer(0.05, self.publish_latest_command)
        self.receiver_thread = threading.Thread(
            target=self.receive_loop,
            daemon=True
        )
        self.receiver_thread.start()

        self.get_logger().info(
            f'Listening for vehicle commands on {self.host}:{self.port}'
        )

    @staticmethod
    def receive_exact(connection, byte_count):
        data = bytearray()
        while len(data) < byte_count:
            chunk = connection.recv(byte_count - len(data))
            if not chunk:
                return None
            data.extend(chunk)
        return bytes(data)

    def receive_loop(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket = server
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((self.host, self.port))
        server.listen(1)
        server.settimeout(0.5)

        while not self.stopping.is_set():
            try:
                connection, address = server.accept()
            except socket.timeout:
                continue
            except OSError:
                break

            self.client_socket = connection
            self.get_logger().info(f'Command client connected: {address}')

            try:
                while not self.stopping.is_set():
                    packet = self.receive_exact(connection, 8)
                    if packet is None:
                        break

                    speed, steering_angle = struct.unpack('<ff', packet)
                    with self.command_lock:
                        self.latest_command = (speed, steering_angle)
                        self.latest_command_time = time.monotonic()
            except OSError:
                pass
            finally:
                connection.close()
                self.client_socket = None
                with self.command_lock:
                    self.latest_command = (0.0, 0.0)
                    self.latest_command_time = 0.0
                self.get_logger().info('Command client disconnected')

    def publish_latest_command(self):
        with self.command_lock:
            speed, steering_angle = self.latest_command
            command_age = time.monotonic() - self.latest_command_time

        if command_age > self.command_timeout:
            speed = 0.0
            steering_angle = 0.0

        command = AckermannDrive()
        command.speed = float(speed)
        command.steering_angle = float(steering_angle)
        self.publisher.publish(command)

    def destroy_node(self):
        self.stopping.set()
        if self.client_socket is not None:
            self.client_socket.close()
        if self.server_socket is not None:
            self.server_socket.close()
        if self.receiver_thread.is_alive():
            self.receiver_thread.join(timeout=1.0)
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = VehicleCommandReceiver()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
