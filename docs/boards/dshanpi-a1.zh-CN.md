# 百问网东山派 A1（100ASK DShanPi A1）

[English](dshanpi-a1.md) | 简体中文

**早期移植。** 能从 eMMC 启动到 UEFI 首页，HDMI 有画面（2026-11-25 第一次上板，板子是 8 GB LPDDR4X、64 GB eMMC）。通过 netboot.xyz 安装到 eMMC 的 Ubuntu 26.04（Linux 7.0）能启动并正常运行（2026-11-26）。

## 硬件

来自板子的主线设备树
[`rk3576-100ask-dshanpi-a1.dts`](../../devicetree/mainline/rk3576-100ask-dshanpi-a1.dts)：

| | |
|---|---|
| SoC、内存 | RK3576，LPDDR4X |
| PMIC | RK806，接在 I2C1 |
| 存储 | eMMC（8 位，支持 HS400）、microSD |
| 以太网 | 2 个千兆口，GMAC0 和 GMAC1 都是裕太 YT8521SC |
| PCIe | M.2 插槽，接 pcie0 |
| USB | USB-C（DRD0，VBUS 由 GPIO1 PC1 控制，没有 CC 控制器）、USB-A（DRD1，主机） |
| 显示 | HDMI |
| 音频 | ES8388，接在 I2C4 和 SAI2；HDMI 音频 |
| RTC | DS1338，接在 I2C2 |
| 按键 | MASKROM、RECOVERY、USER2 接 SARADC；USER1 接 GPIO0 PA0 |
| 串口 | UART0，1500000 8N1 |

## 现状

根据第一次启动的串口日志：

| | |
|---|---|
| 内存 | 8 GB LPDDR4X，2112 MHz |
| HDMI | 可用，3840x2160 60 Hz |
| eMMC | 可用，能从它启动；固件里跑 52 MHz High Speed、8 位。交给 Linux 的设备树把它限制在 HS200、最高 150 MHz：主线的 HS400 和 198 MHz 的 HS200 在安装系统时都出现写入错误；148.5 MHz 下写入并校验 16 GiB 没有错误 |
| 设置保存 | 保存在 eMMC 上，复位后仍在（用 "Save OS Variable Changes to eMMC" 确认过） |
| M.2 PCIe | 链路起来了，Gen2 x1；插槽里识别到一张 Intel AX210 无线网卡。NVMe 没试过 |
| USB-A | 控制器能启动，端口上识别到设备；U 盘和键盘没试过 |
| USB-C | VBUS 保持关闭：没有办法分辨接上的是设备还是充电器。交给 Linux 的设备树把 DRD0 设成 peripheral；用主线默认的 OTG 时 dwc3 会 SError 崩溃 |
| 以太网 GMAC0 | 可用：YT8521 PHY（0x0000011A），50 ms 连上，能拿到 DHCP 地址，DNS 和 NTP 也通；PXE 能经 TFTP 下载 1.1 MB 的 EFI 文件并运行（2026-11-26）。还没记下是两个网口中的哪一个 |
| 以太网 GMAC1 | 没有启用：GmacPlatformDxe 只驱动 GMAC0 |
| RK806 | 在 I2C1 上有应答（CHIP_NAME 0x80）；通过它关机还没确认 |
| SD 卡 | 没试过 |
| RTC | 没有 DS1338 驱动：断电后时间不保留 |
| 音频、按键 | 固件不使用 |
| Linux | 安装程序能启动：eMMC、PCIe、HDMI、USB-A、RTC 都能起来 |
| ACPI / Windows | 没有：固件只把设备树交给 Linux |

## 安装固件

```bash
scripts/build.sh     dshanpi-a1
scripts/build-spl.sh dshanpi-a1
scripts/package.sh   dshanpi-a1
```

`package.sh` 在 `out/DShanPiA1/` 里生成两个镜像：

- `DShanPiA1-emmc.img`：写 eMMC 用。固件前面带分区表：p1 是固件（64 MiB），p2 是 eMMC 剩下的空间，没有格式化。分区表按 64 GB eMMC 算（`boards/dshanpi-a1.conf` 里的 `EMMC_BYTES`），容量不同要改这个值。
- `DShanPiA1-sdcard.img`：写 SD 卡用，或者写进不带分区表的 eMMC。

在 MASKROM 模式下写 eMMC：

```bash
rkdeveloptool db rk3576_spl_loader.bin
rkdeveloptool wl 0 out/DShanPiA1/DShanPiA1-emmc.img
rkdeveloptool rd
```

写入任一个镜像都会覆盖已保存的设置：NV 存储是镜像的一部分。

写 SD 卡（eMMC 里没有可启动的东西时，BootROM 会从 SD 卡启动）：

```bash
dd if=out/DShanPiA1/DShanPiA1-sdcard.img of=/dev/sdX bs=1M status=progress conv=fsync
```

串口用 1500000 8N1 观察。正常启动会依次打印 `U-Boot SPL`、`BL31`，然后是 TianoCore。

## 板级文件

- `boards/dshanpi-a1.conf`
- `Platform/100ASK/DShanPiA1/`：DSC、FDF 片段和平台库
- `devicetree/mainline/rk3576-100ask-dshanpi-a1{,-uefi}.dts`
- `uboot/v2026.10/dshanpi-a1/`：SPL 的 defconfig 和设备树
