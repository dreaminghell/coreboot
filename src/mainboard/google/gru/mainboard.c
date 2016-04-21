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

#include <arch/cache.h>
#include <arch/io.h>
#include <boardid.h>
#include <boot/coreboot_tables.h>
#include <console/console.h>
#include <delay.h>
#include <device/device.h>
#include <gpio.h>
#include <soc/clock.h>
#include <soc/display.h>
#include <soc/emmc.h>
#include <soc/grf.h>

static void configure_sdmmc(void)
{
	gpio_output(GPIO(4, D, 5), 1);  /* SDMMC_PWR_EN */
	gpio_output(GPIO(2, A, 2), 1);  /* SDMMC_SDIO_PWR_EN */
	gpio_input(GPIO(4, D, 2));      /* SDMMC_DET_L */
	gpio_output(GPIO(2, D, 4), 0);  /* Keep the max voltage */
	write32(&rk3399_grf->iomux_sdmmc, IOMUX_SDMMC);
}

/* display configure */
static void configure_display(void)
{
	/*
	 * once hpd pin in high status, we judge the panel connect,
	 * but the gpio4c7 default to pull up(about 33k~89k), we
	 * have external pull down resistance(100k) on hpd pin,
	 * so the voltage will be 1.9V even we do not connect the
	 * panel, set gpio4c7 to pull none here, let the hpd in low status
	 * when panel disconnect
	 */
	gpio_input(GPIO(4, C, 7));
	write32(&rk3399_grf->iomux_edp_hotplug, IOMUX_EDP_HOTPLUG);
	write32(&rk3399_grf->soc_con25, 1 << 27 | 1 << 11);
	gpio_output(GPIO(4, D, 3), 1);
}

static void mainboard_init(device_t dev)
{
	configure_sdmmc();
	configure_emmc();
	configure_display();
}

void mainboard_power_on_backlight(void)
{
	gpio_output(GPIO(1, C, 1), 1);  /* BL_EN */
}

static void mainboard_enable(device_t dev)
{
	dev->ops->init = &mainboard_init;
}

struct chip_operations mainboard_ops = {
	.name = CONFIG_MAINBOARD_PART_NUMBER,
	.enable_dev = mainboard_enable,
};
