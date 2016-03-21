/*
 * This file is part of the coreboot project.
 *
 * Copyright 2016 Rockchip Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <arch/io.h>
#include <assert.h>
#include <bootblock_common.h>
#include <delay.h>
#include <soc/clock.h>
#include <soc/grf.h>
#include <soc/i2c.h>
#include <soc/rk808.h>
#include <soc/spi.h>
#include <soc/tsadc.h>
#include <console/console.h>

void bootblock_mainboard_early_init(void)
{
	if (IS_ENABLED(CONFIG_DRIVERS_UART)) {
		assert(CONFIG_CONSOLE_SERIAL_UART_ADDRESS == UART2_BASE);
		write32(&rk3399_grf->iomux_uart2c, IOMUX_UART2C);

		/* grf soc_con7[11:10] use for uart2 select*/
		write32(&rk3399_grf->soc_con7, UART2C_SEL);
	}
}

void bootblock_mainboard_init(void)
{
	/* Up VDD_CPU (BUCK2) to 1.4V to support max CPU frequency (1.6GHz). */
	write32(&rk3399_pmugrf->iomux_i2c0_scl, IOMUX_I2C0_SCL);
	write32(&rk3399_pmugrf->iomux_i2c0_sda, IOMUX_I2C0_SDA);
	i2c_init(CONFIG_PMIC_BUS, 400*KHz);

	rk808_configure_buck(2, 1100);
	udelay(100);/* Must wait for voltage to stabilize,2mV/us */
	rkclk_configure_cpu(APLL_L_600_MHZ);
	write32(&rk3399_grf->iomux_spi2, IOMUX_SPI2);
	rockchip_spi_init(CONFIG_BOOT_MEDIA_SPI_BUS, 24750*KHz);

	/* enable tsadc */
	write32(&rk3399_pmugrf->tsadc_int, IOMUX_TSADC_INT);
	tsadc_init();
}
