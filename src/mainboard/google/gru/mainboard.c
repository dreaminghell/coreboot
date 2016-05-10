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
#include <device/i2c.h>
#include <gpio.h>
#include <soc/clock.h>
#include <soc/display.h>
#include <soc/emmc.h>
#include <soc/grf.h>
#include <soc/i2c.h>
#include <soc/spi.h>

#include <drivers/spi/tpm/tpm.h>
#include <ec/google/chromeec/ec.h>

static void configure_emmc(void)
{
	/* Host controller does not support programmable clock generator.
	 * If we don't do this setting, when we use phy to control the
	 * emmc clock(when clock exceed 50MHz), it will get wrong clock.
	 *
	 * Refer to TRM V0.3 Part 1 Chapter 15 PAGE 782 for this register.
	 * Please search "_CON11[7:0]" to locate register description.
	 */
	write32(&rk3399_grf->emmccore_con[11], RK_CLRSETBITS(0xff, 0));

	rkclk_configure_emmc();

	enable_emmc_clk();
}

static void configure_sdmmc(void)
{
	gpio_output(GPIO(4, D, 5), 1);  /* SDMMC_PWR_EN */
	gpio_output(GPIO(2, A, 2), 1);  /* SDMMC_SDIO_PWR_EN */
	/*
	 * SDMMC_DET_L is different on different board revisions.
	 * Ideally this and other deviations should come from a table
	 * which could be looked up by board revision.
	 */
	switch (board_id()) {
	case 0:  /* This is for Kevin proto 1. */
		gpio_input(GPIO(4, D, 2));
		break;
	default:
		gpio_input(GPIO(4, D, 0));
		break;
	}
	gpio_output(GPIO(2, D, 4), 0);  /* Keep the max voltage */

	write32(&rk3399_grf->iomux_sdmmc, IOMUX_SDMMC);
}

static void configure_display(void)
{
	/* set pinmux for edp HPD*/
	gpio_input_pulldown(GPIO(4, C, 7));
	write32(&rk3399_grf->iomux_edp_hotplug, IOMUX_EDP_HOTPLUG);

	gpio_output(GPIO(4, D, 3), 1); /* CPU3_EDP_VDDEN for P3.3V_DISP */
}


static int chromeec_hello(void)
{
	struct chromeec_command cec_cmd;
	struct ec_params_hello cmd_hello;
	struct ec_response_hello rsp_hello;
	cmd_hello.in_data = 0x10203040;
	cec_cmd.cmd_code = EC_CMD_HELLO;
	cec_cmd.cmd_version = 0;
	cec_cmd.cmd_data_in = &cmd_hello.in_data;
	cec_cmd.cmd_data_out = &rsp_hello.out_data;
	cec_cmd.cmd_size_in = sizeof(cmd_hello.in_data);
	cec_cmd.cmd_size_out = sizeof(rsp_hello.out_data);
	cec_cmd.cmd_dev_index = 0;
	google_chromeec_command(&cec_cmd);
	printk(BIOS_DEBUG, "Google Chrome EC: Hello got back %x status (%x)\n",
	       rsp_hello.out_data, cec_cmd.cmd_code);
	return cec_cmd.cmd_code;
}

static void mainboard_init(device_t dev)
{
	configure_sdmmc();
	configure_emmc();

	configure_display();

	/* select the pinmux for cr50, note that it is not very fast... */
	write32(&rk3399_grf->iomux_spi0, IOMUX_SPI0);
	rockchip_spi_init(0, 1500*KHz);
	if (!tpm2_init(spi_setup_slave(0, 0)))
		printk(BIOS_INFO, "spi interface to cr50 is up\n");

	rockchip_spi_init(5, 8250*KHz);
	write32(&rk3399_grf->iomux_spi5, IOMUX_SPI5);
	if (!chromeec_hello())
		printk(BIOS_INFO, "spi interface to ec is up\n");
}

static void enable_backlight_booster(void)
{
	const struct {
		uint8_t reg;
		uint8_t value;
	} i2c_writes[] = {
		{1, 0x84},
		{1, 0x85},
		{0, 0x26}
	};
	int i;
	const int booster_i2c_port = 0;
	uint8_t i2c_buf[2];
	struct i2c_seg i2c_command = { .read = 0, .chip = 0x2c,
				       .buf = i2c_buf, .len = sizeof(i2c_buf)
	};

	udelay(10000); /* Let the voltage settle. */

	/* Select pinmux for i2c0, which is the display backlight booster. */
	write32(&rk3399_pmugrf->iomux_i2c0_sda, IOMUX_I2C0_SDA);
	write32(&rk3399_pmugrf->iomux_i2c0_scl, IOMUX_I2C0_SCL);
	i2c_init(0, 100*KHz);

	for (i = 0; i < ARRAY_SIZE(i2c_writes); i++) {
		i2c_buf[0] = i2c_writes[i].reg;
		i2c_buf[1] = i2c_writes[i].value;
		i2c_transfer(booster_i2c_port, &i2c_command, 1);
	}
}

void mainboard_power_on_backlight(void)
{
	gpio_output(GPIO(1, C, 1), 1);  /* BL_EN */

	if (IS_ENABLED(CONFIG_BOARD_GOOGLE_GRU))
		enable_backlight_booster();
}

static void mainboard_enable(device_t dev)
{
	dev->ops->init = &mainboard_init;
}

struct chip_operations mainboard_ops = {
	.name = CONFIG_MAINBOARD_PART_NUMBER,
	.enable_dev = mainboard_enable,
};
