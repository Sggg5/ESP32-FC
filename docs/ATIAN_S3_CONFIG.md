# ATIAN-S3 configuration interface

This document records the hardware interface for the custom ESP32-S3 handheld
target in `components/retro-go/targets/atian-s3/config.h`.

## Build target

- Target name: `ATIAN-S3`
- Retro-Go build target: `RG_TARGET_ATIAN_S3`
- ESP-IDF target: `esp32s3`
- Tested ESP-IDF profile: `C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1`
- Default serial port used during bring-up: `COM4`

## LCD

ILI9341, SPI, no touch.

| Signal | GPIO |
| --- | ---: |
| SCK | 12 |
| MOSI | 11 |
| MISO | 13 |
| CS | 10 |
| DC | 9 |
| RST | 46 |
| BL | 21 |

Current display settings:

- Driver: `RG_SCREEN_DRIVER 0`
- Host: `SPI2_HOST`
- SPI speed: `SPI_MASTER_FREQ_40M`
- Size: `320x240`
- Backlight: static GPIO high through `GPIO21`
- Current MADCTL: `0xA8` (`MY | MV | BGR`)

Common MADCTL orientation values:

| Purpose | Value |
| --- | ---: |
| Current: Y mirrored from base landscape | `0xA8` |
| Base landscape used before Y mirror | `0x28` |
| X and Y mirrored landscape | `0xE8` |

If colors are wrong, keep `BGR` set first and only change orientation bits
(`MY`, `MX`, `MV`) one at a time.

## SD card SPI bus

The SD card shares the LCD SPI bus.

| Signal | GPIO |
| --- | ---: |
| SCK | 12 |
| MOSI | 11 |
| MISO | 13 |
| CS | 7 |

The current diagnostic firmware can boot a NES ROM directly from the `rom0`
flash partition, so the SD card path is not required for the single-ROM test.

## Joystick and buttons

ADC joystick:

| Control | GPIO | ADC |
| --- | ---: | --- |
| X | 5 | `ADC_UNIT_1`, `ADC_CHANNEL_4` |
| Y | 6 | `ADC_UNIT_1`, `ADC_CHANNEL_5` |

Current thresholds:

| Retro-Go key | ADC channel | Range |
| --- | --- | --- |
| Up | `ADC_CHANNEL_5` | `0..800` |
| Down | `ADC_CHANNEL_5` | `3200..4096` |
| Right | `ADC_CHANNEL_4` | `0..800` |
| Left | `ADC_CHANNEL_4` | `3200..4096` |

GPIO buttons:

| Button | GPIO | Active level |
| --- | ---: | ---: |
| Start | 4 | 0 |
| A | 0 | 0 |
| B | 37 | 0 |

## Audio

MAX98357A wiring reserved in the target config:

| MAX98357A signal | GPIO |
| --- | ---: |
| DIN | 39 |
| BCLK | 40 |
| LRC/WS | 41 |

Current state:

- `RG_AUDIO_USE_EXT_DAC` is `0`.
- The IDF 6 I2S driver path is present, but disabled for the main firmware.
- Enabling it currently stalls inside `i2s_new_channel()` before the driver
  returns, so the playable firmware falls back to the dummy audio sink.

## Flash partition layout

The custom partition table keeps a small raw ROM slot in flash before the VFS
area.

| Name | Type | Offset | Size |
| --- | --- | ---: | ---: |
| `launcher` | app | `0x10000` | `0x110000` |
| `retro-core` | app | `0x120000` | `0x100000` |
| `rom0` | data `0x40` | `0x220000` | `0x100000` |
| `vfs` | data `0x81` | `0x320000` | `0xB00000` |

For the current NES test build, flash one mapper-0 `.nes` file into `rom0`:

```powershell
python $env:IDF_PATH\components\esptool_py\esptool\esptool.py `
  --chip esp32s3 --port COM4 write_flash 0x220000 path\to\game.nes
```

Do not commit commercial ROM files to this repository.

## Build and flash

Build the app:

```powershell
.\scripts\atian-build.ps1
```

Flash only the app binary:

```powershell
.\scripts\atian-flash-app.ps1 -Port COM4
```

Flash a test ROM into the `rom0` partition:

```powershell
.\scripts\atian-flash-rom.ps1 -Port COM4 -RomPath path\to\game.nes
```

## Current bring-up notes

- Runtime serial logging is intentionally minimal; heavy per-frame or per-input
  logs can slow the emulator enough to look frozen.
- The current NES path copies the raw flash ROM partition into internal RAM
  before starting the emulator.
- VFS/launcher behavior is still diagnostic and should be restored before this
  becomes a general multi-game firmware.
