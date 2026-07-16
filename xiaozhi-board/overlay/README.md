# Xiaozhi PC screen streaming overlay

This overlay targets Xiaozhi 2.2.6 with ESP-IDF 5.5.4 and LVGL 9.5.0. It adds
the 320x240 RGB332 UDP receiver, complete-frame PSRAM buffering, direct ILI9341
rendering, LVGL ownership transfer, and the `电脑串流` game-page entry.

Install the board definition and overlay from the repository root:

```powershell
.\xiaozhi-board\install-overlay.ps1 -XiaozhiPath C:\xiaozhi-esp32
```

The installer copies the ATIAN-S3 board files, screen-stream modules, and the
matching `lcd_display` implementation, then inserts the idempotent CMake source
block. Review local Xiaozhi changes before running it against a different
upstream version.

Build normally with ESP-IDF, then flash only the Xiaozhi application partition
when preserving games and assets:

```powershell
python -m esptool --chip esp32s3 --port COM4 write_flash 0x20000 build\xiaozhi.bin
```

The Windows sender and GUI are in `tools/pc_screen_streamer`. Protocol details
are in `docs/screen_stream_protocol.md`.
