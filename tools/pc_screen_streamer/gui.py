import argparse
import ipaddress
import queue
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, ttk

import yaml

from capture.base import resize_frame
from capture.window_capture import list_windows
from main import create_source, test_pattern
from transport.udp_sender import UdpFrameSender
from single_instance import acquire_single_instance


SOURCE_LABELS = {
    "测试画面": "test",
    "整个屏幕": "screen",
    "指定窗口": "window",
    "指定区域": "region",
}
FIT_LABELS = {
    "完整显示（黑边）": "fit",
    "居中裁切": "crop",
    "拉伸铺满": "stretch",
}


class StreamWorker:
    def __init__(self, config, events):
        self.config = config
        self.events = events
        self.stop_event = threading.Event()
        self.thread = None
        self.capture_thread = None
        self.capture_request = threading.Event()
        self.frame_ready = threading.Event()
        self.frame_lock = threading.Lock()
        self.latest_frame = None
        self.capture_error = None

    def start(self):
        self.thread = threading.Thread(target=self._run, name="pc-screen-stream", daemon=True)
        self.thread.start()

    def stop(self):
        self.stop_event.set()
        self.capture_request.set()
        self.frame_ready.set()

    def _capture_loop(self, width, height, stream):
        source = None
        try:
            source = create_source(self.config)
            while not self.stop_event.is_set():
                self.capture_request.wait()
                self.capture_request.clear()
                if self.stop_event.is_set():
                    break
                frame = source.grab()
                frame = resize_frame(frame, width, height, stream.get("fit_mode", "fit"),
                                     float(stream.get("sharpen", 0.0)))
                with self.frame_lock:
                    self.latest_frame = frame
                self.frame_ready.set()
        except Exception as exc:
            self.capture_error = exc
            self.stop_event.set()
            self.frame_ready.set()
        finally:
            if source:
                source.close()

    def _run(self):
        sender = None
        frames = 0
        report_started = time.monotonic()
        try:
            stream = self.config["stream"]
            width, height = int(stream["width"]), int(stream["height"])
            sender = UdpFrameSender(
                self.config["esp32"]["ip"], int(self.config["esp32"]["port"]),
                width, height, int(stream.get("lines_per_packet", 4)),
                int(stream.get("fps", 15)),
            )
            is_test = stream.get("source_type") == "test"
            if is_test:
                test_frame = test_pattern(width, height)
            else:
                self.capture_thread = threading.Thread(
                    target=self._capture_loop, args=(width, height, stream),
                    name="pc-screen-capture", daemon=True,
                )
                self.capture_thread.start()
                self.capture_request.set()
            self.events.put(("started", None))
            while not self.stop_event.is_set():
                if is_test:
                    frame = test_frame
                else:
                    if not self.frame_ready.wait(timeout=1.0):
                        continue
                    if self.capture_error:
                        raise self.capture_error
                    if self.stop_event.is_set():
                        break
                    with self.frame_lock:
                        frame = self.latest_frame
                    self.frame_ready.clear()
                    self.capture_request.set()
                sender.send_frame(frame)
                frames += 1
                now = time.monotonic()
                elapsed = now - report_started
                if elapsed >= 1.0:
                    self.events.put(("stats", frames / elapsed))
                    frames = 0
                    report_started = now
        except Exception as exc:
            self.events.put(("error", str(exc)))
        finally:
            if sender:
                sender.close()
            self.stop_event.set()
            self.capture_request.set()
            if self.capture_thread:
                self.capture_thread.join(timeout=2.0)
            self.events.put(("stopped", None))


class StreamerGui:
    def __init__(self, root, config_path):
        self.root = root
        self.config_path = config_path
        self.config = self._load_config()
        self.events = queue.Queue()
        self.worker = None

        root.title("ESP32 电脑串流")
        root.geometry("620x590")
        root.minsize(590, 560)
        root.protocol("WM_DELETE_WINDOW", self._close)

        style = ttk.Style(root)
        if "vista" in style.theme_names():
            style.theme_use("vista")
        style.configure("Title.TLabel", font=("Microsoft YaHei UI", 16, "bold"))
        style.configure("Status.TLabel", font=("Microsoft YaHei UI", 11, "bold"))

        self.ip_var = tk.StringVar(value=str(self.config["esp32"].get("ip", "")))
        self.port_var = tk.StringVar(value=str(self.config["esp32"].get("port", 8888)))
        self.source_var = tk.StringVar(value=self._label_for(
            SOURCE_LABELS, self.config["stream"].get("source_type", "test")))
        self.display_var = tk.IntVar(value=int(self.config["stream"].get("display_index", 1)))
        self.window_var = tk.StringVar(value=str(self.config["stream"].get("window_title", "")))
        self.fit_var = tk.StringVar(value=self._label_for(
            FIT_LABELS, self.config["stream"].get("fit_mode", "fit")))
        self.fps_var = tk.IntVar(value=int(self.config["stream"].get("fps", 15)))
        self.sharpen_var = tk.DoubleVar(value=float(self.config["stream"].get("sharpen", 0.6)))
        region = self.config["stream"].get("region") or {}
        self.region_vars = {
            key: tk.IntVar(value=int(region.get(key, default)))
            for key, default in (("left", 0), ("top", 0), ("width", 1280), ("height", 720))
        }
        self.status_var = tk.StringVar(value="未启动")
        self.stats_var = tk.StringVar(value="发送 FPS：0.0    输出：320 × 240 RGB332")

        self._build_ui()
        self._refresh_windows(show_error=False)
        self._source_changed()
        self.root.after(100, self._poll_events)

    @staticmethod
    def _label_for(mapping, value):
        return next((label for label, item in mapping.items() if item == value), next(iter(mapping)))

    def _load_config(self):
        return yaml.safe_load(self.config_path.read_text(encoding="utf-8"))

    def _build_ui(self):
        outer = ttk.Frame(self.root, padding=18)
        outer.pack(fill="both", expand=True)

        ttk.Label(outer, text="ESP32 电脑串流", style="Title.TLabel").pack(anchor="w")
        ttk.Label(outer, text="将Windows屏幕、窗口或区域发送到ESP32-S3").pack(anchor="w", pady=(2, 14))

        connection = ttk.LabelFrame(outer, text="设备连接", padding=12)
        connection.pack(fill="x")
        ttk.Label(connection, text="ESP32 IP").grid(row=0, column=0, sticky="w")
        ttk.Entry(connection, textvariable=self.ip_var, width=22).grid(row=1, column=0, sticky="ew", padx=(0, 12))
        ttk.Label(connection, text="UDP端口").grid(row=0, column=1, sticky="w")
        ttk.Entry(connection, textvariable=self.port_var, width=10).grid(row=1, column=1, sticky="w")
        connection.columnconfigure(0, weight=1)

        capture = ttk.LabelFrame(outer, text="画面来源", padding=12)
        capture.pack(fill="x", pady=12)
        ttk.Label(capture, text="来源").grid(row=0, column=0, sticky="w")
        self.source_box = ttk.Combobox(capture, textvariable=self.source_var,
                                       values=list(SOURCE_LABELS), state="readonly", width=18)
        self.source_box.grid(row=1, column=0, sticky="ew", padx=(0, 10))
        self.source_box.bind("<<ComboboxSelected>>", lambda _: self._source_changed())

        ttk.Label(capture, text="缩放方式").grid(row=0, column=1, sticky="w")
        ttk.Combobox(capture, textvariable=self.fit_var, values=list(FIT_LABELS),
                     state="readonly", width=20).grid(row=1, column=1, sticky="ew")

        self.screen_frame = ttk.Frame(capture)
        ttk.Label(self.screen_frame, text="显示器编号").pack(side="left")
        ttk.Spinbox(self.screen_frame, from_=1, to=16, textvariable=self.display_var,
                    width=6).pack(side="left", padx=8)

        self.window_frame = ttk.Frame(capture)
        ttk.Label(self.window_frame, text="窗口标题").pack(side="left")
        self.window_box = ttk.Combobox(self.window_frame, textvariable=self.window_var, width=43)
        self.window_box.pack(side="left", fill="x", expand=True, padx=8)
        ttk.Button(self.window_frame, text="刷新", command=self._refresh_windows).pack(side="left")

        self.region_frame = ttk.Frame(capture)
        for index, (key, label) in enumerate((("left", "左"), ("top", "上"),
                                               ("width", "宽"), ("height", "高"))):
            ttk.Label(self.region_frame, text=label).grid(row=0, column=index * 2, padx=(0, 3))
            ttk.Entry(self.region_frame, textvariable=self.region_vars[key], width=7).grid(
                row=0, column=index * 2 + 1, padx=(0, 9))

        capture.columnconfigure(0, weight=1)
        capture.columnconfigure(1, weight=1)

        quality = ttk.LabelFrame(outer, text="串流参数", padding=12)
        quality.pack(fill="x")
        ttk.Label(quality, text="帧率").grid(row=0, column=0, sticky="w")
        ttk.Spinbox(quality, from_=5, to=30, textvariable=self.fps_var, width=7).grid(
            row=1, column=0, sticky="w")
        ttk.Label(quality, text="320 × 240  ·  RGB332  ·  每包4行",
                  foreground="#52606d").grid(row=1, column=1, sticky="w", padx=18)
        ttk.Label(quality, text="锐化").grid(row=0, column=2, sticky="w")
        ttk.Scale(quality, from_=0.0, to=1.5, variable=self.sharpen_var,
                  orient="horizontal", length=150).grid(row=1, column=2, sticky="w")

        status = ttk.LabelFrame(outer, text="运行状态", padding=12)
        status.pack(fill="x", pady=12)
        ttk.Label(status, textvariable=self.status_var, style="Status.TLabel").pack(anchor="w")
        ttk.Label(status, textvariable=self.stats_var).pack(anchor="w", pady=(6, 0))

        actions = ttk.Frame(outer)
        actions.pack(fill="x", pady=(2, 0))
        self.start_button = ttk.Button(actions, text="开始串流", command=self._start)
        self.start_button.pack(side="left", fill="x", expand=True)
        self.stop_button = ttk.Button(actions, text="停止", command=self._stop, state="disabled")
        self.stop_button.pack(side="left", fill="x", expand=True, padx=(10, 0))
        ttk.Button(outer, text="保存配置", command=self._save_only).pack(anchor="e", pady=(12, 0))

    def _source_changed(self):
        for frame in (self.screen_frame, self.window_frame, self.region_frame):
            frame.grid_forget()
        source = SOURCE_LABELS[self.source_var.get()]
        target = {"screen": self.screen_frame, "window": self.window_frame,
                  "region": self.region_frame}.get(source)
        if target:
            target.grid(row=2, column=0, columnspan=2, sticky="ew", pady=(12, 0))

    def _refresh_windows(self, show_error=True):
        try:
            titles = list_windows()
            self.window_box["values"] = titles
            if not self.window_var.get() and titles:
                self.window_var.set(titles[0])
        except Exception as exc:
            if show_error:
                messagebox.showerror("读取窗口失败", str(exc))

    def _collect_config(self):
        ip = self.ip_var.get().strip()
        ipaddress.ip_address(ip)
        port = int(self.port_var.get())
        if not 1 <= port <= 65535:
            raise ValueError("UDP端口必须在1到65535之间")
        fps = int(self.fps_var.get())
        source_type = SOURCE_LABELS[self.source_var.get()]
        if source_type == "window" and not self.window_var.get().strip():
            raise ValueError("请选择或输入窗口标题")
        region = {key: value.get() for key, value in self.region_vars.items()}
        if source_type == "region" and (region["width"] <= 0 or region["height"] <= 0):
            raise ValueError("区域宽度和高度必须大于0")
        return {
            "esp32": {"ip": ip, "port": port},
            "stream": {
                "source_type": source_type,
                "display_index": self.display_var.get(),
                "window_title": self.window_var.get().strip(),
                "region": region if source_type == "region" else None,
                "width": 320,
                "height": 240,
                "fps": fps,
                "color_mode": "RGB332",
                "fit_mode": FIT_LABELS[self.fit_var.get()],
                "sharpen": round(self.sharpen_var.get(), 2),
                "lines_per_packet": 4,
            },
        }

    def _write_config(self, config):
        self.config_path.write_text(
            yaml.safe_dump(config, allow_unicode=True, sort_keys=False), encoding="utf-8"
        )

    def _save_only(self):
        try:
            self.config = self._collect_config()
            self._write_config(self.config)
            self.status_var.set("配置已保存")
        except Exception as exc:
            messagebox.showerror("配置错误", str(exc))

    def _start(self):
        try:
            self.config = self._collect_config()
            self._write_config(self.config)
        except Exception as exc:
            messagebox.showerror("无法开始", str(exc))
            return
        self.worker = StreamWorker(self.config, self.events)
        self.worker.start()
        self.start_button.configure(state="disabled")
        self.stop_button.configure(state="normal")
        self.status_var.set("正在启动…")

    def _stop(self):
        if self.worker:
            self.worker.stop()
            self.status_var.set("正在停止…")
            self.stop_button.configure(state="disabled")

    def _poll_events(self):
        try:
            while True:
                event, value = self.events.get_nowait()
                if event == "started":
                    self.status_var.set(f"正在串流到 {self.ip_var.get()}:{self.port_var.get()}")
                elif event == "stats":
                    self.stats_var.set(f"发送 FPS：{value:.1f}    输出：320 × 240 RGB332")
                elif event == "error":
                    self.status_var.set("串流出错")
                    messagebox.showerror("串流出错", value)
                elif event == "stopped":
                    self.worker = None
                    self.start_button.configure(state="normal")
                    self.stop_button.configure(state="disabled")
                    if self.status_var.get() != "串流出错":
                        self.status_var.set("已停止")
        except queue.Empty:
            pass
        self.root.after(100, self._poll_events)

    def _close(self):
        if self.worker:
            self.worker.stop()
        self.root.destroy()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default="config.yaml")
    parser.add_argument("--smoke-test", action="store_true")
    parser.add_argument("--autostart", action="store_true")
    args = parser.parse_args()
    root = tk.Tk()
    root.withdraw()
    try:
        acquire_single_instance()
    except RuntimeError as exc:
        messagebox.showerror("电脑串流已在运行", str(exc))
        root.destroy()
        return
    app = StreamerGui(root, Path(args.config).resolve())
    if args.smoke_test:
        root.update_idletasks()
        root.destroy()
        return
    root.deiconify()
    if args.autostart:
        root.after(300, app._start)
    root.mainloop()


if __name__ == "__main__":
    main()
