import argparse
import time
from pathlib import Path

import cv2
import numpy as np
import yaml

from capture import ScreenCapture, WindowCapture
from capture.base import resize_frame
from transport.udp_sender import UdpFrameSender
from single_instance import acquire_single_instance


def test_pattern(width: int, height: int) -> np.ndarray:
    colors = (
        (255, 255, 255), (0, 255, 255), (255, 255, 0), (0, 255, 0),
        (255, 0, 255), (0, 0, 255), (255, 0, 0), (0, 0, 0),
    )
    frame = np.zeros((height, width, 3), dtype=np.uint8)
    stripe = width // len(colors)
    for index, color in enumerate(colors):
        frame[:, index * stripe:(index + 1) * stripe] = color
    cv2.putText(frame, "ESP32 RGB332", (56, height // 2), cv2.FONT_HERSHEY_SIMPLEX,
                0.7, (40, 40, 40), 2, cv2.LINE_AA)
    return frame


def create_source(config):
    source_type = config["stream"].get("source_type", "screen")
    if source_type == "window":
        return WindowCapture(config["stream"]["window_title"])
    if source_type == "region":
        return ScreenCapture(region=config["stream"]["region"])
    if source_type == "screen":
        return ScreenCapture(display_index=int(config["stream"].get("display_index", 1)))
    if source_type == "test":
        return None
    raise ValueError(f"unsupported source_type: {source_type}")


def main():
    acquire_single_instance()
    parser = argparse.ArgumentParser(description="Stream a Windows screen to ESP32-S3")
    parser.add_argument("--config", default="config.yaml")
    args = parser.parse_args()
    config_path = Path(args.config).resolve()
    config = yaml.safe_load(config_path.read_text(encoding="utf-8"))
    stream = config["stream"]
    width, height = int(stream["width"]), int(stream["height"])
    source = create_source(config)
    sender = UdpFrameSender(
        config["esp32"]["ip"], int(config["esp32"]["port"]), width, height,
        int(stream.get("lines_per_packet", 4)), int(stream.get("fps", 15)),
    )
    print(f"Streaming to {sender.address[0]}:{sender.address[1]} at {stream['fps']} FPS")
    try:
        while True:
            frame = test_pattern(width, height) if source is None else source.grab()
            frame = resize_frame(frame, width, height, stream.get("fit_mode", "fit"),
                                 float(stream.get("sharpen", 0.0)))
            sender.send_frame(frame)
    except KeyboardInterrupt:
        print("Stopped")
    finally:
        sender.close()
        if source:
            source.close()


if __name__ == "__main__":
    main()
