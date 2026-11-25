# 100ASK DShanPi A1

English | [简体中文](dshanpi-a1.zh-CN.md)

**Early port.** Boots from the eMMC to the UEFI front page, with the picture
on HDMI (first boot 2026-11-25, board with 8 GB LPDDR4X and a 64 GB eMMC).
Ubuntu 26.04 (Linux 7.0) installed over netboot.xyz to the eMMC boots and runs (2026-11-26).

## Hardware

From the board's mainline device tree,
[`rk3576-100ask-dshanpi-a1.dts`](../../devicetree/mainline/rk3576-100ask-dshanpi-a1.dts):

| | |
|---|---|
| SoC, memory | RK3576, LPDDR4X |
| PMIC | RK806 on I2C1 |
| Storage | eMMC (8-bit, HS400 capable), microSD |
| Ethernet | 2x GbE, Motorcomm YT8521SC on GMAC0 and GMAC1 |
| PCIe | M.2 slot on pcie0 |
| USB | USB-C (DRD0, VBUS switched by GPIO1 PC1, no CC controller), USB-A (DRD1, host) |
| Display | HDMI |
| Audio | ES8388 on I2C4 and SAI2, HDMI audio |
| RTC | DS1338 on I2C2 |
| Keys | MASKROM, RECOVERY, USER2 on the SARADC; USER1 on GPIO0 PA0 |
| Serial | UART0, 1500000 8N1 |

## Status

From the serial log of the first boot:

| | |
|---|---|
| DDR | 8 GB LPDDR4X at 2112 MHz |
| HDMI | works, 3840x2160 at 60 Hz |
| eMMC | works, boots from it; the firmware runs it at 52 MHz High Speed, 8-bit. Linux gets HS200 at most 150 MHz from the firmware's device tree: mainline's HS400, and HS200 at 198 MHz, failed writes during an install; 16 GiB written and checked at 148.5 MHz had no error |
| Settings | saved on the eMMC and kept across a reset (checked with "Save OS Variable Changes to eMMC") |
| M.2 PCIe | link up at Gen2 x1; an Intel AX210 Wi-Fi card was found in the slot. NVMe not tried |
| USB-A | the controller starts and finds a device on the port; storage and keyboards not tried |
| USB-C | VBUS stays off: nothing can tell a device from a charger. Linux gets DRD0 as a peripheral (the firmware's device tree); with mainline's OTG default dwc3 panicked with an SError |
| Ethernet GMAC0 | works: YT8521 PHY (0x0000011A), link in 50 ms, DHCP lease, DNS and NTP; PXE boot loads a 1.1 MB EFI file over TFTP and runs it (2026-11-26). Which of the two ports it is not noted yet |
| Ethernet GMAC1 | not brought up: GmacPlatformDxe only drives GMAC0 |
| RK806 | answers on I2C1 (CHIP_NAME 0x80); power off through it not checked |
| SD card | not tried |
| RTC | no DS1338 driver: the clock does not survive a power cycle |
| Audio, keys | not used by the firmware |
| Linux | the installer boots: eMMC, PCIe, HDMI, USB-A and the RTC come up |
| ACPI / Windows | none: the firmware hands Linux the device tree only |

## Install the firmware

```bash
scripts/build.sh     dshanpi-a1
scripts/build-spl.sh dshanpi-a1
scripts/package.sh   dshanpi-a1
```

`package.sh` makes two images in `out/DShanPiA1/`:

- `DShanPiA1-emmc.img`: for the eMMC. The firmware with a partition table in
  front: p1 is the firmware (64 MiB), p2 is the rest of the eMMC, unformatted.
  The table is sized for the 64 GB eMMC (`EMMC_BYTES` in
  `boards/dshanpi-a1.conf`); change that for another size.
- `DShanPiA1-sdcard.img`: for an SD card, or the eMMC without a partition
  table.

To the eMMC, in MASKROM mode:

```bash
rkdeveloptool db rk3576_spl_loader.bin
rkdeveloptool wl 0 out/DShanPiA1/DShanPiA1-emmc.img
rkdeveloptool rd
```

Writing either image replaces the saved settings: the NV store is part of the
image.

To an SD card (the BootROM boots it when the eMMC has nothing bootable):

```bash
dd if=out/DShanPiA1/DShanPiA1-sdcard.img of=/dev/sdX bs=1M status=progress conv=fsync
```

Watch the serial console at 1500000 8N1. A good boot prints `U-Boot SPL`,
then `BL31`, then TianoCore.

## Board files

- `boards/dshanpi-a1.conf`
- `Platform/100ASK/DShanPiA1/`: DSC, FDF include and the platform library
- `devicetree/mainline/rk3576-100ask-dshanpi-a1{,-uefi}.dts`
- `uboot/v2026.10/dshanpi-a1/`: SPL defconfig and device tree
