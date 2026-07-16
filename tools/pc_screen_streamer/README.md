# ESP32 PC Screen Streamer

Windows screen, region, or visible window capture for the Xiaozhi ESP32-S3 firmware.

## Install

The easiest Windows option is to double-click `start_gui.bat`. On first launch it
creates `.venv`, installs the required packages, and opens the desktop interface.

To start the GUI manually:

```powershell
cd tools\pc_screen_streamer
py -m venv .venv
.\.venv\Scripts\pip install -r requirements.txt
.\.venv\Scripts\python gui.py --config config.yaml
```

The original command-line mode remains available:

```powershell
cd tools\pc_screen_streamer
.\.venv\Scripts\python main.py --config config.yaml
```

Set `esp32.ip` to the address shown on the ESP32 stream connection page. Start with
`source_type: test` to verify orientation and color. Supported sources are `screen`,
`window`, `region`, and `test`. Region format:

```yaml
region: {left: 100, top: 100, width: 1280, height: 720}
```

Supported fit modes are `fit`, `crop`, and `stretch`. Window capture uses the visible
client area through MSS. Minimized and fully covered windows cannot be captured.

## ESP32 operation

1. Press GPIO3 to open the 2x2 home menu.
2. Move the joystick to `游戏机`, then press the joystick switch.
3. Select `电脑串流` and press the joystick switch.
4. Copy the displayed IP address into `config.yaml` and start `main.py`.
5. Hold GPIO0 to stop streaming and return to the game page.

The ESP32 listens on UDP port 8888. The first complete frame transfers LCD ownership
from LVGL to the stream renderer. Stopping the PC leaves the last complete frame on
screen; holding GPIO0 remains available.

## Configuration

```yaml
esp32:
  ip: "192.168.1.100"
  port: 8888
stream:
  source_type: "window"  # screen, window, region, or test
  display_index: 1
  window_title: "地下城与勇士"
  region: null
  width: 320
  height: 240
  fps: 15
  color_mode: "RGB332"
  fit_mode: "fit"        # fit, crop, or stretch
  sharpen: 0.7            # 0 disables sharpening, 0.5-0.9 is recommended
  lines_per_packet: 4
```

Use `source_type: test` first when checking RGB order or screen orientation. Stop the
PC client with Ctrl+C. Windows Firewall must allow outbound UDP traffic to the ESP32.
