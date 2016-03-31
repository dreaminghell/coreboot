##
## This file is part of the coreboot project.
##
## Copyright 2016 Rockchip Inc.
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; version 2 of the License.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##

ifeq ($(CONFIG_SOC_ROCKCHIP_RK3399),y)

IDBTOOL = util/rockchip/make_idb.py
bootblock-y += bootblock.c
bootblock-y += timer.c
bootblock-y += ../common/spi.c
bootblock-y += clock.c
bootblock-y += mmu_operations.c
bootblock-y += i2c.c
bootblock-y += gpio.c
bootblock-y += pwm.c
bootblock-y += tsadc.c
bootblock-$(CONFIG_EVB_MODE) += ../common/rk808.c
ifeq ($(CONFIG_BOOTBLOCK_CONSOLE),y)
bootblock-$(CONFIG_DRIVERS_UART) += ../common/uart.c
endif

verstage-$(CONFIG_DRIVERS_UART) += ../common/uart.c
verstage-y += ../common/cbmem.c
verstage-y += timer.c
verstage-y += ../common/spi.c
verstage-y += clock.c
verstage-y += i2c.c
verstage-y += gpio.c
verstage-y += pwm.c
################################################################################

romstage-$(CONFIG_DRIVERS_UART) += ../common/uart.c
romstage-y += ../common/cbmem.c
romstage-y += timer.c
romstage-y += ../common/spi.c
romstage-y += clock.c
romstage-y += mmu_operations.c
romstage-y += i2c.c
romstage-y += gpio.c
romstage-y += pwm.c
romstage-y += sdram.c
################################################################################

ramstage-y += ../common/cbmem.c
ramstage-y += soc.c
ramstage-$(CONFIG_DRIVERS_UART) += ../common/uart.c
ramstage-y += timer.c
ramstage-y += ../common/spi.c
ramstage-y += clock.c
ramstage-y += i2c.c
ramstage-y += gpio.c
ramstage-y += pwm.c
ramstage-y += saradc.c
ramstage-y += sdram.c
ramstage-y += edp.c
ramstage-y += vop.c
ramstage-y += display.c
ramstage-$(CONFIG_EVB_MODE) += rk808.c

BL31_MAKEARGS += PLAT=rk3399
################################################################################

CPPFLAGS_common += -Isrc/soc/rockchip/rk3399/include
CPPFLAGS_common += -Isrc/soc/rockchip/common/include

$(objcbfs)/bootblock.bin: $(objcbfs)/bootblock.raw.bin
	@printf "Generating: $(subst $(obj)/,,$(@))\n"
	@mkdir -p $(dir $@)
	@$(IDBTOOL) --from=$< --to=$@ --enable-align --chip=RK33

endif
