/** @file
 *
 *  RockchipPlatformLib.c -- board glue for the 100ASK DShanPi A1 (RK3576)
 *
 *  Written from the TaishanPi 3M and CM5-IO libraries and the board's mainline
 *  device tree (arch/arm64/boot/dts/rockchip/rk3576-100ask-dshanpi-a1.dts,
 *  torvalds/linux 4dce97c6c265).  Every pin below names the device tree line
 *  it was taken from.  Not run on the board yet.
 *
 *  Pins and supplies that differ from the TaishanPi 3M and the CM5-IO:
 *    - SD power:      none to switch.  vmmc-supply is vcc_3v3_s0, always on.
 *    - USB-A 5V:      none to switch.  u2phy1_otg's supply is vcc_5v0_sys.
 *    - USB-C VBUS:    GPIO1 PC1 (vbus5v0_typec), left alone: there is no CC
 *                     controller, so the firmware cannot tell what is there.
 *    - M.2 3V3:       none to switch.  vcc_3v3_m2 is always on.
 *    - PCIe PERST#:   GPIO1 PB7
 *    - GMAC0 PHY:     reset GPIO0 PC2, no 25 MHz reference from the SoC
 *    - GMAC1 PHY:     reset GPIO0 PC5 (GMAC1 is not used by the firmware yet)
 *    - HDMI:          no level shifter or 5V enable GPIO
 *    - LEDs, fan:     none in the device tree
 *
 *  Copyright (c) 2021, Rockchip Limited.
 *  Copyright (c) 2023-2025, Mario Bălănică <mariobalanica02@gmail.com>
 *  Copyright (c) 2026, the edk2-rk3576 contributors
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 **/

#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/GpioLib.h>
#include <Library/PcdLib.h>
#include <Library/RK806.h>
#include <Library/RockchipPlatformLib.h>
#include <Library/TimerLib.h>
#include <Soc.h>
#include <VarStoreData.h>

/*
 * RK806 PMIC voltage init.
 *
 * Only the two CPU rails are written, as on the TaishanPi 3M.  RK3576Dxe
 * raises both clusters to 1608 MHz over SCMI, and mainline's OPP tables in
 * rk3576.dtsi want 750 mV (A53) and 737.5 mV (A72) for that rate; 800 mV
 * leaves 50 mV of margin and is inside the 550-950 mV range this board's
 * device tree allows on both.  Every other rail stays at what the PMIC and
 * the SPL left.
 *
 *   DCDC1 -> vdd_cpu_big_s0  (dcdc-reg1, A72, 550-950 mV)
 *   DCDC3 -> vdd_cpu_lit_s0  (dcdc-reg3, A53, 550-950 mV)
 */
static struct regulator_init_data rk806_init_data[] = {
  RK8XX_VOLTAGE_INIT (MASTER_BUCK1, 800000),   /* vdd_cpu_big_s0 (A72) */
  RK8XX_VOLTAGE_INIT (MASTER_BUCK3, 800000),   /* vdd_cpu_lit_s0 (A53) */
};

/*
 * SdmmcIoMux -- SD card iomux (sdmmc: mmc@2a310000)
 *
 * &sdmmc keeps rk3576.dtsi's pinctrl (all function 1 in rk3576-pinctrl.dtsi):
 *   sdmmc0_bus4:  GPIO2 PA0-PA3  (D0-D3)
 *   sdmmc0_cmd:   GPIO2 PA4      (CMD)
 *   sdmmc0_clk:   GPIO2 PA5      (CLK)
 *   sdmmc0_det:   GPIO0 PA7      (hardware DETN)
 *
 * sdmmc0_pwren (GPIO0 PB6) is in that pinctrl too, but is not muxed, as on
 * the CM5-IO: the card is powered from vcc_3v3_s0 (vmmc-supply), which is
 * always on, so nothing on this board needs the controller's PWREN.
 */
VOID
EFIAPI
SdmmcIoMux (
  VOID
  )
{
  /* Ungate SDGMAC root clock */
  MmioWrite32 (CRU_CLKGATE_CON(42),
    (1U << (0 + 16)) | (0U << 0));

  /* Ungate SDMMC0 clocks */
  MmioWrite32 (CRU_CLKGATE_CON(43),
    (1U << (1 + 16)) | (0U << 1));
  MmioWrite32 (CRU_CLKGATE_CON(43),
    (1U << (2 + 16)) | (0U << 2));

  /* sdmmc0_bus4: GPIO2 PA0-PA3 function 1 (data D0-D3) */
  GpioPinSetFunction (2, GPIO_PIN_PA0, 1);
  GpioPinSetFunction (2, GPIO_PIN_PA1, 1);
  GpioPinSetFunction (2, GPIO_PIN_PA2, 1);
  GpioPinSetFunction (2, GPIO_PIN_PA3, 1);
  /* sdmmc0_cmd: GPIO2 PA4 function 1 */
  GpioPinSetFunction (2, GPIO_PIN_PA4, 1);
  /* sdmmc0_clk: GPIO2 PA5 function 1 */
  GpioPinSetFunction (2, GPIO_PIN_PA5, 1);
  /* sdmmc0_det: GPIO0 PA7 function 1 */
  GpioPinSetFunction (0, GPIO_PIN_PA7, 1);

  DEBUG ((DEBUG_INFO, "DShanPi-A1 SdmmcIoMux: SDMMC0 clocks ungated + iomux set\n"));
}

/*
 * SdhciEmmcIoMux -- eMMC iomux (sdhci: mmc@2a330000)
 *
 * Onboard eMMC, 8-bit (&sdhci bus-width = <8>).
 * From rk3576-pinctrl.dtsi (emmc groups, all function 1):
 *   emmc_bus8: GPIO1 PA0-PA7  (D0-D7)
 *   emmc_cmd:  GPIO1 PB0      (CMD)
 *   emmc_clk:  GPIO1 PB1      (CLK)
 *   emmc_strb: GPIO1 PB2      (STRB)
 *   emmc_rstnout: GPIO1 PB3   (RSTN)
 */
VOID
EFIAPI
SdhciEmmcIoMux (
  VOID
  )
{
  /* Ungate NVM root clocks */
  MmioWrite32 (CRU_CLKGATE_CON(33),
    (1U << (0 + 16)) | (0U << 0));
  MmioWrite32 (CRU_CLKGATE_CON(33),
    (1U << (1 + 16)) | (0U << 1));

  /* Ungate eMMC clocks */
  MmioWrite32 (CRU_CLKGATE_CON(33),
    (1U << (8 + 16)) | (0U << 8));
  MmioWrite32 (CRU_CLKGATE_CON(33),
    (1U << (9 + 16)) | (0U << 9));
  MmioWrite32 (CRU_CLKGATE_CON(33),
    (1U << (10 + 16)) | (0U << 10));
  MmioWrite32 (CRU_CLKGATE_CON(33),
    (1U << (11 + 16)) | (0U << 11));
  MmioWrite32 (CRU_CLKGATE_CON(33),
    (1U << (12 + 16)) | (0U << 12));

  /* emmc_bus8: GPIO1 PA0-PA7 function 1 */
  GpioPinSetFunction (1, GPIO_PIN_PA0, 1);
  GpioPinSetFunction (1, GPIO_PIN_PA1, 1);
  GpioPinSetFunction (1, GPIO_PIN_PA2, 1);
  GpioPinSetFunction (1, GPIO_PIN_PA3, 1);
  GpioPinSetFunction (1, GPIO_PIN_PA4, 1);
  GpioPinSetFunction (1, GPIO_PIN_PA5, 1);
  GpioPinSetFunction (1, GPIO_PIN_PA6, 1);
  GpioPinSetFunction (1, GPIO_PIN_PA7, 1);
  /* emmc_cmd: GPIO1 PB0 function 1 */
  GpioPinSetFunction (1, GPIO_PIN_PB0, 1);
  /* emmc_clk: GPIO1 PB1 function 1 */
  GpioPinSetFunction (1, GPIO_PIN_PB1, 1);
  /* emmc_strb: GPIO1 PB2 function 1 */
  GpioPinSetFunction (1, GPIO_PIN_PB2, 1);
  /* emmc_rstnout: GPIO1 PB3 function 1 */
  GpioPinSetFunction (1, GPIO_PIN_PB3, 1);

  DEBUG ((DEBUG_INFO, "DShanPi-A1 SdhciEmmcIoMux: eMMC clocks ungated + iomux set\n"));
}

/*
 * Rk806SpiIomux -- the RK806 is on I2C1 here (&i2c1 pmic@23), not SPI.
 */
VOID
EFIAPI
Rk806SpiIomux (
  VOID
  )
{
}

VOID
EFIAPI
Rk806Configure (
  VOID
  )
{
  UINTN  RegCfgIndex;

  RK806Init ();

  /* dvs1 -> null, as rk806_dvs1_null in the device tree */
  RK806PinSetFunction (MASTER, 1, 0);  /* gpio_pwrctrl1 = pin_fun0 (null) */

  for (RegCfgIndex = 0; RegCfgIndex < ARRAY_SIZE (rk806_init_data); RegCfgIndex++) {
    RK806RegulatorInit (rk806_init_data[RegCfgIndex]);
  }
}

VOID
EFIAPI
SetCPULittleVoltage (
  IN UINT32  Microvolts
  )
{
  /* A53 cluster = MASTER_BUCK3 (vdd_cpu_lit_s0) */
  struct regulator_init_data Rk806CpuLittleSupply =
    RK8XX_VOLTAGE_INIT (MASTER_BUCK3, Microvolts);
  RK806RegulatorInit (Rk806CpuLittleSupply);
}

/*
 * NorFspiIomux -- SPI NOR flash iomux (sfc0: spi@2a340000).  The board has no
 * SPI NOR; NorFlashDxe finds nothing and the NV store stays on the SD card.
 */
VOID
EFIAPI
NorFspiIomux (
  VOID
  )
{
}

VOID
EFIAPI
NorFspiEnableClock (
  UINT32  *CruBase
  )
{
  //
  // Only called at runtime, from NorFlashDxe, once the OS has had the
  // chance to gate these clocks.  So the register is reached through
  // CruBase, which NorFlashDxe converts at SetVirtualAddressMap, and not
  // through the CRU_CLKGATE_CON() physical address.
  //
  UINTN  GateCon33 = (UINTN)CruBase + 0x800 + 33 * 4;

  /*
   * NVM domain: HCLK_NVM_ROOT (bit 0) + ACLK_NVM_ROOT (bit 1) in CON(33).
   * SFC0:  SCLK_FSPI_X2 (bit 6) + HCLK_FSPI (bit 7) in CON(33).
   */
  MmioWrite32 (GateCon33,
    (1U << (0 + 16)) | (0U << 0));
  MmioWrite32 (GateCon33,
    (1U << (1 + 16)) | (0U << 1));
  MmioWrite32 (GateCon33,
    (1U << (6 + 16)) | (0U << 6));
  MmioWrite32 (GateCon33,
    (1U << (7 + 16)) | (0U << 7));
}

/*
 * GmacIomux -- GMAC pins.
 *
 * gmac0, eth0 M0 group, from &gmac0 pinctrl-0 and rk3576-pinctrl.dtsi
 * (all function 3):
 *   eth0m0_miim:       GPIO3_PA5 (MDIO), GPIO3_PA6 (MDC)
 *   eth0m0_rx_bus2:    GPIO3_PA7 (RXCTL), GPIO3_PB2 (RXD0), GPIO3_PB1 (RXD1)
 *   eth0m0_tx_bus2:    GPIO3_PB3 (TXCTL), GPIO3_PB5 (TXD0), GPIO3_PB4 (TXD1)
 *   eth0m0_rgmii_clk:  GPIO3_PD1 (RXCLK), GPIO3_PB6 (TXCLK)
 *   eth0m0_rgmii_bus:  GPIO3_PD3 (RXD2), GPIO3_PD2 (RXD3), GPIO3_PC3 (TXD2), GPIO3_PC2 (TXD3)
 *
 * gmac1, eth1 M0 group, from &gmac1 pinctrl-0 (all function 2):
 *   eth1m0_miim:       GPIO2_PD5 (MDIO), GPIO2_PD4 (MDC)
 *   eth1m0_rx_bus2:    GPIO2_PD3 (RXCTL), GPIO2_PD1 (RXD0), GPIO2_PD2 (RXD1)
 *   eth1m0_tx_bus2:    GPIO2_PD0 (TXCTL), GPIO2_PC6 (TXD0), GPIO2_PC7 (TXD1)
 *   eth1m0_rgmii_clk:  GPIO2_PC2 (RXCLK), GPIO2_PC5 (TXCLK)
 *   eth1m0_rgmii_bus:  GPIO2_PC0 (RXD2), GPIO2_PC1 (RXD3), GPIO2_PC3 (TXD2), GPIO2_PC4 (TXD3)
 *
 * Neither pinctrl has an mclk or a 25 MHz reference (ethm0_clk0_25m_out):
 * clock_in_out = "output", so the MAC TX clock comes from the CRU, and the
 * YT8521SC PHYs have their own crystals.  GmacPlatformDxe only brings up
 * gmac0 (PcdGmac1Supported is FALSE), so the gmac1 half is not reached yet.
 */
VOID
EFIAPI
GmacIomux (
  IN UINT32  Id
  )
{
  switch (Id) {
    case 0:
      /* eth0m0_miim */
      GpioPinSetFunction (3, GPIO_PIN_PA5, 3);
      GpioPinSetFunction (3, GPIO_PIN_PA6, 3);
      /* eth0m0_rx_bus2 */
      GpioPinSetFunction (3, GPIO_PIN_PA7, 3);
      GpioPinSetFunction (3, GPIO_PIN_PB2, 3);
      GpioPinSetFunction (3, GPIO_PIN_PB1, 3);
      /* eth0m0_tx_bus2 */
      GpioPinSetFunction (3, GPIO_PIN_PB3, 3);
      GpioPinSetFunction (3, GPIO_PIN_PB5, 3);
      GpioPinSetFunction (3, GPIO_PIN_PB4, 3);
      /* eth0m0_rgmii_clk */
      GpioPinSetFunction (3, GPIO_PIN_PD1, 3);
      GpioPinSetFunction (3, GPIO_PIN_PB6, 3);
      /* eth0m0_rgmii_bus */
      GpioPinSetFunction (3, GPIO_PIN_PD3, 3);
      GpioPinSetFunction (3, GPIO_PIN_PD2, 3);
      GpioPinSetFunction (3, GPIO_PIN_PC3, 3);
      GpioPinSetFunction (3, GPIO_PIN_PC2, 3);
      break;
    case 1:
      /* eth1m0_miim */
      GpioPinSetFunction (2, GPIO_PIN_PD4, 2);
      GpioPinSetFunction (2, GPIO_PIN_PD5, 2);
      /* eth1m0_rx_bus2 */
      GpioPinSetFunction (2, GPIO_PIN_PD3, 2);
      GpioPinSetFunction (2, GPIO_PIN_PD1, 2);
      GpioPinSetFunction (2, GPIO_PIN_PD2, 2);
      /* eth1m0_tx_bus2 */
      GpioPinSetFunction (2, GPIO_PIN_PD0, 2);
      GpioPinSetFunction (2, GPIO_PIN_PC6, 2);
      GpioPinSetFunction (2, GPIO_PIN_PC7, 2);
      /* eth1m0_rgmii_clk */
      GpioPinSetFunction (2, GPIO_PIN_PC2, 2);
      GpioPinSetFunction (2, GPIO_PIN_PC5, 2);
      /* eth1m0_rgmii_bus */
      GpioPinSetFunction (2, GPIO_PIN_PC0, 2);
      GpioPinSetFunction (2, GPIO_PIN_PC1, 2);
      GpioPinSetFunction (2, GPIO_PIN_PC3, 2);
      GpioPinSetFunction (2, GPIO_PIN_PC4, 2);
      break;
    default:
      break;
  }
}

/*
 * GmacIoPhyReset -- assert (Enable = TRUE) or release the PHY reset.
 *
 *   gmac0 PHY: &mdio0 rgmii_phy0 reset-gpios = <&gpio0 RK_PC2 GPIO_ACTIVE_LOW>
 *   gmac1 PHY: &mdio1 rgmii_phy1 reset-gpios = <&gpio0 RK_PC5 GPIO_ACTIVE_LOW>
 *
 * Both are active-low: assert drives the pin low, release drives it high.
 * The device tree asks for 20 ms asserted and 100 ms after release; the
 * caller's delays are GmacPlatformDxe's.
 */
VOID
EFIAPI
GmacIoPhyReset (
  IN UINT32   Id,
  IN BOOLEAN  Enable
  )
{
  switch (Id) {
    case 0:
      GpioPinSetDirection (0, GPIO_PIN_PC2, GPIO_PIN_OUTPUT);
      GpioPinWrite (0, GPIO_PIN_PC2, !Enable);
      break;
    case 1:
      GpioPinSetDirection (0, GPIO_PIN_PC5, GPIO_PIN_OUTPUT);
      GpioPinWrite (0, GPIO_PIN_PC5, !Enable);
      break;
    default:
      break;
  }
}

/*
 * I2cIomux -- I2C pins.
 *
 * &i2c1 and &i2c2 keep rk3576.dtsi's pinctrl:
 *   i2c1m0: GPIO0 PB2 (fn 11) = SCL, GPIO0 PB3 (fn 11) = SDA  (RK806 PMIC)
 *   i2c2m0: GPIO0 PB7 (fn  9) = SCL, GPIO0 PC0 (fn  9) = SDA  (DS1338 RTC)
 *
 * The firmware only uses I2C1.  I2C2 is set up anyway, the same way as on the
 * other boards, so that a later DS1338 driver has its bus.
 */
VOID
EFIAPI
I2cIomux (
  UINT32  id
  )
{
  switch (id) {
    case 1:
      GpioPinSetFunction (0, GPIO_PIN_PB2, 11);
      GpioPinSetFunction (0, GPIO_PIN_PB3, 11);
      DEBUG ((DEBUG_INFO, "DShanPi-A1 I2cIomux: I2C1 (PMIC) GPIO0 PB2/PB3 fn11\n"));
      break;
    case 2:
      /*
       * PCLK_I2C2 and CLK_I2C2 are CRU_GATE_CON12 bits 1 and 13 (clk-rk3576.c).
       * Linux gates them while the bus is idle, so a warm reboot need not
       * find them open.
       */
      MmioWrite32 (CRU_CLKGATE_CON (12), (BIT1 | BIT13) << 16);
      GpioPinSetFunction (0, GPIO_PIN_PB7, 9);
      GpioPinSetFunction (0, GPIO_PIN_PC0, 9);
      DEBUG ((DEBUG_INFO, "DShanPi-A1 I2cIomux: I2C2 (RTC) GPIO0 PB7/PC0 fn9, clocks ungated\n"));
      break;
    default:
      break;
  }
}

/*
 * UsbPortPowerEnable -- nothing to switch.
 *
 * The USB-A ports are on DRD1 (&usb_drd1_dwc3, dr_mode = "host"), and
 * u2phy1_otg's phy-supply is vcc_5v0_sys, a fixed always-on rail.
 *
 * The USB-C port (DRD0) has vbus5v0_typec, enabled by GPIO1 PC1, but no CC
 * controller in the device tree.  Turning VBUS on blind would drive 5 V into
 * a charger or a PC on the other end, so it is left as the SPL left it (off).
 */
VOID
EFIAPI
UsbPortPowerEnable (
  VOID
  )
{
  DEBUG ((DEBUG_INFO, "DShanPi-A1 UsbPortPowerEnable: USB-A always powered; USB-C VBUS (GPIO1 PC1) left off\n"));
}

/*
 * UsbTypeCGetOrientation -- unknown: there is no CC controller to ask.
 */
USB_TYPEC_ORIENTATION
EFIAPI
UsbTypeCGetOrientation (
  IN UINT32  Port
  )
{
  return UsbTypeCOrientationUnknown;
}

/*
 * Usb2PhyResume -- USB2 PHY wakeup and USB clocks.
 *
 * The same RK3576 USB2 PHY sequence as the TaishanPi 3M, the CM5-IO and the
 * ROCK 4D.
 */
VOID
EFIAPI
Usb2PhyResume (
  VOID
  )
{
  /* Step 1: Ungate ALL USB clocks in CLKGATE_CON(47) */
  MmioWrite32 (CRU_CLKGATE_CON(47), 0xFFFF0000);

  /* Step 2: Deassert SIDDQ (bit 13 in GRF+0x0010) */
  MmioWrite32 (USB2PHY0_BASE + 0x0010, 0x20000000);
  MmioWrite32 (USB2PHY1_BASE + 0x0010, 0x20000000);

  /* Step 3: First PHY CRU hard reset */
  MmioWrite32 (CRU_SOFTRST_CON(28), 0x00030003);
  MicroSecondDelay (10);
  MmioWrite32 (CRU_SOFTRST_CON(28), 0x00030000);
  MicroSecondDelay (150);

  /* Step 4: HS signal quality tuning */
  MmioWrite32 (USB2PHY0_BASE + 0x000C, 0x0F000900);
  MmioWrite32 (USB2PHY1_BASE + 0x000C, 0x0F000900);
  MmioWrite32 (USB2PHY0_BASE + 0x0010, 0x00180010);
  MmioWrite32 (USB2PHY1_BASE + 0x0010, 0x00180010);

  /* Step 5: Clear phy_sus */
  MmioWrite32 (USB2PHY0_BASE + 0x0000, 0x01FF0000);
  MmioWrite32 (USB2PHY1_BASE + 0x0000, 0x01FF0000);

  /* Step 5b: Second PHY CRU hard reset */
  MmioWrite32 (CRU_SOFTRST_CON(28), 0x00030003);
  MicroSecondDelay (10);
  MmioWrite32 (CRU_SOFTRST_CON(28), 0x00030000);
  MicroSecondDelay (150);

  /* Step 5c: Wait for UTMI clock stabilization */
  MicroSecondDelay (2000);

  DEBUG ((DEBUG_INFO,
    "DShanPi-A1 Usb2PhyResume: SIDDQ off, 1st reset, HS tuning, phy_sus off, "
    "2nd reset+2ms\n"));
}

/*
 * PcieIoInit / PciePowerEn / PciePeReset -- pcie0, the M.2 slot.
 *
 *   power:  vcc_3v3_m2, a fixed always-on regulator: nothing to switch
 *   reset:  &pcie0 reset-gpios = <&gpio1 RK_PB7 GPIO_ACTIVE_HIGH>
 *
 * PERST# is driven low to hold the endpoint in reset and high to release
 * it, so Enable ("assert reset") maps to low, as on the CM5-IO (where the
 * opposite was measured to break link training).
 */
VOID
EFIAPI
PcieIoInit (
  UINT32  Segment
  )
{
  switch (Segment) {
    case 0:
      GpioPinSetDirection (1, GPIO_PIN_PB7, GPIO_PIN_OUTPUT);
      break;
    default:
      break;
  }
}

VOID
EFIAPI
PciePowerEn (
  UINT32   Segment,
  BOOLEAN  Enable
  )
{
}

VOID
EFIAPI
PciePeReset (
  UINT32   Segment,
  BOOLEAN  Enable
  )
{
  switch (Segment) {
    case 0:
      GpioPinWrite (1, GPIO_PIN_PB7, !Enable);
      break;
    default:
      break;
  }
}

/*
 * HdmiTxIomux -- HDMI TX clocks and resets.
 *
 * The device tree has no HDMI 5V or level-shifter GPIO for this board, so
 * there is nothing to drive.  The CRU clock ungating and reset deasserts are
 * the SoC's, the same as on the other three boards; see the CM5-IO library
 * for where each value comes from.
 */
VOID
EFIAPI
HdmiTxIomux (
  IN UINT32  Id
  )
{
  switch (Id) {
    case 0:
      MicroSecondDelay (50 * 1000);

      /* CLKGATE_CON(61): VOP Root + VP clocks (ungate all) */
      MmioWrite32 (CRU_CLKGATE_CON(61), 0x3F0D0000 | 0x0000);

      /* CLKGATE_CON(62): DCLK_VP1, DCLK_VP2 */
      MmioWrite32 (CRU_CLKGATE_CON(62), 0x00030000 | 0x0000);

      /* CLKGATE_CON(63): VO0 root clocks, plus ACLK/HCLK/PCLK_HDCP0 (PD_VO0) */
      MmioWrite32 (CRU_CLKGATE_CON(63), 0x700B0000 | 0x0000);

      /* CLKGATE_CON(64): HDMI TX clocks */
      MmioWrite32 (CRU_CLKGATE_CON(64), 0x03800000 | 0x0000);

      /* PMU1CRU: PCLK_HDPTX_APB + CLK_HDMITXHDP + PCLK_PMUPHY_ROOT */
      MmioWrite32 (PMU1CRU_BASE + 0x800,       (BIT (1) << 16) | 0U);
      MmioWrite32 (PMU1CRU_BASE + 0x800 + 4,   (BIT (13) << 16) | 0U);
      MmioWrite32 (PMU1CRU_BASE + 0x800 + 20,  (BIT (0) << 16) | 0U);

      /* Enable HDPTX PHY analog bias + BGR */
      MmioWrite32 (0x26032000U,
        ((BIT (6) | BIT (5)) << 16) | (BIT (6) | BIT (5)));
      MicroSecondDelay (100);

      /*
       * Deassert the HDMI TX software resets (rst-rk3576.c):
       *   SRST_HDMITX0_REF        CRU     SOFTRST_CON64 bit 9
       *   SRST_LINKSYM_HDMITXPHY0 CRU     SOFTRST_CON75 bit 1
       *   SRST_HDMITXHDP          PMU1CRU SOFTRST_CON01 bit 13
       */
      MmioWrite32 (CRU_SOFTRST_CON (64), (BIT (9) << 16) | 0U);
      MmioWrite32 (CRU_SOFTRST_CON (75), (BIT (1) << 16) | 0U);
      MmioWrite32 (PMU1CRU_BASE + 0xA04, (BIT (13) << 16) | 0U);

      MicroSecondDelay (5 * 1000);
      DEBUG ((DEBUG_INFO,
        "DShanPi-A1 HdmiTxIomux: VOP, HDMI and HDPTX APB clocks ungated, resets deasserted\n"));
      break;
    default:
      break;
  }
}

/*
 * PwmFanIoSetup / PwmFanSetSpeed -- no fan in the device tree.
 * PcdPwmFanSpeedDefault stays 0, so the fan setup option is not offered.
 */
VOID
EFIAPI
PwmFanIoSetup (
  VOID
  )
{
}

VOID
EFIAPI
PwmFanSetSpeed (
  IN UINT32  Percentage
  )
{
}

/*
 * PlatformInitLeds / PlatformSetStatusLed -- no LED in the device tree.
 */
VOID
EFIAPI
PlatformInitLeds (
  VOID
  )
{
}

VOID
EFIAPI
PlatformSetStatusLed (
  IN BOOLEAN  Enable
  )
{
}

/*
 * PlatformEarlyInit -- nothing yet.  The board has no Wi-Fi or Bluetooth in
 * its device tree, and the ES8388 codec (I2C4, SAI2) is the OS's to set up.
 */
VOID
EFIAPI
PlatformEarlyInit (
  VOID
  )
{
}

/*
 * PlatformGetDtbFileGuid -- the embedded DTB for a compat mode.  The mainline
 * GUID must match FILE_GUID in DeviceTree/Mainline.inf.
 */
CONST EFI_GUID *
EFIAPI
PlatformGetDtbFileGuid (
  IN UINT32  CompatMode
  )
{
  STATIC CONST EFI_GUID VendorDtbFileGuid = {
    /* No vendor DTB is built for this board; reserved. 309ba733-3c9c-48b5-9cb0-110d34ca87e2 */
    0x309ba733, 0x3c9c, 0x48b5, { 0x9c, 0xb0, 0x11, 0x0d, 0x34, 0xca, 0x87, 0xe2 }
  };
  STATIC CONST EFI_GUID MainlineDtbFileGuid = {
    /* DeviceTree/Mainline.inf FILE_GUID: 6021fee2-c821-49ce-9486-a36d45d40a43 */
    0x6021fee2, 0xc821, 0x49ce, { 0x94, 0x86, 0xa3, 0x6d, 0x45, 0xd4, 0x0a, 0x43 }
  };

  switch (CompatMode) {
    case FDT_COMPAT_MODE_VENDOR:
      return &VendorDtbFileGuid;
    case FDT_COMPAT_MODE_MAINLINE:
      return &MainlineDtbFileGuid;
  }

  return NULL;
}

/*
 * PlatformSdioWifiInit -- this board does not set PcdAcpiSdioWifiSupported,
 * so RK3576Dxe never calls this.
 */
EFI_STATUS
EFIAPI
PlatformSdioWifiInit (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}

/*
 * PlatformGetPin40Config -- this board does not set PcdPin40Supported, so
 * RK3576Dxe never asks.
 */
CONST PIN40_CONFIG *
EFIAPI
PlatformGetPin40Config (
  VOID
  )
{
  return NULL;
}
