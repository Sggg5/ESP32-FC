# ATIAN-S3 hardware and firmware reference

This document is the source of truth for the custom ESP32-S3 board used by
this repository. For a step-by-step Chinese build guide, see
[`TUTORIAL_ZH-CN.md`](TUTORIAL_ZH-CN.md).

## Board requirements

- ESP32-S3 with 16 MB flash and PSRAM
- ILI9341 320x240 SPI LCD, no touch
- Analog two-axis joystick
- MAX98357A I2S amplifier and speaker
- INMP441 I2S microphone
- Optional SHT30 temperature/humidity sensor at I2C address `0x44`

The Retro-Go target is `RG_TARGET_ATIAN_S3`. The reusable Xiaozhi board
definition is included under [`xiaozhi-board/atian-s3`](../xiaozhi-board/atian-s3)
and is copied to `main/boards/atian-s3` in a Xiaozhi source tree.

## Wiring

All modules must share GND. Power each module according to its board marking;
the signal pins below use 3.3 V logic.

### ILI9341 LCD and SD card

| Signal | ESP32-S3 GPIO |
| --- | ---: |
| LCD SCK + SD SCK | 12 |
| LCD MOSI + SD MOSI | 11 |
| LCD MISO + SD MISO | 13 |
| LCD CS | 10 |
| LCD DC | 9 |
| LCD RST | 46 |
| LCD BL | 21 |
| SD CS | 7 |

Display settings: SPI2 at 40 MHz, `320x240`, ILI9341 BGR mode, MADCTL `0xA8`.
The current build stores games in internal flash; the SD socket is reserved but
not enabled in the stable configuration.

### Joystick and buttons

| Control | GPIO | Active state |
| --- | ---: | --- |
| Joystick X | 5 | ADC1 channel 4 |
| Joystick Y | 6 | ADC1 channel 5 |
| Joystick press / Start | 4 | Low |
| A / confirm / Xiaozhi talk | 0 | Low |
| Menu | 3 | Low |
| B / fire / back to Xiaozhi | 47 | Low |

The joystick direction thresholds are `0..800` and `3200..4096`. GPIO0 is a
boot strap pin: do not hold it while resetting unless download mode is wanted.

### Audio and microphone

MAX98357A output:

| Signal | GPIO |
| --- | ---: |
| DIN | 39 |
| BCLK | 40 |
| LRC / WS | 41 |

INMP441 input:

| Signal | GPIO |
| --- | ---: |
| SD / DOUT | 38 |
| SCK / BCLK | 40 |
| WS / LRC | 41 |

The microphone and amplifier share BCLK and WS. Both use a 32-bit stereo I2S
frame. Retro-Go's software output gain is `0.35`; Xiaozhi starts at 30 percent.

### SHT30

| Signal | GPIO |
| --- | ---: |
| SDA | 17 |
| SCL | 18 |
| Address | `0x44` |

## Controls

In Xiaozhi, click GPIO0 to start/stop a conversation and long-press GPIO0 to
open the game launcher. Voice tools recognize the game menu, Contra, Super
Contra, and Jackal. Wake-word processing is disabled in the current memory-safe
build.

In the launcher, use the joystick to move, GPIO0 to confirm, GPIO4 for Start,
GPIO47 for B/back, and GPIO3 for the emulator menu. An idle launcher or game
returns to Xiaozhi after about five minutes.

## Coexistence partition layout

The board uses 16 MB flash. Do not use offsets from older `rom0` experiments.

| Name | Offset | Size | Purpose |
| --- | ---: | ---: | --- |
| `nvs` | `0x009000` | `0x004000` | Wi-Fi and device settings |
| `otadata` | `0x00D000` | `0x002000` | Selected application |
| `xiaozhi_0` | `0x020000` | `0x3F0000` | Xiaozhi application slot A |
| `xiaozhi_1` | `0x410000` | `0x3F0000` | Xiaozhi OTA slot B |
| `assets` | `0x800000` | `0x200000` | Xiaozhi assets |
| `launcher` | `0xA00000` | `0x0C0000` | Game launcher |
| `retro-core` | `0xAC0000` | `0x100000` | NES emulator |
| `vfs` | `0xBC0000` | `0x440000` | FAT image containing ROMs/config |

Commercial ROMs, Wi-Fi credentials, activation data, NVS dumps, and private
Xiaozhi assets must not be committed to this repository.

## Helper scripts

```powershell
# Back up the complete board before changing partitions.
.\scripts\atian-backup-flash.ps1 -Port COM4

# Build launcher and NES core.
.\scripts\atian-build.ps1

# Update only the launcher and NES core. Xiaozhi/NVS/assets are preserved.
.\scripts\atian-flash-app.ps1 -Port COM4

# Build and flash VFS from a folder of user-supplied .nes files.
.\scripts\atian-flash-rom.ps1 -Port COM4 -RomDirectory C:\path\to\nes
```

The filenames in `launcher/main/applications.c` must match the files in the VFS
image. See the Chinese tutorial before adding or renaming games.
