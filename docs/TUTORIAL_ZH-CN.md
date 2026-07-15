# ESP32-S3 小智 + NES 模拟器复现教程

这套固件把小智语音助手、天气待机界面和 Retro-Go NES 模拟器放在同一块
ESP32-S3 上。小智和模拟器分别运行在独立的应用分区中，通过重启切换，互不
抢占运行内存。

> 本项目不提供商业游戏 ROM、Wi-Fi 密码、设备激活信息或私人服务器密钥。
> 请仅使用你有权使用的 ROM。第一次烧录前务必备份整块 Flash。

## 1. 成品功能

- 小智语音对话，GPIO0 单击开始/结束对话
- 长按 GPIO0 打开游戏列表
- 可说“打开游戏”“玩魂斗罗”“玩超级魂斗罗”或“玩赤色要塞”
- NES 游戏列表、声音、存档和模拟器菜单
- 模拟器约 5 分钟无操作后自动返回小智
- 待机显示慈溪室外天气和 SHT30 室内温湿度
- 室外天气约 30 分钟更新一次

当前为了保持内存稳定，唤醒词功能关闭，需要按 GPIO0 开始对话。

## 2. 材料

- ESP32-S3 开发板，要求 16 MB Flash 和 PSRAM
- ILI9341 320x240 SPI 彩屏，不带触摸
- 双轴模拟摇杆模块
- 3 个独立按键（GPIO0 通常使用开发板自带 BOOT 键）
- MAX98357A I2S 功放和小扬声器
- INMP441 I2S 麦克风
- SHT30 温湿度模块（可选）
- 面包板、杜邦线和稳定的 USB 供电

## 3. 接线

所有模块必须共地。ESP32-S3 信号电平为 3.3 V，不要给 GPIO 输入 5 V。

### 彩屏和 SD 卡

| 模块引脚 | ESP32-S3 |
| --- | ---: |
| LCD SCK + SD SCK | GPIO12 |
| LCD MOSI + SD MOSI | GPIO11 |
| LCD MISO + SD MISO | GPIO13 |
| LCD CS | GPIO10 |
| LCD DC | GPIO9 |
| LCD RST | GPIO46 |
| LCD BL | GPIO21 |
| SD CS | GPIO7 |

当前稳定版的游戏放在芯片内部 Flash，SD 卡暂不参与读取。LCD 和 SD 的 SPI
信号仍按上表预留。

### 摇杆和按键

| 功能 | ESP32-S3 |
| --- | ---: |
| 摇杆 X | GPIO5 |
| 摇杆 Y | GPIO6 |
| 摇杆 SW / Start | GPIO4 |
| A / 确定 / 小智对话 | GPIO0 |
| 模拟器菜单 | GPIO3 |
| B / 开枪 / 返回小智 | GPIO47 |

按键另一端接 GND，固件使用内部上拉。GPIO0 是下载启动脚，正常开机时不要一直
按住。

### MAX98357A 功放

| MAX98357A | ESP32-S3 |
| --- | ---: |
| DIN | GPIO39 |
| BCLK | GPIO40 |
| LRC / WS | GPIO41 |
| GND | GND |
| VIN | 按模块标注供电 |

### INMP441 麦克风

| INMP441 | ESP32-S3 |
| --- | ---: |
| SD / DOUT | GPIO38 |
| SCK / BCLK | GPIO40 |
| WS / LRC | GPIO41 |
| VDD | 3V3 |
| GND | GND |

功放和麦克风共用 GPIO40、GPIO41 的 I2S 时钟。INMP441 的 L/R 选择脚按模块
要求固定到 GND 或 3V3，不能悬空。

### SHT30

| SHT30 | ESP32-S3 |
| --- | ---: |
| SDA | GPIO17 |
| SCL | GPIO18 |
| VIN | 3V3 |
| GND | GND |

默认 I2C 地址为 `0x44`。

## 4. 准备开发环境

本机验证环境是 Windows PowerShell 和 ESP-IDF 6.0.1。安装 ESP-IDF 后克隆
本仓库，并准备单独的 `xiaozhi-esp32` 源码目录。

```powershell
git clone https://github.com/Sggg5/ESP32-FC.git
cd ESP32-FC
```

仓库里的 Retro-Go 自定义目标位于：

```text
components/retro-go/targets/atian-s3/config.h
```

仓库同时提供了本机验证过的小智板级文件：

```text
xiaozhi-board/atian-s3/
```

将该目录复制到小智工程的 `main/boards/atian-s3/`，并在小智工程的板型配置中
选择 `atian-s3`。其中包含统一的手环风菜单、天气和网络收音机界面。

小智工程包含个人服务配置时，请在公开前删除密钥、Wi-Fi、NVS 和编译产物。

## 5. 第一次操作先备份

把串口号替换成设备管理器里看到的端口：

```powershell
.\scripts\atian-backup-flash.ps1 -Port COM4
```

脚本会读取完整 16 MB Flash。恢复整机备份的命令是：

```powershell
esptool.py --chip esp32s3 --port COM4 write_flash 0x0 .\atian-s3-backup-日期时间.bin
```

备份包含 Wi-Fi 和设备激活数据，只能自己保存，不要上传 GitHub。

## 6. 编译模拟器

```powershell
.\scripts\atian-build.ps1
```

成功后得到：

```text
launcher/build/launcher.bin
retro-core/build/retro-core.bin
```

已经安装共存分区的板子，只更新模拟器时运行：

```powershell
.\scripts\atian-flash-app.ps1 -Port COM4
```

该脚本只写 `0xA00000` 和 `0xAC0000`，不会覆盖小智、NVS、assets 或游戏。

## 7. 添加自己的 NES 游戏

当前稳定方案使用固定游戏列表，避免 ESP32 在启动器扫描文件时耗尽内部内存。
因此文件名和列表必须完全一致。

1. 将你自己的 `.nes` 文件放入一个单独文件夹。
2. 文件名尽量使用短的 ASCII 名称，例如 `My_Game.nes`。
3. 编辑 `launcher/main/applications.c` 中的 `flash_nes_roms[]`。
4. 每一项必须与实际 `.nes` 文件名逐字一致，包括大小写。
5. ROM 总大小建议不超过约 3.8 MB。

示例：

```c
static const char *flash_nes_roms[] = {
    "My_Game.nes",
    "Another_Game.nes",
};
```

重新编译启动器，再生成并烧录 VFS：

```powershell
.\scripts\atian-build.ps1
.\scripts\atian-flash-app.ps1 -Port COM4
.\scripts\atian-flash-rom.ps1 -Port COM4 -RomDirectory C:\NES-ROMs
```

只想生成镜像、不立刻烧板子时增加 `-NoFlash`。

不要直接把 ROM 写到旧教程中的 `0x220000` 或 `0x230000`。这些地址现在属于
小智应用分区，会导致小智损坏、黑屏或反复重启。

## 8. 首次烧录共存固件

只有在你已经编译好小智、模拟器和 VFS 镜像后才执行本节。先确认小智 build
目录包含：

```text
bootloader/bootloader.bin
ota_data_initial.bin
xiaozhi.bin
generated_assets.bin
```

共存分区表由本仓库的 launcher 构建产生，脚本不会采用普通小智工程的默认
分区表。

然后运行：

```powershell
.\scripts\atian-flash-coexist.ps1 `
  -Port COM4 `
  -XiaozhiBuildDirectory C:\xiaozhi-esp32\build `
  -VfsImage .\flashfs_vfs_coexist.img
```

如果要保留自己的原始 `assets.bin`，增加：

```powershell
  -AssetsBin C:\path\to\assets.bin
```

首次完整烧录可能需要重新配网和激活。不要在没有备份时执行 `erase_flash`。

## 9. 使用方法

### 小智界面

- 单击 GPIO0：开始或结束语音对话
- 长按 GPIO0：打开游戏启动器
- 语音：“打开游戏”进入列表
- 语音：“玩魂斗罗”“玩超级魂斗罗”“玩赤色要塞”直接启动指定游戏

### 游戏启动器

- 摇杆上下：选择游戏
- GPIO0：确定 / NES A
- GPIO47：返回 / NES B / 开枪
- GPIO4：Start
- GPIO3：模拟器菜单
- 根列表按 GPIO47：返回小智

## 10. 常见问题

### 屏幕白屏、彩条或方向错误

先检查 CS、DC、SCK、MOSI 和 RST。稳定配置是 40 MHz、BGR、MADCTL
`0xA8`。杜邦线过长或接触不良时，先缩短线再降低 SPI 频率排查。

### 能看见游戏，但选择后卡住

检查 `flash_nes_roms[]` 与 VFS 内文件名是否完全相同，并确认烧录的是 `.nes`
文件而不是外层 `.zip`。串口日志中若找不到文件，重新生成 VFS 镜像。

### 游戏运行几秒后卡住

摇杆 ADC 在临界值附近持续抖动会产生连续按键。确认 X/Y 中位值稳定，并避免
在 GPIO5、GPIO6 上接入其他负载。不要开启逐帧串口日志。

### 小智有文字但没有声音

先确认模拟器能出声，以排除功放和扬声器。随后检查 GPIO39/40/41、I2S 格式和
小智音频任务是否正常。功放启动“叮”一声只能证明输出硬件基本连通。

### 小智反复重启

查看串口中是否有 `stack overflow`、`failed to allocate` 或 MQTT/AES 分配失败。
当前稳定版把 Opus 和天气任务栈放入 PSRAM，并关闭唤醒词，不能随意恢复大型
字体和高内存天气界面。

### SHT30 没有温湿度

检查 SDA=GPIO17、SCL=GPIO18、地址 `0x44` 和共地。串口应周期性出现 SHT30
读数；传感器读取失败时，先用 I2C 扫描确认地址。

### 室外天气不显示

天气接口需要 Wi-Fi。开机约 20 秒后首次获取，此后约 30 分钟更新。接口不可用
时室内 SHT30 仍可独立显示。

## 11. 发布教程时的合规清单

- 可以公开：源码、接线、构建脚本、分区表和不含秘密的配置示例
- 不要公开：商业 ROM、整机 Flash 备份、NVS、Wi-Fi 密码和设备激活信息
- 不要公开：私人服务地址、访问令牌和带账号信息的 `sdkconfig`
- 发布预编译固件前，使用空白设备验证一次首次配网流程
- 标明项目基于 Xiaozhi ESP32 与 Retro-Go，并保留原项目许可证和署名

## 12. 项目来源

- [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)
- [ducalex/retro-go](https://github.com/ducalex/retro-go)
- 本硬件适配仓库：[Sggg5/ESP32-FC](https://github.com/Sggg5/ESP32-FC)
