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
 */

#include <arch/io.h>
#include <console/console.h>
#include <gpio.h>
#include <soc/addressmap.h>
#include <soc/grf.h>
#include <soc/soc.h>
#include <stdlib.h>

struct rk3399_gpio_regs *gpio_port[] = {
	(struct rk3399_gpio_regs *)GPIO0_BASE,
	(struct rk3399_gpio_regs *)GPIO1_BASE,
	(struct rk3399_gpio_regs *)GPIO2_BASE,
	(struct rk3399_gpio_regs *)GPIO3_BASE,
	(struct rk3399_gpio_regs *)GPIO4_BASE,
};

enum {
	PULLNONE = 0,
	PULLUP,
	PULLDOWN
};

#define PMU_GPIO_PORT0 0
#define PMU_GPIO_PORT1 1

static void __gpio_input(gpio_t gpio, u32 pull)
{
	clrbits_le32(&gpio_port[gpio.port]->swporta_ddr, 1 << gpio.num);
	if (gpio.port == PMU_GPIO_PORT0 || gpio.port == PMU_GPIO_PORT1)
		clrsetbits_le32(&rk3399_pmugrf->gpio0_p[gpio.port][gpio.bank],
				3 << (gpio.idx * 2),  pull << (gpio.idx * 2));
	else
		write32(&rk3399_grf->gpio2_p[(gpio.port - 2)][gpio.bank],
			RK_CLRSETBITS(3 << (gpio.idx * 2),
				   pull << (gpio.idx * 2)));
}

void gpio_input(gpio_t gpio)
{
	__gpio_input(gpio, PULLNONE);
}

void gpio_input_pulldown(gpio_t gpio)
{
	__gpio_input(gpio, PULLDOWN);
}

void gpio_input_pullup(gpio_t gpio)
{
	__gpio_input(gpio, PULLUP);
}

int gpio_get(gpio_t gpio)
{
	return (read32(&gpio_port[gpio.port]->ext_porta) >> gpio.num) & 0x1;
}

void gpio_output(gpio_t gpio, int value)
{
	setbits_le32(&gpio_port[gpio.port]->swporta_ddr, 1 << gpio.num);
	clrsetbits_le32(&gpio_port[gpio.port]->swporta_dr, 1 << gpio.num,
							   !!value << gpio.num);
}
