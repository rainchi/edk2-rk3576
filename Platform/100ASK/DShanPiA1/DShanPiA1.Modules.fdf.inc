##
#  DShanPiA1.Modules.fdf.inc -- FV_MAIN module list for the 100ASK DShanPi A1
#
#  Included by Silicon/Rockchip/RK3576/RK3576.fdf through the
#  RK_PLATFORM_FVMAIN_MODULES define in DShanPiA1.dsc.
#
#  Copyright (c) 2026, the edk2-rk3576 contributors
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

  # Compiled mainline DTS (rk3576-100ask-dshanpi-a1-uefi.dts)
  INF RuleOverride = DTB  $(PLATFORM_DIRECTORY)/DeviceTree/Mainline.inf

  # Splash screen logo (TianoCore's)
  INF MdeModulePkg/Logo/LogoDxe.inf

  INF Silicon/Rockchip/RK3576/Drivers/Rk806I2cPowerOffDxe/Rk806I2cPowerOffDxe.inf
