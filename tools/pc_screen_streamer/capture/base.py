from abc import ABC, abstractmethod

import cv2
import numpy as np


class CaptureSource(ABC):
    @abstractmethod
    def grab(self) -> np.ndarray:
        """Return a BGR image."""

    def close(self) -> None:
        pass


def _sharpen(frame: np.ndarray, amount: float) -> np.ndarray:
    if amount <= 0:
        return frame
    blurred = cv2.GaussianBlur(frame, (0, 0), 0.8)
    return cv2.addWeighted(frame, 1.0 + amount, blurred, -amount, 0)


def resize_frame(frame: np.ndarray, width: int, height: int, mode: str,
                 sharpen: float = 0.0) -> np.ndarray:
    source_h, source_w = frame.shape[:2]
    if mode == "stretch":
        return _sharpen(cv2.resize(frame, (width, height), interpolation=cv2.INTER_AREA), sharpen)

    scale = min(width / source_w, height / source_h) if mode == "fit" else max(
        width / source_w, height / source_h
    )
    resized_w = max(1, round(source_w * scale))
    resized_h = max(1, round(source_h * scale))
    resized = cv2.resize(frame, (resized_w, resized_h), interpolation=cv2.INTER_AREA)

    if mode == "fit":
        output = np.zeros((height, width, 3), dtype=np.uint8)
        x = (width - resized_w) // 2
        y = (height - resized_h) // 2
        output[y:y + resized_h, x:x + resized_w] = resized
        return _sharpen(output, sharpen)
    if mode == "crop":
        x = (resized_w - width) // 2
        y = (resized_h - height) // 2
        return _sharpen(resized[y:y + height, x:x + width], sharpen)
    raise ValueError(f"unsupported fit_mode: {mode}")
