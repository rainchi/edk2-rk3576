## @file
#
#  UEFI platform description for the 100ASK DShanPi A1 (RK3576)
#
#  Boots from the eMMC (or an SD card) to the UEFI front page and on to
#  Linux; Ubuntu 26.04 installed over netboot.xyz runs (2026-11-26).  See
#  docs/boards/dshanpi-a1.md for what works.
#
#  Board:  100ASK DShanPi A1
#  SoC:    RK3576 (4x A72 + 4x A53), LPDDR4X
#  PMIC:   RK806 on I2C1, 0x23
#  RTC:    DS1338 on I2C2, 0x68 (no firmware driver; the virtual clock is used)
#  PHY:    two Motorcomm YT8521SC, on GMAC0 and GMAC1
#  Debug:  UART0 at 0x2AD40000, 1500000 8N1
#
#  Every pin comes from the board's mainline device tree,
#  devicetree/mainline/rk3576-100ask-dshanpi-a1.dts.  Everything the SoC
#  decides is in Silicon/Rockchip/RK3576/RK3576Base.dsc.inc.  The values below
#  start from the TaishanPi 3M and the CM5-IO, the other boards here that boot
#  from SD or eMMC, and each one says whether it has been checked here.
#
#  Copyright (c) 2023-2025, Mario Bălănică <mariobalanica02@gmail.com>
#  Copyright (c) 2026, the edk2-rk3576 contributors
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

################################################################################
[Defines]
  PLATFORM_NAME                  = DShanPiA1
  PLATFORM_VENDOR                = 100ASK
  PLATFORM_GUID                  = c24a8e2b-31c1-407b-b1aa-7e55a3608d94
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010019
  OUTPUT_DIRECTORY               = Build/$(PLATFORM_NAME)
  VENDOR_DIRECTORY               = Platform/$(PLATFORM_VENDOR)
  PLATFORM_DIRECTORY             = $(VENDOR_DIRECTORY)/$(PLATFORM_NAME)
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Silicon/Rockchip/RK3576/RK3576.fdf
  RK_PLATFORM_FVMAIN_MODULES     = $(PLATFORM_DIRECTORY)/DShanPiA1.Modules.fdf.inc
  FIRMWARE_VER                   = "0.1"

  # RK_NOR_FLASH_ENABLE stays TRUE as on the TaishanPi 3M: the board has no
  # SPI NOR, but that is the combination known to build and run for a board
  # whose NV store is on the boot medium (see the FVB PCDs below).
  DEFINE RK_SD_ENABLE            = TRUE
  DEFINE RK_EMMC_ENABLE          = TRUE
  DEFINE RK_NOR_FLASH_ENABLE     = TRUE
  DEFINE RK_FVB_ENABLE           = TRUE
  # The RTC is a DS1338 (&i2c2 rtc@68), not the HYM8563 the firmware has a
  # driver for.  I2cDxe stays on for the PMIC (Rk806I2cPowerOffDxe).
  DEFINE RK_RTC8563_ENABLE       = FALSE
  DEFINE RK_I2C_ENABLE           = TRUE
  DEFINE RK_STATUS_LED_ENABLE    = FALSE
  # GMAC0 only for now.  GMAC1 needs GmacPlatformDxe support, and the
  # YT8521SC on both needs an init in MotorcommPhy.c.
  DEFINE RK3576_GMAC_ENABLE      = TRUE
  # pcie0 is the M.2 slot (&pcie0, vcc_3v3_m2 always on).
  DEFINE RK3576_PCIE_ENABLE      = TRUE
  DEFINE RK_AHCI_ENABLE          = FALSE
  DEFINE RK_DW_HDMI_QP_ENABLE    = TRUE
  DEFINE RK_DISPLAY_ENABLE       = TRUE
  DEFINE RK_AMD_GOP_ENABLE       = FALSE
  DEFINE SECURE_BOOT_ENABLE      = TRUE

  DEFINE NETWORK_ENABLE          = TRUE
  DEFINE NETWORK_SNP_ENABLE      = TRUE
  DEFINE NETWORK_IP4_ENABLE      = TRUE
  DEFINE NETWORK_IP6_ENABLE      = TRUE
  DEFINE NETWORK_PXE_BOOT_ENABLE = TRUE
  DEFINE NETWORK_HTTP_BOOT_ENABLE = FALSE
  DEFINE NETWORK_TLS_ENABLE      = FALSE
  DEFINE RK_REALTEK_UNDI_ENABLE  = FALSE
  DEFINE RK_X86_EMULATOR_ENABLE  = FALSE
  DEFINE RK_PLATFORM_SMBIOS_ENABLE = TRUE
  DEFINE RK_USB_ENABLE           = TRUE

!include Silicon/Rockchip/RK3576/RK3576Base.dsc.inc

################################################################################
[LibraryClasses.common]
  RockchipPlatformLib|$(PLATFORM_DIRECTORY)/Library/RockchipPlatformLib/RockchipPlatformLib.inf

################################################################################
[PcdsFixedAtBuild.common]
  # The USB-C port has no CC controller in the device tree, so the firmware
  # cannot tell a sink from a source and leaves its VBUS switch (GPIO1 PC1)
  # alone.  Nothing can appear on the port: do not wait for a device there.
  gRK3576TokenSpaceGuid.PcdUsbCDeviceWaitMs|0

  # SMBIOS identification
  gRockchipTokenSpaceGuid.PcdPlatformName|"DShanPi A1"
  gRockchipTokenSpaceGuid.PcdPlatformVendorName|"100ASK"
  gRockchipTokenSpaceGuid.PcdFamilyName|"DShanPi"
  gRockchipTokenSpaceGuid.PcdProductUrl|"https://www.100ask.net/"
  gRockchipTokenSpaceGuid.PcdDeviceTreeName|"rk3576-100ask-dshanpi-a1"

  # eMMC speed: the CM5-IO's values (see CM5IO.dsc for how they were
  # measured).  Same controller, different eMMC part.  The firmware runs it
  # at 52 MHz High Speed, 8-bit, and boots from it (2026-11-26).  Linux's
  # speed is set in the device tree, rk3576-100ask-dshanpi-a1-uefi.dts.
  gRockchipTokenSpaceGuid.PcdDwcSdhciForceDefaultSpeed|FALSE
  gRockchipTokenSpaceGuid.PcdDwcSdhciForceHighSpeed|TRUE
  gRockchipTokenSpaceGuid.PcdDwcSdhciNonDllStrbinDelay|0xa
  gRockchipTokenSpaceGuid.PcdDwcSdhciDisableSdma|FALSE

  # NV variable store on the boot medium.  These follow from the 32 MB SD
  # image layout package.sh writes, not from the board: the FIT is at sector
  # 16384 (0x800000) and the store sits past it at 0x1600000.
  gRockchipTokenSpaceGuid.PcdRkFvbNvStorageSpiOffset|0x1600000
  gRockchipTokenSpaceGuid.PcdNvStoragePreferSpiFlash|FALSE
  gRockchipTokenSpaceGuid.PcdFitImageFlashAddress|0x800000

  # GMAC0 is rgmii-id in the device tree, with no tx_delay or rx_delay: both
  # delays are the PHY's, so the MAC adds none, as on the CM5-IO and ROCK 4D.
  # The YT8521SC (ID 0x0000011A) is not one MotorcommPhy.c knows yet, so
  # nothing programs the PHY's delays and the link may not pass traffic.
  gRK3576TokenSpaceGuid.PcdGmac0TxDelay|0

  # FDT only, as on the ROCK 4D.  There are no ACPI tables for this board.
  gRK3576TokenSpaceGuid.PcdConfigTableModeDefault|$(CONFIG_TABLE_MODE_FDT)

  # 0x80000000 = NATIVE: follow the sink's EDID, as on the other boards.
  gRK3588TokenSpaceGuid.PcdDisplayModePresetDefault|{ 0x00, 0x00, 0x00, 0x80 }

  #
  # I2C.  The base file lists the HYM8563 the other boards have at 0x51 on
  # I2C2.  This board's only device the firmware talks to is
  #   I2C1 0x23  the RK806 PMIC (&i2c1 pmic@23).  Listed so that I2cDxe sets
  #              the bus up and maps it for runtime; Rk806I2cPowerOffDxe
  #              switches the PMIC off through it when the OS shuts down.
  #
  gRockchipTokenSpaceGuid.PcdI2cSlaveAddresses|{ 0x23 }
  gRockchipTokenSpaceGuid.PcdI2cSlaveBuses|{ 0x1 }
  gRockchipTokenSpaceGuid.PcdI2cSlaveBusesRuntimeSupport|{ TRUE }

  # The base file points Rk860xRegulatorDxe at I2C1 0x23, which on this board
  # is the RK806.  It attached to the PMIC, read "chip id 15" and logged two
  # errors every time BDS connected the bus.  No RK860x here: empty list.
  gRockchipTokenSpaceGuid.PcdRk860xRegulatorAddresses|{ 0x0 }
  gRockchipTokenSpaceGuid.PcdRk860xRegulatorBuses|{ 0x0 }

  # PSCI SYSTEM_OFF only raises PWR_CTRL1, which the RK806 ignores unless
  # told otherwise; see Rk806I2cPowerOffDxe.  Same PMIC, bus and address as
  # the TaishanPi 3M.  The driver reads CHIP_NAME 0x80 VER 0x6A on this
  # board (2026-11-26); switching off through it is not checked yet.
  gRK3576TokenSpaceGuid.PcdRk806I2cBase|0x2AC40000
  gRK3576TokenSpaceGuid.PcdRk806I2cAddress|0x23
  gRK3576TokenSpaceGuid.PcdRk806I2cBus|1

  # TSADC read-out and the hardware thermal shutdown (CRU reset at 120 C),
  # armed only after the readings check out (Rk3576TsadcLib.c).  The SoC's,
  # the same on every board.
  gRK3576TokenSpaceGuid.PcdTsadcEnable|TRUE

################################################################################
[Components.common]
  # Mainline device tree, built from DTS at compile time
  $(PLATFORM_DIRECTORY)/DeviceTree/Mainline.inf

  # The TianoCore logo: there is no 100ASK logo here.
  MdeModulePkg/Logo/LogoDxe.inf

  Silicon/Rockchip/RK3576/Drivers/Rk806I2cPowerOffDxe/Rk806I2cPowerOffDxe.inf

  # RK3576Dxe, FdtPlatformDxe, the PCIe stack and the GMAC stack are common to
  # every RK3576 board and are declared in RK3576Base.dsc.inc.
