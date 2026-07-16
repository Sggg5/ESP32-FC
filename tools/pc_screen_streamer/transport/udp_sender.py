import socket
import time

import numpy as np

from .protocol import frame_packet, heartbeat_packet


class UdpFrameSender:
    def __init__(self, host: str, port: int, width: int, height: int,
                 lines_per_packet: int = 4, fps: int = 15):
        if height % lines_per_packet:
            raise ValueError("height must be divisible by lines_per_packet")
        self.address = (host, port)
        self.width = width
        self.height = height
        self.lines_per_packet = lines_per_packet
        self.frame_period = 1.0 / fps
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.frame_id = 0
        self.last_heartbeat = 0.0

    @staticmethod
    def bgr_to_rgb332(frame: np.ndarray) -> np.ndarray:
        b = frame[:, :, 0]
        g = frame[:, :, 1]
        r = frame[:, :, 2]
        return ((r & 0xE0) | ((g & 0xE0) >> 3) | (b >> 6)).astype(np.uint8)

    def send_frame(self, bgr_frame: np.ndarray) -> None:
        if bgr_frame.shape[:2] != (self.height, self.width):
            raise ValueError(f"expected {self.width}x{self.height} frame")
        started = time.perf_counter()
        rgb332 = self.bgr_to_rgb332(bgr_frame)
        chunks = self.height // self.lines_per_packet
        packet_gap = self.frame_period / chunks

        for chunk in range(chunks):
            y = chunk * self.lines_per_packet
            payload = rgb332[y:y + self.lines_per_packet].tobytes()
            self.socket.sendto(
                frame_packet(self.frame_id, y, self.lines_per_packet, payload),
                self.address,
            )
            target = started + (chunk + 1) * packet_gap
            delay = target - time.perf_counter()
            if delay > 0:
                time.sleep(delay)

        self.frame_id = (self.frame_id + 1) & 0xFFFF
        now = time.monotonic()
        if now - self.last_heartbeat >= 1.0:
            self.socket.sendto(heartbeat_packet(self.frame_id), self.address)
            self.last_heartbeat = now

    def close(self) -> None:
        self.socket.close()
