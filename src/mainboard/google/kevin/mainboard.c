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
#include <device/device.h>
#include <gpio.h>
#include <soc/clock.h>
#include <soc/grf.h>

static void configure_sdmmc(void)
{
#if CONFIG_EVB_MODE
	gpio_output(GPIO(0, A, 1), 1);	/* SDMMC_PWR_EN */
	gpio_input(GPIO(0, A, 7));	/* SDMMC_DET_L */
	write32(&rk3399_grf->iomux_sdmmc, IOMUX_SDMMC);
#endif
}

static void mainboard_init(device_t dev)
{
	configure_sdmmc();
	rkclk_configure_emmc();
}

static void mainboard_enable(device_t dev)
{
	dev->ops->init = &mainboard_init;
}

struct chip_operations mainboard_ops = {
	.name = CONFIG_MAINBOARD_PART_NUMBER,
	.enable_dev = mainboard_enable,
};
