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
#include <console/console.h>
#include <delay.h>
#include <device/device.h>
#include <soc/clock.h>
#include <soc/emmc.h>
#include <soc/grf.h>

/* emmc phy control */
#define PHYCTRL_PDB_MASK	0x1
#define PHYCTRL_PDB_SHIFT	0x0
#define PHYCTRL_PDB_PWR_ON	0x1
#define PHYCTRL_PDB_PWR_OFF	0x0
#define PHYCTRL_ENDLL_MASK	0x1
#define PHYCTRL_ENDLL_SHIFT     0x1
#define PHYCTRL_ENDLL_ENABLE	0x1
#define PHYCTRL_ENDLL_DISABLE	0x0
#define PHYCTRL_CALDONE_MASK	0x1
#define PHYCTRL_CALDONE_SHIFT   0x6
#define PHYCTRL_CALDONE_DONE	0x1
#define PHYCTRL_CALDONE_GOING	0x0
#define PHYCTRL_DLLRDY_MASK	0x1
#define PHYCTRL_DLLRDY_SHIFT	0x5
#define PHYCTRL_DLLRDY_DONE	0x1
#define PHYCTRL_DLLRDY_GOING	0x0
#define PHYCTRL_FREQSEL_200M	0x0
#define PHYCTRL_FREQSEL_50M	0x1
#define PHYCTRL_FREQSEL_100M	0x2
#define PHYCTRL_FREQSEL_150M	0x3
#define PHYCTRL_FREQSEL_MASK	0x3
#define PHYCTRL_FREQSEL_SHIFT	0xc
#define PHYCTRL_DR_MASK		0x7
#define PHYCTRL_DR_SHIFT	0x4
#define PHYCTRL_DR_50OHM	0x0
#define PHYCTRL_DR_33OHM	0x1
#define PHYCTRL_DR_66OHM	0x2
#define PHYCTRL_DR_100OHM	0x3
#define PHYCTRL_DR_40OHM	0x4

/* emmc clock control */
#define SDHCI_CLOCK_CONTROL	0x2c
#define SDHCI_CLOCK_CARD_EN	0x0004
#define SDHCI_CLOCK_INT_STABLE	0x0002
#define SDHCI_CLOCK_INT_EN	0x0001

void configure_emmc(void)
{
	int clk = 0;
	int caldone, dllrdy, timeout;

	write32(&rk3399_grf->emmccore_con[11], RK_CLRSETBITS(0xff, 0));
	rkclk_configure_emmc();

	/*
	 * if want to control emmc phy, we need to enalbe emmc clock
	 * so let's do it
	 */
	clk |= SDHCI_CLOCK_INT_EN;
	write32((void *)(EMMC_BASE + SDHCI_CLOCK_CONTROL), clk);

	/* Wait max 20 ms */
	timeout = 20;
	while (!((clk = read32((void *)(EMMC_BASE + SDHCI_CLOCK_CONTROL)))
		& SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			printk(BIOS_DEBUG, "Internal clock never stabilised.\n");
			return;
		}
		timeout--;
		udelay(1000);
	}

	clk |= SDHCI_CLOCK_CARD_EN;
	write32((void *)(EMMC_BASE + SDHCI_CLOCK_CONTROL), clk);

	/* enable emmc phy */
	write32(&rk3399_grf->emmcphy_con[6],
		RK_CLRSETBITS(PHYCTRL_PDB_MASK << PHYCTRL_PDB_SHIFT,
			      PHYCTRL_PDB_PWR_OFF << PHYCTRL_PDB_SHIFT));
	write32(&rk3399_grf->emmcphy_con[6],
		RK_CLRSETBITS(PHYCTRL_ENDLL_MASK << PHYCTRL_ENDLL_SHIFT,
			      PHYCTRL_ENDLL_DISABLE << PHYCTRL_ENDLL_SHIFT));
	udelay(10);
	write32(&rk3399_grf->emmcphy_con[6],
		RK_CLRSETBITS(PHYCTRL_PDB_MASK << PHYCTRL_PDB_SHIFT,
			      PHYCTRL_PDB_PWR_ON << PHYCTRL_PDB_SHIFT));
	udelay(10);
	caldone = read32(&rk3399_grf->emmcphy_status);
	caldone = (caldone >> PHYCTRL_CALDONE_SHIFT) & PHYCTRL_CALDONE_MASK;
	if (caldone != PHYCTRL_CALDONE_DONE) {
		printk(BIOS_DEBUG, "rockchip_emmc_phy_power: caldone timeout.\n");
		return;
	}
	write32(&rk3399_grf->emmcphy_con[6],
		RK_CLRSETBITS(PHYCTRL_ENDLL_MASK << PHYCTRL_ENDLL_SHIFT,
			      PHYCTRL_ENDLL_ENABLE << PHYCTRL_ENDLL_SHIFT));
	udelay(10);
	dllrdy = read32(&rk3399_grf->emmcphy_status);
	dllrdy = (dllrdy >> PHYCTRL_DLLRDY_SHIFT) & PHYCTRL_DLLRDY_MASK;
	if (dllrdy != PHYCTRL_DLLRDY_DONE)
		printk(BIOS_DEBUG, "rockchip_emmc_phy_power: dllrdy timeout.\n");
}
