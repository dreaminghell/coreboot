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
bootblock-y += spi.c
bootblock-y += clock.c
bootblock-y += mmu_operations.c
bootblock-y += i2c.c
bootblock-y += gpio.c
bootblock-y += pwm.c
ifeq ($(CONFIG_BOOTBLOCK_CONSOLE),y)
bootblock-$(CONFIG_DRIVERS_UART) += uart.c
endif

verstage-$(CONFIG_DRIVERS_UART) += uart.c
verstage-y += cbmem.c
verstage-y += timer.c
verstage-y += spi.c
verstage-y += clock.c
verstage-y += i2c.c
verstage-y += gpio.c
verstage-y += pwm.c
################################################################################

romstage-$(CONFIG_DRIVERS_UART) += uart.c
romstage-y += cbmem.c
romstage-y += timer.c
romstage-y += spi.c
romstage-y += clock.c
romstage-y += mmu_operations.c
romstage-y += i2c.c
romstage-y += gpio.c
romstage-y += pwm.c
romstage-y += sdram.c
################################################################################

ramstage-y += cbmem.c
ramstage-y += soc.c
ramstage-$(CONFIG_DRIVERS_UART) += uart.c
ramstage-y += timer.c
ramstage-y += spi.c
ramstage-y += clock.c
ramstage-y += i2c.c
ramstage-y += gpio.c
ramstage-y += pwm.c
################################################################################

CPPFLAGS_common += -Isrc/soc/rockchip/rk3399/include

$(objcbfs)/bootblock.bin: $(objcbfs)/bootblock.raw.bin
	@printf "Generating: $(subst $(obj)/,,$(@))\n"
	@mkdir -p $(dir $@)
	@$(IDBTOOL) --from=$< --to=$@ --enable-align --chip=RK33

endif
