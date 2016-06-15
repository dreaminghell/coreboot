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

#include <assert.h>
#include <arch/cache.h>
#include <arch/cpu.h>
#include <arch/exception.h>
#include <arch/io.h>
#include <arch/mmu.h>
#include <cbfs.h>
#include <console/console.h>
#include <delay.h>
#include <program_loading.h>
#include <romstage_handoff.h>
#include <soc/addressmap.h>
#include <soc/grf.h>
#include <soc/mmu_operations.h>
#include <soc/pwm.h>
#include <soc/tsadc.h>
#include <soc/sdram.h>
#include <symbols.h>
#include <soc/usb.h>

static const uint64_t dram_size =
		(uint64_t)min((uint64_t)CONFIG_DRAM_SIZE_MB * MiB, 0xf8000000);

static void regulate_vdd_centre(unsigned int mv)
{
	unsigned int duty_ns;
	const u32 period_ns = 2000;	/* pwm period: 2000ns */
	const u32 max_regulator_mv = 1350;	/* 1.35V */
	const u32 min_regulator_mv = 870;	/* 0.87V */

	write32(&rk3399_pmugrf->iomux_pwm_3a, IOMUX_PWM_3_A);
	write32(&rk3399_pmugrf->pwm3_sel, PWM3_SEL_A);
	assert((mv >= min_regulator_mv) && (mv <= max_regulator_mv));

	duty_ns = (max_regulator_mv - mv) * period_ns /
			(max_regulator_mv - min_regulator_mv);

	pwm_init(3, period_ns, duty_ns);
}

static void regulate_vdd_log(unsigned int mv)
{
	unsigned int duty_ns;
	const u32 period_ns = 2000;	/* pwm period: 2000ns */
	const u32 max_regulator_mv = 1400;	/* 1.4V */
	const u32 min_regulator_mv = 800;	/* 0.8V */

	write32(&rk3399_pmugrf->iomux_pwm_2, IOMUX_PWM_2);
	assert((mv >= min_regulator_mv) && (mv <= max_regulator_mv));

	duty_ns = (max_regulator_mv - mv) * period_ns /
			(max_regulator_mv - min_regulator_mv);

	pwm_init(2, period_ns, duty_ns);
}

void main(void)
{
	console_init();
	exception_init();
	regulate_vdd_log(950);
	regulate_vdd_centre(950);
	sdram_init(get_sdram_config());
	mmu_config_range((void *)0, (uintptr_t)dram_size, CACHED_MEM);
	mmu_config_range(_dma_coherent, _dma_coherent_size, UNCACHED_MEM);

	cbmem_initialize_empty();
	run_ramstage();
}
