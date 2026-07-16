import win32gui

from .screen_capture import ScreenCapture


def list_windows():
    titles = []

    def visit(hwnd, _):
        if win32gui.IsWindowVisible(hwnd) and not win32gui.IsIconic(hwnd):
            title = win32gui.GetWindowText(hwnd).strip()
            if title:
                titles.append(title)

    win32gui.EnumWindows(visit, None)
    return sorted(set(titles), key=str.lower)


def _find_window(title_fragment: str):
    matches = []

    def visit(hwnd, _):
        if win32gui.IsWindowVisible(hwnd):
            title = win32gui.GetWindowText(hwnd)
            if title_fragment.lower() in title.lower():
                matches.append((hwnd, title))

    win32gui.EnumWindows(visit, None)
    if not matches:
        raise RuntimeError(f"window not found: {title_fragment!r}")
    return matches[0]


class WindowCapture(ScreenCapture):
    """Capture visible window bounds through MSS, avoiding PrintWindow black frames."""

    def __init__(self, title_fragment: str):
        self.hwnd, self.window_title = _find_window(title_fragment)
        region = self._client_region()
        super().__init__(region=region)

    def _client_region(self):
        if win32gui.IsIconic(self.hwnd):
            raise RuntimeError(f"window is minimized: {self.window_title}")
        left, top, right, bottom = win32gui.GetClientRect(self.hwnd)
        left, top = win32gui.ClientToScreen(self.hwnd, (left, top))
        right, bottom = win32gui.ClientToScreen(self.hwnd, (right, bottom))
        if right <= left or bottom <= top:
            raise RuntimeError(f"window is minimized or has an invalid client area: {self.window_title}")
        return {
            "left": left,
            "top": top,
            "width": right - left,
            "height": bottom - top,
        }

    def grab(self):
        self.monitor = self._client_region()
        return super().grab()
