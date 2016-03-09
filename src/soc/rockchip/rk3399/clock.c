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
#include <console/console.h>
#include <delay.h>
#include <lib.h>
#include <soc/addressmap.h>
#include <soc/clock.h>
#include <soc/grf.h>
#include <soc/soc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct pll_div {
	u32 refdiv;
	u32 fbdiv;
	u32 postdiv1;
	u32 postdiv2;
	u32 frac;
};

#define PLL_DIVISORS(hz, _refdiv, _postdiv1, _postdiv2) {\
	.refdiv = _refdiv,\
	.fbdiv = (u32)((u64)hz * _refdiv * _postdiv1 * _postdiv2 / OSC_HZ),\
	.postdiv1 = _postdiv1, .postdiv2 = _postdiv2};\
	_Static_assert(((u64)hz * _refdiv * _postdiv1 * _postdiv2 / OSC_HZ) *\
			 OSC_HZ / (_refdiv * _postdiv1 * _postdiv2) == hz,\
			 #hz "Hz cannot be hit with PLL "\
			 "divisors on line " STRINGIFY(__LINE__));

static const struct pll_div gpll_init_cfg = PLL_DIVISORS(GPLL_HZ, 2, 2, 1);
static const struct pll_div cpll_init_cfg = PLL_DIVISORS(CPLL_HZ, 1, 2, 2);
static const struct pll_div ppll_init_cfg = PLL_DIVISORS(PPLL_HZ, 2, 2, 1);

static const struct pll_div apll_l_1600_cfg = PLL_DIVISORS(1600*MHz, 3, 1, 1);
static const struct pll_div apll_l_600_cfg = PLL_DIVISORS(600*MHz, 1, 2, 1);

static const struct pll_div *apll_l_cfgs[] = {
	[APLL_L_1600_MHZ] = &apll_l_1600_cfg,
	[APLL_L_600_MHZ] = &apll_l_600_cfg,
};

enum {
	/* PLL_CON0 */
	PLL_FBDIV_MASK			= 0xfff,
	PLL_FBDIV_SHIFT			= 0,

	/* PLL_CON1 */
	PLL_POSTDIV2_MASK		= 0x7,
	PLL_POSTDIV2_SHIFT		= 12,
	PLL_POSTDIV1_MASK		= 0x7,
	PLL_POSTDIV1_SHIFT		= 8,
	PLL_REFDIV_MASK			= 0x3f,
	PLL_REFDIV_SHIFT		= 0,

	/* PLL_CON2 */
	PLL_LOCK_STATUS_MASK		= 1,
	PLL_LOCK_STATUS_SHIFT		= 31,
	PLL_FRACDIV_MASK		= 0xffffff,
	PLL_FRACDIV_SHIFT		= 0,

	/* PLL_CON3 */
	PLL_MODE_MASK			= 3,
	PLL_MODE_SHIFT			= 8,
	PLL_MODE_SLOW			= 0,
	PLL_MODE_NORM,
	PLL_MODE_DEEP,
	PLL_DSMPD_MASK			= 1,
	PLL_DSMPD_SHIFT			= 3,
	PLL_INTEGER_MODE		= 1,

	/* PMUCRU_CLKSEL_CON0 */
	PMU_PCLK_DIV_CON_MASK		= 0x1f,
	PMU_PCLK_DIV_CON_SHIFT		= 0,

	/* PMUCRU_CLKSEL_CON1 */
	SPI3_PLL_SEL_MASK		= 1,
	SPI3_PLL_SEL_SHIFT		= 7,
	SPI3_PLL_SEL_24M		= 0,
	SPI3_PLL_SEL_PPLL		= 1,
	SPI3_DIV_CON_MASK		= 0x7f,
	SPI3_DIV_CON_SHIFT		= 0x0,

	/* PMUCRU_CLKSEL_CON2 */
	I2C_DIV_CON_MASK		= 0x7f,
	I2C8_DIV_CON_SHIFT		= 8,
	I2C0_DIV_CON_SHIFT		= 0,

	/* PMUCRU_CLKSEL_CON3 */
	I2C4_DIV_CON_SHIFT		= 0,

	/* CLKSEL_CON0 */
	ACLKM_CORE_L_DIV_CON_MASK	= 0x1f,
	ACLKM_CORE_L_DIV_CON_SHIFT	= 8,
	CLK_CORE_L_PLL_SEL_MASK		= 3,
	CLK_CORE_L_PLL_SEL_SHIFT	= 6,
	CLK_CORE_L_PLL_SEL_ALPLL	= 0,
	CLK_CORE_L_PLL_SEL_ABPLL	= 1,
	CLK_CORE_L_PLL_SEL_DPLL		= 2,
	CLK_CORE_L_PLL_SEL_GPLL		= 3,
	CLK_CORE_L_DIV_MASK		= 0x1f,
	CLK_CORE_L_DIV_SHIFT		= 0,

	/* CLKSEL_CON1 */
	PCLK_DBG_L_DIV_MASK		= 0x1f,
	PCLK_DBG_L_DIV_SHIFT		= 0x8,
	ATCLK_CORE_L_DIV_MASK		= 0x1f,
	ATCLK_CORE_L_DIV_SHIFT		= 0,

	/* CLKSEL_CON14 */
	PCLK_PERIHP_DIV_CON_MASK	= 0x7,
	PCLK_PERIHP_DIV_CON_SHIFT	= 12,
	HCLK_PERIHP_DIV_CON_MASK	= 3,
	HCLK_PERIHP_DIV_CON_SHIFT	= 8,
	ACLK_PERIHP_PLL_SEL_MASK	= 1,
	ACLK_PERIHP_PLL_SEL_SHIFT	= 7,
	ACLK_PERIHP_PLL_SEL_CPLL	= 0,
	ACLK_PERIHP_PLL_SEL_GPLL	= 1,
	ACLK_PERIHP_DIV_CON_MASK	= 0x1f,
	ACLK_PERIHP_DIV_CON_SHIFT	= 0,

	/* CLKSEL_CON21 */
	ACLK_EMMC_PLL_SEL_MASK          = 1,
	ACLK_EMMC_PLL_SEL_SHIFT         = 7,
	ACLK_EMMC_PLL_SEL_GPLL          = 1,
	ACLK_EMMC_DIV_CON_MASK          = 0x1f,
	ACLK_EMMC_DIV_CON_SHIFT         = 0,

	/* CLKSEL_CON22 */
	CLK_EMMC_PLL_MASK               = 7,
	CLK_EMMC_PLL_SHIFT              = 8,
	CLK_EMMC_PLL_SEL_GPLL           = 1,
	CLK_EMMC_DIV_CON_MASK           = 0x7f,
	CLK_EMMC_DIV_CON_SHIFT          = 0x0,

	/* CLKSEL_CON23 */
	PCLK_PERILP0_DIV_CON_MASK	= 0x7,
	PCLK_PERILP0_DIV_CON_SHIFT	= 12,
	HCLK_PERILP0_DIV_CON_MASK	= 3,
	HCLK_PERILP0_DIV_CON_SHIFT	= 8,
	ACLK_PERILP0_PLL_SEL_MASK	= 1,
	ACLK_PERILP0_PLL_SEL_SHIFT	= 7,
	ACLK_PERILP0_PLL_SEL_CPLL	= 0,
	ACLK_PERILP0_PLL_SEL_GPLL	= 1,
	ACLK_PERILP0_DIV_CON_MASK	= 0x1f,
	ACLK_PERILP0_DIV_CON_SHIFT	= 0,

	/* CLKSEL_CON25 */
	PCLK_PERILP1_DIV_CON_MASK	= 0x7,
	PCLK_PERILP1_DIV_CON_SHIFT	= 8,
	HCLK_PERILP1_PLL_SEL_MASK	= 1,
	HCLK_PERILP1_PLL_SEL_SHIFT	= 7,
	HCLK_PERILP1_PLL_SEL_CPLL	= 0,
	HCLK_PERILP1_PLL_SEL_GPLL	= 1,
	HCLK_PERILP1_DIV_CON_MASK	= 0x1f,
	HCLK_PERILP1_DIV_CON_SHIFT	= 0,

	/* CLKSEL_CON58 */
	CLK_SPI_PLL_SEL_MASK		= 1,
	CLK_SPI_PLL_SEL_CPLL		= 0,
	CLK_SPI_PLL_SEL_GPLL		= 1,
	CLK_SPI_PLL_DIV_CON_MASK	= 0x7f,
	CLK_SPI5_PLL_DIV_CON_SHIFT	= 8,
	CLK_SPI5_PLL_SEL_SHIFT		= 15,

	/* CLKSEL_CON59 */
	CLK_SPI1_PLL_SEL_SHIFT		= 15,
	CLK_SPI1_PLL_DIV_CON_SHIFT	= 8,
	CLK_SPI0_PLL_SEL_SHIFT		= 7,
	CLK_SPI0_PLL_DIV_CON_SHIFT	= 0,

	/* CLKSEL_CON60 */
	CLK_SPI4_PLL_SEL_SHIFT		= 15,
	CLK_SPI4_PLL_DIV_CON_SHIFT	= 8,
	CLK_SPI2_PLL_SEL_SHIFT		= 7,
	CLK_SPI2_PLL_DIV_CON_SHIFT	= 0,

	/* CLKSEL_CON61 */
	CLK_I2C_PLL_SEL_MASK		= 1,
	CLK_I2C_PLL_SEL_CPLL		= 0,
	CLK_I2C_PLL_SEL_GPLL		= 1,
	CLK_I2C5_PLL_SEL_SHIFT		= 15,
	CLK_I2C5_DIV_CON_SHIFT		= 8,
	CLK_I2C1_PLL_SEL_SHIFT		= 7,
	CLK_I2C1_DIV_CON_SHIFT		= 0,

	/* CLKSEL_CON62 */
	CLK_I2C6_PLL_SEL_SHIFT		= 15,
	CLK_I2C6_DIV_CON_SHIFT		= 8,
	CLK_I2C2_PLL_SEL_SHIFT		= 7,
	CLK_I2C2_DIV_CON_SHIFT		= 0,

	/* CLKSEL_CON63 */
	CLK_I2C7_PLL_SEL_SHIFT		= 15,
	CLK_I2C7_DIV_CON_SHIFT		= 8,
	CLK_I2C3_PLL_SEL_SHIFT		= 7,
	CLK_I2C3_DIV_CON_SHIFT		= 0,

	/* CRU_SOFTRST_CON4 */
	RESETN_DDR0_REQ_MASK		= 1,
	RESETN_DDR0_REQ_SHIFT		= 8,
	RESETN_DDRPHY0_REQ_MASK		= 1,
	RESETN_DDRPHY0_REQ_SHIFT	= 9,
	RESETN_DDR1_REQ_MASK		= 1,
	RESETN_DDR1_REQ_SHIFT		= 12,
	RESETN_DDRPHY1_REQ_MASK		= 1,
	RESETN_DDRPHY1_REQ_SHIFT	= 13,
};

#define VCO_MAX_KHZ	(3200 * (MHz / KHz))
#define VCO_MIN_KHZ	(800 * (MHz / KHz))
#define OUTPUT_MAX_KHZ	(3200 * (MHz / KHz))
#define OUTPUT_MIN_KHZ	(16 * (MHz / KHz))

static void rkclk_set_pll(u32 *pll_con, const struct pll_div *div)
{
	/* All PLLs have same VCO and output frequency range restrictions. */
	u32 vco_khz = OSC_HZ / 1000 * div->fbdiv / div->refdiv;
	u32 output_khz = vco_khz / div->postdiv1 / div->postdiv2;

	printk(BIOS_DEBUG, "PLL at %p: fbdiv=%d, refdiv=%d, postdiv1=%d,\
			    postdiv2=%d, vco=%u khz, output=%u khz\n",
			   pll_con, div->fbdiv, div->refdiv, div->postdiv1,
			   div->postdiv2, vco_khz, output_khz);
	assert(vco_khz >= VCO_MIN_KHZ && vco_khz <= VCO_MAX_KHZ &&
	       output_khz >= OUTPUT_MIN_KHZ && output_khz <= OUTPUT_MAX_KHZ &&
	       div->fbdiv >= 16 && div->fbdiv <= 3200);

	/* pll enter slow mode */
	rk_clrsetreg(&pll_con[3], PLL_MODE_MASK << PLL_MODE_SHIFT,
				  PLL_MODE_SLOW << PLL_MODE_SHIFT);

	/* use integer mode */
	rk_clrsetreg(&pll_con[3], PLL_DSMPD_MASK << PLL_DSMPD_SHIFT,
				  PLL_INTEGER_MODE << PLL_DSMPD_SHIFT);

	rk_clrsetreg(&pll_con[0], PLL_FBDIV_MASK << PLL_FBDIV_SHIFT,
				  div->fbdiv << PLL_FBDIV_SHIFT);
	rk_clrsetreg(&pll_con[1], PLL_POSTDIV2_MASK << PLL_POSTDIV2_SHIFT |
				  PLL_POSTDIV1_MASK << PLL_POSTDIV1_SHIFT |
				  PLL_REFDIV_MASK | PLL_REFDIV_SHIFT,
				  (div->postdiv2 << PLL_POSTDIV2_SHIFT) |
				  (div->postdiv1 << PLL_POSTDIV1_SHIFT) |
				  (div->refdiv << PLL_REFDIV_SHIFT));

	/* waiting for pll lock */
	while (!(read32(&pll_con[2]) & (1 << PLL_LOCK_STATUS_SHIFT)))
		udelay(1);

	/* pll enter normal mode */
	rk_clrsetreg(&pll_con[3], PLL_MODE_MASK << PLL_MODE_SHIFT,
				 PLL_MODE_NORM << PLL_MODE_SHIFT);
}

void rkclk_init(void)
{
	u32 aclk_div;
	u32 hclk_div;
	u32 pclk_div;

	/* some cru register need reset */
	write32(&cru_ptr->clksel_con[12], 0xffff4101);
	write32(&cru_ptr->clksel_con[19], 0xffff033f);
	write32(&cru_ptr->clksel_con[56], 0x00030003);

	/* configure pmu pll */
	rkclk_set_pll(&pmucru_ptr->ppll_con[0], &ppll_init_cfg);

	/* configure pmu plck */
	pclk_div = PPLL_HZ / PMU_PCLK_HZ - 1;
	assert((pclk_div + 1) * PMU_PCLK_HZ == PPLL_HZ && pclk_div < 0x1f);
	rk_clrsetreg(&pmucru_ptr->pmucru_clksel[0],
		     PMU_PCLK_DIV_CON_MASK << PMU_PCLK_DIV_CON_SHIFT,
		     pclk_div << PMU_PCLK_DIV_CON_SHIFT);

	/* configure gpll cpll */
	rkclk_set_pll(&cru_ptr->gpll_con[0], &gpll_init_cfg);
	rkclk_set_pll(&cru_ptr->cpll_con[0], &cpll_init_cfg);

	/* configure perihp aclk, hclk, pclk */
	aclk_div = GPLL_HZ / PERIHP_ACLK_HZ - 1;
	assert((aclk_div + 1) * PERIHP_ACLK_HZ == GPLL_HZ && aclk_div < 0x1f);

	hclk_div = PERIHP_ACLK_HZ / PERIHP_HCLK_HZ - 1;
	assert((hclk_div + 1) * PERIHP_HCLK_HZ ==
		PERIHP_ACLK_HZ && (hclk_div < 0x4));

	pclk_div = PERIHP_ACLK_HZ / PERIHP_PCLK_HZ - 1;
	assert((pclk_div + 1) * PERIHP_PCLK_HZ ==
		PERIHP_ACLK_HZ && (pclk_div < 0x7));

	rk_clrsetreg(&cru_ptr->clksel_con[14],
		     PCLK_PERIHP_DIV_CON_MASK << PCLK_PERIHP_DIV_CON_SHIFT |
		     HCLK_PERIHP_DIV_CON_MASK << HCLK_PERIHP_DIV_CON_SHIFT |
		     ACLK_PERIHP_PLL_SEL_MASK << ACLK_PERIHP_PLL_SEL_SHIFT |
		     ACLK_PERIHP_DIV_CON_MASK << ACLK_PERIHP_DIV_CON_SHIFT,
		     pclk_div << PCLK_PERIHP_DIV_CON_SHIFT |
		     hclk_div << HCLK_PERIHP_DIV_CON_SHIFT |
		     ACLK_PERIHP_PLL_SEL_GPLL << ACLK_PERIHP_PLL_SEL_SHIFT |
		     aclk_div << ACLK_PERIHP_DIV_CON_SHIFT);

	/* configure perilp0 aclk, hclk, pclk */
	aclk_div = GPLL_HZ / PERILP0_ACLK_HZ - 1;
	assert((aclk_div + 1) * PERILP0_ACLK_HZ == GPLL_HZ && aclk_div < 0x1f);

	hclk_div = PERILP0_ACLK_HZ / PERILP0_HCLK_HZ - 1;
	assert((hclk_div + 1) * PERILP0_HCLK_HZ ==
		PERILP0_ACLK_HZ && (hclk_div < 0x4));

	pclk_div = PERILP0_ACLK_HZ / PERILP0_PCLK_HZ - 1;
	assert((pclk_div + 1) * PERILP0_PCLK_HZ ==
		PERILP0_ACLK_HZ && (pclk_div < 0x7));

	rk_clrsetreg(&cru_ptr->clksel_con[23],
		     PCLK_PERILP0_DIV_CON_MASK << PCLK_PERILP0_DIV_CON_SHIFT |
		     HCLK_PERILP0_DIV_CON_MASK << HCLK_PERILP0_DIV_CON_SHIFT |
		     ACLK_PERILP0_PLL_SEL_MASK << ACLK_PERILP0_PLL_SEL_SHIFT |
		     ACLK_PERILP0_DIV_CON_MASK << ACLK_PERILP0_DIV_CON_SHIFT,
		     pclk_div << PCLK_PERILP0_DIV_CON_SHIFT |
		     hclk_div << HCLK_PERILP0_DIV_CON_SHIFT |
		     ACLK_PERILP0_PLL_SEL_GPLL << ACLK_PERILP0_PLL_SEL_SHIFT |
		     aclk_div << ACLK_PERILP0_DIV_CON_SHIFT);

	/* perilp1 hclk select gpll as source */
	hclk_div = GPLL_HZ / PERILP1_HCLK_HZ - 1;
	assert((hclk_div + 1) * PERILP1_HCLK_HZ ==
		GPLL_HZ && (hclk_div < 0x1f));

	pclk_div = PERILP1_HCLK_HZ / PERILP1_HCLK_HZ - 1;
	assert((pclk_div + 1) * PERILP1_HCLK_HZ ==
		PERILP1_HCLK_HZ && (hclk_div < 0x7));

	rk_clrsetreg(&cru_ptr->clksel_con[25],
		     PCLK_PERILP1_DIV_CON_MASK << PCLK_PERILP1_DIV_CON_SHIFT |
		     HCLK_PERILP1_DIV_CON_MASK << HCLK_PERILP1_DIV_CON_SHIFT |
		     HCLK_PERILP1_PLL_SEL_MASK << HCLK_PERILP1_PLL_SEL_SHIFT,
		     pclk_div << PCLK_PERILP1_DIV_CON_SHIFT |
		     hclk_div << HCLK_PERILP1_DIV_CON_SHIFT |
		     HCLK_PERILP1_PLL_SEL_GPLL << HCLK_PERILP1_PLL_SEL_SHIFT);
}

void rkclk_configure_cpu(enum apll_l_frequencies apll_l_freq)
{
	u32 atclkm_div;
	u32 pclk_dbg_div;
	u32 atclk_div;

	rkclk_set_pll(&cru_ptr->apll_l_con[0], apll_l_cfgs[apll_l_freq]);

	atclkm_div = APLL_HZ / ATCLKM_CORE_HZ - 1;
	assert((atclkm_div + 1) * ATCLKM_CORE_HZ == APLL_HZ && atclkm_div < 0x1f);

	pclk_dbg_div = APLL_HZ / PCLK_DBG_HZ - 1;
	assert((pclk_dbg_div + 1) * PCLK_DBG_HZ == APLL_HZ && pclk_dbg_div < 0x1f);

	atclk_div = APLL_HZ / ATCLK_CORE_HZ - 1;
	assert((atclk_div + 1) * ATCLK_CORE_HZ == APLL_HZ && atclk_div < 0x1f);

	rk_clrsetreg(&cru_ptr->clksel_con[0],
		     ACLKM_CORE_L_DIV_CON_MASK << ACLKM_CORE_L_DIV_CON_SHIFT |
		     CLK_CORE_L_PLL_SEL_MASK << CLK_CORE_L_PLL_SEL_MASK |
		     CLK_CORE_L_DIV_MASK << CLK_CORE_L_DIV_SHIFT,
		     atclkm_div << ACLKM_CORE_L_DIV_CON_SHIFT |
		     CLK_CORE_L_PLL_SEL_ALPLL << CLK_CORE_L_PLL_SEL_SHIFT |
		     0 << CLK_CORE_L_DIV_SHIFT);

	rk_clrsetreg(&cru_ptr->clksel_con[1],
		     PCLK_DBG_L_DIV_MASK << PCLK_DBG_L_DIV_SHIFT |
		     ATCLK_CORE_L_DIV_MASK << ATCLK_CORE_L_DIV_SHIFT,
		     pclk_dbg_div << PCLK_DBG_L_DIV_SHIFT |
		     atclk_div << ATCLK_CORE_L_DIV_SHIFT);
}

void rkclk_ddr_reset(u32 ch, u32 ctl, u32 phy)
{
	if (ch)
		rk_clrsetreg(&cru_ptr->softrst_con[4],
			     RESETN_DDR1_REQ_MASK << RESETN_DDR1_REQ_SHIFT |
			     RESETN_DDRPHY1_REQ_MASK << RESETN_DDRPHY1_REQ_SHIFT,
			     ctl << RESETN_DDR1_REQ_SHIFT |
			     phy << RESETN_DDRPHY1_REQ_SHIFT);
	else
		rk_clrsetreg(&cru_ptr->softrst_con[4],
			     RESETN_DDR0_REQ_MASK << RESETN_DDR0_REQ_SHIFT |
			     RESETN_DDRPHY0_REQ_MASK << RESETN_DDRPHY0_REQ_SHIFT,
			     ctl << RESETN_DDR0_REQ_SHIFT |
			     phy << RESETN_DDRPHY0_REQ_SHIFT);
}

void rkclk_configure_ddr(unsigned int hz)
{
	struct pll_div dpll_cfg;

	/* IC ECO bug, need to set thiw register */
	write32(&rk3399_pmusgrf->ddr_rgn_con[16], 0xc000c000);
	switch (hz) {
	case 200*MHz:
		dpll_cfg = (struct pll_div)
		{.refdiv = 1, .fbdiv = 50, .postdiv1 = 6, .postdiv2 = 1};
		break;
	case 300*MHz:
		dpll_cfg = (struct pll_div)
		{.refdiv = 2, .fbdiv = 100, .postdiv1 = 4, .postdiv2 = 1};
		break;
	case 666*MHz:
		dpll_cfg = (struct pll_div)
		{.refdiv = 2, .fbdiv = 111, .postdiv1 = 2, .postdiv2 = 1};
		break;
	case 800*MHz:
		dpll_cfg = (struct pll_div)
		{.refdiv = 1, .fbdiv = 100, .postdiv1 = 3, .postdiv2 = 1};
		break;
	default:
		die("Unsupported SDRAM frequency, add to clock.c!");
	}
	rkclk_set_pll(&cru_ptr->dpll_con[0], &dpll_cfg);
}

void rkclk_configure_spi(unsigned int bus, unsigned int hz)
{
	int src_clk_div;

	/* spi3 src clock from ppll*/
	if (bus == 3) {
		src_clk_div = PPLL_HZ / hz;
		assert((src_clk_div - 1 < 127) && (src_clk_div * hz == PPLL_HZ));
	} else { /* spi0,1,2,4,5 src clock from gpll*/
		src_clk_div = GPLL_HZ / hz;
		assert((src_clk_div - 1 < 127) && (src_clk_div * hz == GPLL_HZ));
	}

	switch (bus) {
	case 0:
		rk_clrsetreg(&cru_ptr->clksel_con[59],
			     CLK_SPI_PLL_SEL_MASK << CLK_SPI0_PLL_SEL_SHIFT |
			     CLK_SPI_PLL_DIV_CON_MASK << CLK_SPI0_PLL_DIV_CON_SHIFT,
			     CLK_SPI_PLL_SEL_GPLL << CLK_SPI0_PLL_SEL_SHIFT |
			     (src_clk_div - 1) << CLK_SPI0_PLL_DIV_CON_SHIFT);
		break;
	case 1:
		rk_clrsetreg(&cru_ptr->clksel_con[59],
			     CLK_SPI_PLL_SEL_MASK << CLK_SPI1_PLL_SEL_SHIFT |
			     CLK_SPI_PLL_DIV_CON_MASK << CLK_SPI1_PLL_DIV_CON_SHIFT,
			     CLK_SPI_PLL_SEL_GPLL << CLK_SPI1_PLL_SEL_SHIFT |
			     (src_clk_div - 1) << CLK_SPI1_PLL_DIV_CON_SHIFT);
		break;
	case 2:
		rk_clrsetreg(&cru_ptr->clksel_con[60],
			     CLK_SPI_PLL_SEL_MASK << CLK_SPI2_PLL_SEL_SHIFT |
			     CLK_SPI_PLL_DIV_CON_MASK << CLK_SPI2_PLL_DIV_CON_SHIFT,
			     CLK_SPI_PLL_SEL_GPLL << CLK_SPI2_PLL_SEL_SHIFT |
			     (src_clk_div - 1) << CLK_SPI2_PLL_DIV_CON_SHIFT);
		break;
	case 3:
		rk_clrsetreg(&pmucru_ptr->pmucru_clksel[1],
			     SPI3_PLL_SEL_MASK << SPI3_PLL_SEL_SHIFT |
			     SPI3_DIV_CON_MASK << SPI3_DIV_CON_SHIFT,
			     SPI3_PLL_SEL_PPLL << SPI3_PLL_SEL_SHIFT |
			     (src_clk_div - 1) << SPI3_DIV_CON_SHIFT);
		break;
	case 4:
		rk_clrsetreg(&cru_ptr->clksel_con[60],
			     CLK_SPI_PLL_SEL_MASK << CLK_SPI4_PLL_SEL_SHIFT |
			     CLK_SPI_PLL_DIV_CON_MASK << CLK_SPI4_PLL_DIV_CON_SHIFT,
			     CLK_SPI_PLL_SEL_GPLL << CLK_SPI4_PLL_SEL_SHIFT |
			     (src_clk_div - 1) << CLK_SPI4_PLL_DIV_CON_SHIFT);
		break;
	case 5:
		rk_clrsetreg(&cru_ptr->clksel_con[58],
			     CLK_SPI_PLL_SEL_MASK << CLK_SPI5_PLL_SEL_SHIFT |
			     CLK_SPI_PLL_DIV_CON_MASK << CLK_SPI5_PLL_DIV_CON_SHIFT,
			     CLK_SPI_PLL_SEL_GPLL << CLK_SPI5_PLL_SEL_SHIFT |
			     (src_clk_div - 1) << CLK_SPI5_PLL_DIV_CON_SHIFT);
		break;
	default:
		printk(BIOS_ERR, "do not support this spi bus\n");
	}
}

void rkclk_configure_i2c(unsigned int bus, unsigned int hz)
{
	int src_clk_div;

	/* i2c0, 4, 8 src clock from ppll*/
	if (bus == 0 || bus == 4 || bus == 8) {
		src_clk_div = PPLL_HZ / hz;
		assert((src_clk_div - 1 < 127) && (src_clk_div * hz == PPLL_HZ));
	} else {/* i2c 1, 2, 3, 5, 6, 7 src clock from gpll*/
		src_clk_div = GPLL_HZ / hz;
		assert((src_clk_div - 1 < 127) && (src_clk_div * hz == GPLL_HZ));
	}

	switch (bus) {
	case 0:
		rk_clrsetreg(&pmucru_ptr->pmucru_clksel[2],
			     I2C_DIV_CON_MASK << I2C0_DIV_CON_SHIFT,
			     (src_clk_div - 1) << I2C0_DIV_CON_SHIFT);
		break;
	case 1:
		rk_clrsetreg(&cru_ptr->clksel_con[61],
			     I2C_DIV_CON_MASK << CLK_I2C1_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_MASK << CLK_I2C1_PLL_SEL_SHIFT,
			     (src_clk_div - 1) << CLK_I2C1_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_GPLL << CLK_I2C1_PLL_SEL_SHIFT);
		break;
	case 2:
		rk_clrsetreg(&cru_ptr->clksel_con[62],
			     I2C_DIV_CON_MASK << CLK_I2C2_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_MASK << CLK_I2C2_PLL_SEL_SHIFT,
			     (src_clk_div - 1) << CLK_I2C2_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_GPLL << CLK_I2C2_PLL_SEL_SHIFT);
		break;
	case 3:
		rk_clrsetreg(&cru_ptr->clksel_con[63],
			     I2C_DIV_CON_MASK << CLK_I2C3_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_MASK << CLK_I2C3_PLL_SEL_SHIFT,
			     (src_clk_div - 1) << CLK_I2C3_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_GPLL << CLK_I2C3_PLL_SEL_SHIFT);
		break;
	case 4:
		rk_clrsetreg(&pmucru_ptr->pmucru_clksel[3],
			     I2C_DIV_CON_MASK << I2C4_DIV_CON_SHIFT,
			     (src_clk_div - 1) << I2C4_DIV_CON_SHIFT);
		break;
	case 5:
		rk_clrsetreg(&cru_ptr->clksel_con[61],
			     I2C_DIV_CON_MASK << CLK_I2C5_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_MASK << CLK_I2C5_PLL_SEL_SHIFT,
			     (src_clk_div - 1) << CLK_I2C5_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_GPLL << CLK_I2C5_PLL_SEL_SHIFT);
		break;
	case 6:
		rk_clrsetreg(&cru_ptr->clksel_con[62],
			     I2C_DIV_CON_MASK << CLK_I2C6_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_MASK << CLK_I2C6_PLL_SEL_SHIFT,
			     (src_clk_div - 1) << CLK_I2C6_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_GPLL << CLK_I2C6_PLL_SEL_SHIFT);
		break;
	case 7:
		rk_clrsetreg(&cru_ptr->clksel_con[63],
			     I2C_DIV_CON_MASK << CLK_I2C7_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_MASK << CLK_I2C7_PLL_SEL_SHIFT,
			     (src_clk_div - 1) << CLK_I2C7_DIV_CON_SHIFT |
			     CLK_I2C_PLL_SEL_GPLL << CLK_I2C7_PLL_SEL_SHIFT);
		break;
	case 8:
		rk_clrsetreg(&pmucru_ptr->pmucru_clksel[2],
			     I2C_DIV_CON_MASK << I2C8_DIV_CON_SHIFT,
			     (src_clk_div - 1) << I2C8_DIV_CON_SHIFT);
		break;
	default:
		printk(BIOS_ERR, "do not support this i2c bus\n");
	}
}

void rkclk_configure_tsadc(unsigned int hz)
{
	int src_clk_div;

	/* use 24M as src clock */
	src_clk_div = OSC_HZ / hz;
	assert((src_clk_div - 1 < 1024) && (src_clk_div * hz == OSC_HZ));

	rk_clrsetreg(&cru_ptr->clksel_con[27],
			CLK_TSADC_DIV_CON_MASK << CLK_TSADC_DIV_CON_SHIFT |
			CLK_TSADC_SEL_MASK << CLK_TSADC_SEL_SHIFT,
			src_clk_div << CLK_TSADC_DIV_CON_SHIFT |
			0 << CLK_TSADC_SEL_SHIFT);
}

void rkclk_configure_emmc(void)
{
        int src_clk_div;

        /* aclk emmc set to 198Mhz, source clock from gpll */
        src_clk_div = GPLL_HZ / 198000000;
        assert((src_clk_div - 1 < 31) && (src_clk_div * 198000000 == GPLL_HZ));

        rk_clrsetreg(&cru_ptr->clksel_con[21],
                     ACLK_EMMC_PLL_SEL_MASK << ACLK_EMMC_PLL_SEL_SHIFT |
                     ACLK_EMMC_DIV_CON_MASK << ACLK_EMMC_DIV_CON_SHIFT,
                     ACLK_EMMC_PLL_SEL_GPLL << ACLK_EMMC_PLL_SEL_SHIFT |
                     (src_clk_div - 1) << ACLK_EMMC_DIV_CON_SHIFT);

        /* clk emmc set to 198Mhz, source clock from gpll */
        src_clk_div = GPLL_HZ / 198000000;
        assert((src_clk_div - 1 < 127) && (src_clk_div * 198000000 == GPLL_HZ));

        rk_clrsetreg(&cru_ptr->clksel_con[22],
                     CLK_EMMC_PLL_MASK << CLK_EMMC_PLL_SHIFT |
                     CLK_EMMC_DIV_CON_MASK << CLK_EMMC_DIV_CON_SHIFT,
                     CLK_EMMC_PLL_SEL_GPLL << CLK_EMMC_PLL_SHIFT |
                     (src_clk_div - 1) << CLK_EMMC_DIV_CON_SHIFT);
}
