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

#include <boardid.h>
#include <console/console.h>
#include <stdlib.h>
#include <soc/saradc.h>
#include <delay.h>
uint8_t board_id(void)
{
	int value;

	value = get_saradc_value(0);

	printk(BIOS_DEBUG, "board id adc = %d\n", value);
	if (value > 350)
		return 2;
	else
		return 1;
}

uint32_t ram_code(void)
{
	int value;

	value = get_saradc_value(3);
	printk(BIOS_DEBUG, "ram id adc = %d\n", value);
	if (value > 450)
		return 0;
	else
		return 1;
}
