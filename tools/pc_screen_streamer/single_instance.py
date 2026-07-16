import atexit
import ctypes
import sys


_handle = None


def acquire_single_instance():
    global _handle
    if sys.platform != "win32" or _handle:
        return
    kernel32 = ctypes.windll.kernel32
    _handle = kernel32.CreateMutexW(None, False, "Local\\XiaozhiEsp32ScreenStreamer")
    if not _handle:
        raise RuntimeError("无法创建程序实例锁")
    if kernel32.GetLastError() == 183:
        kernel32.CloseHandle(_handle)
        _handle = None
        raise RuntimeError("已有一个电脑串流程序正在运行，请先关闭旧窗口或旧命令行")
    atexit.register(lambda: kernel32.CloseHandle(_handle) if _handle else None)
