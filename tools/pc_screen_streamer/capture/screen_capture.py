import cv2
import mss
import numpy as np

from .base import CaptureSource


class ScreenCapture(CaptureSource):
    def __init__(self, display_index: int = 1, region=None):
        self.sct = mss.mss()
        if region:
            self.monitor = {
                "left": int(region["left"]),
                "top": int(region["top"]),
                "width": int(region["width"]),
                "height": int(region["height"]),
            }
        else:
            if display_index < 1 or display_index >= len(self.sct.monitors):
                raise ValueError(f"display_index must be 1..{len(self.sct.monitors) - 1}")
            self.monitor = self.sct.monitors[display_index]

    def grab(self) -> np.ndarray:
        bgra = np.asarray(self.sct.grab(self.monitor))
        return cv2.cvtColor(bgra, cv2.COLOR_BGRA2BGR)

    def close(self) -> None:
        self.sct.close()
