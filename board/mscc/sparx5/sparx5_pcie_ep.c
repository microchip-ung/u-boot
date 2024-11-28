// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2024 Microchip Technology Inc. and its subsidiaries.
 *
 * Configure PCIe Endpoint in Sparx5
 *
 */

#include <debug_uart.h>
#include <linux/sizes.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>

#include "sparx5_regs.h"
#include "ddr_platform.h"

enum pcie_ep_access_type {
	PCIE_CMU_ACCESS = 0,
	PCIE_LANE_ACCESS = 1,
};

struct pcie_ep_config {
	uint32_t max_link_speed;
	uint32_t vendor_id;
	uint32_t device_id;
};

static void pcie_ep_serdes_reset(void)
{
	uintptr_t pcie_phy_wrap = SPARX5_PCIE_PHY_WRAP_BASE;

	INFO("pcie: PHY register block reset\n");
	mmio_clrsetbits_32(PCIE_PHY_WRAP_PCIE_PHY_CFG(pcie_phy_wrap),
			   PCIE_PHY_WRAP_PCIE_PHY_CFG_EXT_CFG_RST_M,
			   PCIE_PHY_WRAP_PCIE_PHY_CFG_EXT_CFG_RST(1));

	udelay(1);
	INFO("pcie: Releasing PHY register block reset\n");
	mmio_clrsetbits_32(PCIE_PHY_WRAP_PCIE_PHY_CFG(pcie_phy_wrap),
			   PCIE_PHY_WRAP_PCIE_PHY_CFG_EXT_CFG_RST_M,
			   PCIE_PHY_WRAP_PCIE_PHY_CFG_EXT_CFG_RST(0));
}

static void pcie_ep_reset_pipe(bool enable)
{
	uintptr_t pcie_phy_wrap = SPARX5_PCIE_PHY_WRAP_BASE;

	mmio_clrsetbits_32(PCIE_PHY_WRAP_PCIE_PHY_CFG(pcie_phy_wrap),
			   PCIE_PHY_WRAP_PCIE_PHY_CFG_PIPE_RST_M,
			   PCIE_PHY_WRAP_PCIE_PHY_CFG_PIPE_RST(enable));
}

static void pcie_ep_ssc_clock(void)
{
	uintptr_t pcie_phy_pma = SPARX5_PCIE_PHY_PMA_BASE;
	uintptr_t pcie_phy_pcs = SPARX5_PCIE_PHY_PCS_BASE;

	/* Setting up PCS for Spread spectrum clocking according to GUC mail August 22 2019 */
	mmio_clrsetbits_32(PCIE_PHY_PCS_PHY_LINK_3E(pcie_phy_pcs),
			   PCIE_PHY_PCS_PHY_LINK_3E_R_LOWER_SKPOS_RECEPTION_M,
			   PCIE_PHY_PCS_PHY_LINK_3E_R_LOWER_SKPOS_RECEPTION(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PCS_PHY_LINK_3E(pcie_phy_pcs),
			   PCIE_PHY_PCS_PHY_LINK_3E_R_SEP_REFCLK_SSC_M,
			   PCIE_PHY_PCS_PHY_LINK_3E_R_SEP_REFCLK_SSC(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PCS_PHY_LINK_32(pcie_phy_pcs),
			   PCIE_PHY_PCS_PHY_LINK_32_R_SSC_EN_M,
			   PCIE_PHY_PCS_PHY_LINK_32_R_SSC_EN(0x0));

	/* Setting up macro for PCIe clocking structure depending on board design */
	mmio_write_32(PCIE_PHY_PMA_PMA_CMU_FF(pcie_phy_pma), PCIE_LANE_ACCESS);
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_7F(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_7F_R_ASSERT_PPM_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_7F_R_ASSERT_PPM_7_0(0xFF));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_80(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_80_R_ASSERT_PPM_9_8_M,
			   PCIE_PHY_PMA_PMA_LANE_80_R_ASSERT_PPM_9_8(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_7D(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_7D_R_DEASSERT_PPM_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_7D_R_DEASSERT_PPM_7_0(0xE0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_7E(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_7E_R_DEASSERT_PPM_9_8_M,
			   PCIE_PHY_PMA_PMA_LANE_7E_R_DEASSERT_PPM_9_8(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_78(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_78_R_TIME_DEASSERT_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_78_R_TIME_DEASSERT_7_0(0xD4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_79(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_79_R_TIME_DEASSERT_15_8_M,
			   PCIE_PHY_PMA_PMA_LANE_79_R_TIME_DEASSERT_15_8(0x30));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_7A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_7A_R_TIME_ASSERT_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_7A_R_TIME_ASSERT_7_0(0xD4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_7B(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_7B_R_TIME_ASSERT_15_8_M,
			   PCIE_PHY_PMA_PMA_LANE_7B_R_TIME_ASSERT_15_8(0x30));
}

static void pcie_ep_calibrate_vco(bool enable)
{
	uintptr_t pcie_phy_pma = SPARX5_PCIE_PHY_PMA_BASE;

	mmio_write_32(PCIE_PHY_PMA_PMA_CMU_FF(pcie_phy_pma), PCIE_CMU_ACCESS);
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_42(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_42_R_EN_PRE_CAL_VCO_M,
			   PCIE_PHY_PMA_PMA_CMU_42_R_EN_PRE_CAL_VCO(enable));
}

static int pcie_ep_serdes_init(void)
{
	uintptr_t pcie_phy_pma = SPARX5_PCIE_PHY_PMA_BASE;

	/* Setting up PCS registers different than default according to GUC
	 * config application note v002
	 */
	/* New July 7th REXT10K internal setting was missing (is in config
	 * app note)
	 */
	mmio_write_32(PCIE_PHY_PMA_PMA_CMU_FF(pcie_phy_pma), PCIE_CMU_ACCESS);
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_00(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_00_CFG_PLL_TP_SEL_1_0_M,
			   PCIE_PHY_PMA_PMA_CMU_00_CFG_PLL_TP_SEL_1_0(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_1F(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_1F_CFG_VTUNE_SEL_M,
			   PCIE_PHY_PMA_PMA_CMU_1F_CFG_VTUNE_SEL(1));

	/* Commands that makes PC link using macro registers/HWT pins -
	 * used by default
	 */
	mmio_write_32(PCIE_PHY_PMA_PMA_CMU_FF(pcie_phy_pma), PCIE_LANE_ACCESS);

	/* The modification below makes phymode=3 work and was confirmed by
	 * GUC to be correct - included in configuration application note v003
	 */
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_93(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_93_R_RX_PCIE_GEN12_FROM_HWT_M,
			   PCIE_PHY_PMA_PMA_LANE_93_R_RX_PCIE_GEN12_FROM_HWT(0));

	/* This modification was supplied by GUC August 7, 2019 and is included
	 * in config app note v003
	 */
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_A1(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_A1_R_CDR_FROM_HWT_M,
			   PCIE_PHY_PMA_PMA_LANE_A1_R_CDR_FROM_HWT(0));

	/* These modification was supplied by GUC August 12, 2019 and are
	 * included in configuration application note v003
	 */
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_9F(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_9F_R_SWING_RATECHG_REG_M,
			   PCIE_PHY_PMA_PMA_LANE_9F_R_SWING_RATECHG_REG(0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_9F(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_9F_R_TXEQ_RATECHG_REG_M,
			   PCIE_PHY_PMA_PMA_LANE_9F_R_TXEQ_RATECHG_REG(0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_9F(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_9F_R_RXEQ_RATECHG_REG_M,
			   PCIE_PHY_PMA_PMA_LANE_9F_R_RXEQ_RATECHG_REG(0));

	/* Config application note Table 2.1-1 */
	pcie_ep_calibrate_vco(true);
	mmio_write_32(PCIE_PHY_PMA_PMA_CMU_FF(pcie_phy_pma), PCIE_CMU_ACCESS);
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_42(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_42_R_AUTO_PWRCHG_EN_M,
			   PCIE_PHY_PMA_PMA_CMU_42_R_AUTO_PWRCHG_EN(0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_1B(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_1B_CFG_RESERVE_7_0_M,
			   PCIE_PHY_PMA_PMA_CMU_1B_CFG_RESERVE_7_0(0x8));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_46(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_46_R_EN_RATECHG_CTRL_SEL_DIV_M,
			   PCIE_PHY_PMA_PMA_CMU_46_R_EN_RATECHG_CTRL_SEL_DIV(1));

	/* Config application note Table 2.1-2 */
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_4A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_4A_R_SW_8G_GEN12_M,
			   PCIE_PHY_PMA_PMA_CMU_4A_R_SW_8G_GEN12(1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_4A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_4A_R_SW_10G_GEN12_M,
			   PCIE_PHY_PMA_PMA_CMU_4A_R_SW_10G_GEN12(1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_4A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_4A_R_SW_8G_GEN34_M,
			   PCIE_PHY_PMA_PMA_CMU_4A_R_SW_8G_GEN34(0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_4A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_4A_R_SW_10G_GEN34_M,
			   PCIE_PHY_PMA_PMA_CMU_4A_R_SW_10G_GEN34(0));

	/* Config application note Table 2.1-3 */
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_48(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_48_R_GEN12_SEL_DIV_5_0_M,
			   PCIE_PHY_PMA_PMA_CMU_48_R_GEN12_SEL_DIV_5_0(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_49(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_49_R_GEN34_SEL_DIV_5_0_M,
			   PCIE_PHY_PMA_PMA_CMU_49_R_GEN34_SEL_DIV_5_0(0x12));

	/* Config application note Table 2.1-4 */
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_0B(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_0B_CFG_I_VCO_3_0_M,
			   PCIE_PHY_PMA_PMA_CMU_0B_CFG_I_VCO_3_0(0x8));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_0B(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_0B_CFG_ICP_BASE_SEL_3_0_M,
			   PCIE_PHY_PMA_PMA_CMU_0B_CFG_ICP_BASE_SEL_3_0(0xF));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_0C(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_0C_CFG_ICP_SEL_2_0_M,
			   PCIE_PHY_PMA_PMA_CMU_0C_CFG_ICP_SEL_2_0(0x5));

	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_0C(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_0C_CFG_RSEL_2_0_M,
			   PCIE_PHY_PMA_PMA_CMU_0C_CFG_RSEL_2_0(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_4D(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_4D_CFG_I_VCO_GEN34_3_0_M,
			   PCIE_PHY_PMA_PMA_CMU_4D_CFG_I_VCO_GEN34_3_0(0x8));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_4D(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_4D_CFG_ICP_SEL_GEN34_2_0_M,
			   PCIE_PHY_PMA_PMA_CMU_4D_CFG_ICP_SEL_GEN34_2_0(0x7));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_4E(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_4E_CFG_ICP_BASE_SEL_GEN34_3_0_M,
			   PCIE_PHY_PMA_PMA_CMU_4E_CFG_ICP_BASE_SEL_GEN34_3_0(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_4E(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_4E_CFG_RSEL_GEN34_2_0_M,
			   PCIE_PHY_PMA_PMA_CMU_4E_CFG_RSEL_GEN34_2_0(0x5));

	/* Enable LOL signal */
	mmio_write_32(PCIE_PHY_PMA_PMA_CMU_FF(pcie_phy_pma), PCIE_CMU_ACCESS);
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_CMU_30(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_CMU_30_R_PLL_DLOL_EN_M,
			   PCIE_PHY_PMA_PMA_CMU_30_R_PLL_DLOL_EN(1));

	/* New for Laguna from Table 2.1-5 in app note version 003 -
	 * settings different that default only
	 */
	mmio_write_32(PCIE_PHY_PMA_PMA_CMU_FF(pcie_phy_pma), PCIE_LANE_ACCESS);
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_93(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_93_R_RX_PCIE_GEN12_FROM_HWT_M,
			   PCIE_PHY_PMA_PMA_LANE_93_R_RX_PCIE_GEN12_FROM_HWT(0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_A1(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_A1_R_CDR_FROM_HWT_M,
			   PCIE_PHY_PMA_PMA_LANE_A1_R_CDR_FROM_HWT(0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_90(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_90_R_OSCAL_REG_M,
			   PCIE_PHY_PMA_PMA_LANE_90_R_OSCAL_REG(1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_90(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_90_R_AUTO_OSCAL_SQ_M,
			   PCIE_PHY_PMA_PMA_LANE_90_R_AUTO_OSCAL_SQ(1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_9F(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_9F_R_SUM_SETCM_EN_REG_M,
			   PCIE_PHY_PMA_PMA_LANE_9F_R_SUM_SETCM_EN_REG(1));

	/* Config application note Table 2.1-6 */
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_42(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_42_CFG_CDR_KF_GEN1_2_0_M,
			   PCIE_PHY_PMA_PMA_LANE_42_CFG_CDR_KF_GEN1_2_0(1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_42(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_42_CFG_CDR_KF_GEN2_2_0_M,
			   PCIE_PHY_PMA_PMA_LANE_42_CFG_CDR_KF_GEN2_2_0(1));

	/* r_cdr_m_gen1_7_0: PCIE Gen1 / SATA I / SAS 1.5G/other rate */
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_0F(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_0F_R_CDR_M_GEN1_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_0F_R_CDR_M_GEN1_7_0(0xA4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_10(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_10_R_CDR_M_GEN2_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_10_R_CDR_M_GEN2_7_0(0xA4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_11(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_11_R_CDR_M_GEN3_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_11_R_CDR_M_GEN3_7_0(0x64));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_12(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_12_R_CDR_M_GEN4_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_12_R_CDR_M_GEN4_7_0(0x64));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_24(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_24_CFG_PI_BW_GEN1_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_24_CFG_PI_BW_GEN1_3_0(0xD));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_24(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_24_CFG_PI_BW_GEN2_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_24_CFG_PI_BW_GEN2_3_0(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_25(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_25_CFG_PI_BW_GEN3_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_25_CFG_PI_BW_GEN3_3_0(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_25(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_25_CFG_PI_BW_GEN4_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_25_CFG_PI_BW_GEN4_3_0(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_26(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_26_CFG_ISCAN_EXT_DAC_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_26_CFG_ISCAN_EXT_DAC_7_0(0x82));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_BB(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_BB_CFG_ISCAN_EXT_DAC_B4TOB0_GEN2_4_0_M,
			   PCIE_PHY_PMA_PMA_LANE_BB_CFG_ISCAN_EXT_DAC_B4TOB0_GEN2_4_0(0x4));
	mmio_clrsetbits_32( PCIE_PHY_PMA_PMA_LANE_BC(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_BC_CFG_ISCAN_EXT_DAC_B4TOB0_GEN3_4_0_M,
			   PCIE_PHY_PMA_PMA_LANE_BC_CFG_ISCAN_EXT_DAC_B4TOB0_GEN3_4_0(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_BD(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_BD_CFG_ISCAN_EXT_DAC_B4TOB0_GEN4_4_0_M,
			   PCIE_PHY_PMA_PMA_LANE_BD_CFG_ISCAN_EXT_DAC_B4TOB0_GEN4_4_0(0x9));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_14(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_14_CFG_PI_EXT_DAC_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_14_CFG_PI_EXT_DAC_7_0(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B9(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B9_CFG_PI_EXT_DAC_B3TOB0_GEN2_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_B9_CFG_PI_EXT_DAC_B3TOB0_GEN2_3_0(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B9(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B9_CFG_PI_EXT_DAC_B3TOB0_GEN3_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_B9_CFG_PI_EXT_DAC_B3TOB0_GEN3_3_0(0x5));

	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_BA(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_BA_CFG_PI_EXT_DAC_B3TOB0_GEN4_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_BA_CFG_PI_EXT_DAC_B3TOB0_GEN4_3_0(0xC));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_15(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_15_CFG_PI_EXT_DAC_15_8_M,
			   PCIE_PHY_PMA_PMA_LANE_15_CFG_PI_EXT_DAC_15_8(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_16(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_16_CFG_PI_EXT_DAC_23_16_M,
			   PCIE_PHY_PMA_PMA_LANE_16_CFG_PI_EXT_DAC_23_16(0x20));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B2(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B2_CFG_PI_EXT_DAC_B17TOB14_GEN2_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_B2_CFG_PI_EXT_DAC_B17TOB14_GEN2_3_0(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B2(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B2_CFG_PI_EXT_DAC_B17TOB14_GEN3_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_B2_CFG_PI_EXT_DAC_B17TOB14_GEN3_3_0(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B3(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B3_CFG_PI_EXT_DAC_B17TOB14_GEN4_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_B3_CFG_PI_EXT_DAC_B17TOB14_GEN4_3_0(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_16(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_16_CFG_PI_EXT_DAC_23_16_M,
			   PCIE_PHY_PMA_PMA_LANE_16_CFG_PI_EXT_DAC_23_16(0x20));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B3(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B3_CFG_PI_EXT_DAC_B21_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_B3_CFG_PI_EXT_DAC_B21_GEN2(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B3(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B3_CFG_PI_EXT_DAC_B21_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_B3_CFG_PI_EXT_DAC_B21_GEN3(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B3(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B3_CFG_PI_EXT_DAC_B21_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_B3_CFG_PI_EXT_DAC_B21_GEN4(0x0));

	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B6(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B6_CFG_PI_OFFSET_GEN4_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_B6_CFG_PI_OFFSET_GEN4_3_0(0x6));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_1A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_1A_CFG_PI_FLOOP_STEPS_GEN1_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_1A_CFG_PI_FLOOP_STEPS_GEN1_1_0(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_A7(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_A7_CFG_PI_FLOOP_STEPS_GEN2_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_A7_CFG_PI_FLOOP_STEPS_GEN2_1_0(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_A7(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_A7_CFG_PI_FLOOP_STEPS_GEN3_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_A7_CFG_PI_FLOOP_STEPS_GEN3_1_0(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_A8(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_A8_CFG_PI_FLOOP_STEPS_GEN4_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_A8_CFG_PI_FLOOP_STEPS_GEN4_1_0(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B8(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B8_CFG_TACC_SEL_GEN4_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_B8_CFG_TACC_SEL_GEN4_1_0(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B4(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B4_CFG_RX_SSC_LH_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_B4_CFG_RX_SSC_LH_GEN4(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_38(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_38_CFG_RXFILT_Z_GEN1_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_38_CFG_RXFILT_Z_GEN1_1_0(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B0(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B0_CFG_RXFILT_Z_GEN2_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_B0_CFG_RXFILT_Z_GEN2_1_0(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_B1(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_B1_CFG_DIS_2NDORDER_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_B1_CFG_DIS_2NDORDER_GEN4(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_27(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_27_CFG_ISCAN_EXT_DAC_15_8_M,
			   PCIE_PHY_PMA_PMA_LANE_27_CFG_ISCAN_EXT_DAC_15_8(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_A9(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_A9_CFG_ISCAN_EXT_DAC_B12TOB8_GEN2_4_0_M,
			   PCIE_PHY_PMA_PMA_LANE_A9_CFG_ISCAN_EXT_DAC_B12TOB8_GEN2_4_0(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_AA(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_AA_CFG_ISCAN_EXT_DAC_B12TOB8_GEN3_4_0_M,
			   PCIE_PHY_PMA_PMA_LANE_AA_CFG_ISCAN_EXT_DAC_B12TOB8_GEN3_4_0(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_AB(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_AB_CFG_ISCAN_EXT_DAC_B12TOB8_GEN4_4_0_M,
			   PCIE_PHY_PMA_PMA_LANE_AB_CFG_ISCAN_EXT_DAC_B12TOB8_GEN4_4_0(0x1f));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_29(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_29_CFG_ISCAN_EXT_DAC_30_24_M,
			   PCIE_PHY_PMA_PMA_LANE_29_CFG_ISCAN_EXT_DAC_30_24(0x68));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_BE(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_BE_CFG_ISCAN_EXT_DAC_B30_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_BE_CFG_ISCAN_EXT_DAC_B30_GEN2(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_BE(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_BE_CFG_ISCAN_EXT_DAC_B30_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_BE_CFG_ISCAN_EXT_DAC_B30_GEN3(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_BE(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_BE_CFG_ISCAN_EXT_DAC_B30_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_BE_CFG_ISCAN_EXT_DAC_B30_GEN4(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_31(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_31_CFG_RSTN_DFEDIG_GEN1_M,
			   PCIE_PHY_PMA_PMA_LANE_31_CFG_RSTN_DFEDIG_GEN1(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_BE(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_BE_CFG_RSTN_DFEDIG_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_BE_CFG_RSTN_DFEDIG_GEN2(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_BE(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_BE_CFG_RSTN_DFEDIG_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_BE_CFG_RSTN_DFEDIG_GEN3(0x1));

	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_3B(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_3B_CFG_MF_MAX_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_3B_CFG_MF_MAX_3_0(0x5));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_28(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_28_CFG_ISCAN_EXT_DAC_23_16_M,
			   PCIE_PHY_PMA_PMA_LANE_28_CFG_ISCAN_EXT_DAC_23_16(0x20));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_17(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_17_CFG_PI_EXT_DAC_30_24_M,
			   PCIE_PHY_PMA_PMA_LANE_17_CFG_PI_EXT_DAC_30_24(0x15));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_36(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_36_CFG_PREDRV_SLEWRATE_GEN1_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_36_CFG_PREDRV_SLEWRATE_GEN1_1_0(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_F0(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_F0_CFG_PREDRV_SLEWRATE_GEN2_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_F0_CFG_PREDRV_SLEWRATE_GEN2_1_0(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_37(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_37_CFG_IP_PRE_BASE_GEN1_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_37_CFG_IP_PRE_BASE_GEN1_1_0(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_F5(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_F5_CFG_IP_PRE_BASE_GEN2_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_F5_CFG_IP_PRE_BASE_GEN2_1_0(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_F5(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_F5_CFG_IP_PRE_BASE_GEN3_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_F5_CFG_IP_PRE_BASE_GEN3_1_0(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_3A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_3A_CFG_MP_MAX_GEN1_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_3A_CFG_MP_MAX_GEN1_3_0(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_F2(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_F2_CFG_MP_MAX_GEN2_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_F2_CFG_MP_MAX_GEN2_3_0(0x6));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_F2(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_F2_CFG_MP_MAX_GEN3_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_F2_CFG_MP_MAX_GEN3_3_0(0xE));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_F3(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_F3_CFG_MP_MAX_GEN4_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_F3_CFG_MP_MAX_GEN4_3_0(0xC));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_0A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_0A_CFG_SUM_SETCM_EN_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_0A_CFG_SUM_SETCM_EN_GEN3(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_0A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_0A_CFG_SUM_SETCM_EN_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_0A_CFG_SUM_SETCM_EN_GEN4(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_34(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_34_CFG_EN_DFEDIG_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_34_CFG_EN_DFEDIG_GEN2(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_34(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_34_CFG_EN_DFEDIG_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_34_CFG_EN_DFEDIG_GEN3(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_34(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_34_CFG_EN_DFEDIG_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_34_CFG_EN_DFEDIG_GEN4(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_26(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_26_CFG_ISCAN_EXT_DAC_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_26_CFG_ISCAN_EXT_DAC_7_0(0x82));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_3C(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_3C_CFG_ISCAN_EXT_DAC_B7_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_3C_CFG_ISCAN_EXT_DAC_B7_GEN2(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_3C(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_3C_CFG_ISCAN_EXT_DAC_B7_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_3C_CFG_ISCAN_EXT_DAC_B7_GEN3(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_3C(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_3C_CFG_ISCAN_EXT_DAC_B7_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_3C_CFG_ISCAN_EXT_DAC_B7_GEN4(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_08(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_08_CFG_CTLE_NEGC_M,
			   PCIE_PHY_PMA_PMA_LANE_08_CFG_CTLE_NEGC(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_44(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_44_CFG_CTLE_NEGC_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_44_CFG_CTLE_NEGC_GEN2(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_44(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_44_CFG_CTLE_NEGC_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_44_CFG_CTLE_NEGC_GEN3(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_44(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_44_CFG_CTLE_NEGC_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_44_CFG_CTLE_NEGC_GEN4(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_09(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_09_CFG_IP_RX_LS_SEL_2_0_M,
			   PCIE_PHY_PMA_PMA_LANE_09_CFG_IP_RX_LS_SEL_2_0(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_45(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_45_CFG_IP_RX_LS_SEL_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_45_CFG_IP_RX_LS_SEL_GEN2(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_45(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_45_CFG_IP_RX_LS_SEL_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_45_CFG_IP_RX_LS_SEL_GEN3(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_46(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_46_CFG_IP_RX_LS_SEL_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_46_CFG_IP_RX_LS_SEL_GEN4(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_16(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_16_CFG_PI_EXT_DAC_23_16_M,
			   PCIE_PHY_PMA_PMA_LANE_16_CFG_PI_EXT_DAC_23_16(0x20));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_47(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_47_CFG_PI_EXT_DAC_B23TOB22_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_47_CFG_PI_EXT_DAC_B23TOB22_GEN2(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_47(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_47_CFG_PI_EXT_DAC_B23TOB22_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_47_CFG_PI_EXT_DAC_B23TOB22_GEN3(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_47(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_47_CFG_PI_EXT_DAC_B23TOB22_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_47_CFG_PI_EXT_DAC_B23TOB22_GEN4(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_07(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_07_CFG_RX_REG_BYP_M,
			   PCIE_PHY_PMA_PMA_LANE_07_CFG_RX_REG_BYP(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_4A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_4A_CFG_RX_REG_BYP_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_4A_CFG_RX_REG_BYP_GEN2(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_4A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_4A_CFG_RX_REG_BYP_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_4A_CFG_RX_REG_BYP_GEN3(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_4A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_4A_CFG_RX_REG_BYP_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_4A_CFG_RX_REG_BYP_GEN4(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_0E(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_0E_CFG_EQC_FORCE_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_0E_CFG_EQC_FORCE_3_0(0x8));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_51(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_51_CFG_EQC_FORCE_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_51_CFG_EQC_FORCE_GEN2(0x8));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_51(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_51_CFG_EQC_FORCE_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_51_CFG_EQC_FORCE_GEN3(0x8));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_55(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_55_CFG_EQC_FORCE_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_55_CFG_EQC_FORCE_GEN4(0x8));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_36(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_36_CFG_EN_PREDRV_EMPH_M,
			   PCIE_PHY_PMA_PMA_LANE_36_CFG_EN_PREDRV_EMPH(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_55(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_55_CFG_EN_PREDRV_EMPH_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_55_CFG_EN_PREDRV_EMPH_GEN2(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_55(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_55_CFG_EN_PREDRV_EMPH_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_55_CFG_EN_PREDRV_EMPH_GEN3(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_55(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_55_CFG_EN_PREDRV_EMPH_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_55_CFG_EN_PREDRV_EMPH_GEN4(0x1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_2F(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_2F_CFG_VGA_CTRL_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_2F_CFG_VGA_CTRL_3_0(0x5));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_56(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_56_CFG_VGA_CTRL_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_56_CFG_VGA_CTRL_GEN2(0x5));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_56(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_56_CFG_VGA_CTRL_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_56_CFG_VGA_CTRL_GEN3(0x5));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_57(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_57_CFG_VGA_CTRL_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_57_CFG_VGA_CTRL_GEN4(0x5));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_3B(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_3B_CFG_MF_MIN_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_3B_CFG_MF_MIN_3_0(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_6E(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_6E_CFG_MF_MIN_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_6E_CFG_MF_MIN_GEN2(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_6E(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_6E_CFG_MF_MIN_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_6E_CFG_MF_MIN_GEN3(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_6F(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_6F_CFG_MF_MIN_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_6F_CFG_MF_MIN_GEN4(0x2));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_4A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_4A_CFG_RX_SP_CTLE_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_4A_CFG_RX_SP_CTLE_1_0(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_70(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_70_CFG_RX_SP_CTLE_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_70_CFG_RX_SP_CTLE_GEN2(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_70(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_70_CFG_RX_SP_CTLE_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_70_CFG_RX_SP_CTLE_GEN3(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_70(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_70_CFG_RX_SP_CTLE_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_70_CFG_RX_SP_CTLE_GEN4(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_16(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_16_CFG_PI_EXT_DAC_23_16_M,
			   PCIE_PHY_PMA_PMA_LANE_16_CFG_PI_EXT_DAC_23_16(0x20));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_71(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_71_CFG_PI_EXT_DAC_B20TOB18_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_71_CFG_PI_EXT_DAC_B20TOB18_GEN2(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_71(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_71_CFG_PI_EXT_DAC_B20TOB18_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_71_CFG_PI_EXT_DAC_B20TOB18_GEN3(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_72(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_72_CFG_PI_EXT_DAC_B20TOB18_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_72_CFG_PI_EXT_DAC_B20TOB18_GEN4(0x0));

	/* Amplitude settings of 4 from configuration note caused link
	 * failure on partner. Setting to 3 later for maximum amplitude
	 */
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_33(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_33_CFG_ITX_IPDRIVER_BASE_2_0_M,
			   PCIE_PHY_PMA_PMA_LANE_33_CFG_ITX_IPDRIVER_BASE_2_0(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_73(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_73_CFG_ITX_IPDRIVER_BASE_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_73_CFG_ITX_IPDRIVER_BASE_GEN2(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_73(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_73_CFG_ITX_IPDRIVER_BASE_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_73_CFG_ITX_IPDRIVER_BASE_GEN3(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_AE(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_AE_CFG_ITX_IPDRIVER_BASE_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_AE_CFG_ITX_IPDRIVER_BASE_GEN4(0x4));

	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_2F(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_2F_CFG_VGA_CP_2_0_M,
			   PCIE_PHY_PMA_PMA_LANE_2F_CFG_VGA_CP_2_0(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_AE(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_AE_CFG_VGA_CAP_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_AE_CFG_VGA_CAP_GEN2(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_AF(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_AF_CFG_VGA_CAP_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_AF_CFG_VGA_CAP_GEN3(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_AF(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_AF_CFG_VGA_CAP_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_AF_CFG_VGA_CAP_GEN4(0x4));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_03(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_03_CFG_TAP_ADV_4_0_M,
			   PCIE_PHY_PMA_PMA_LANE_03_CFG_TAP_ADV_4_0(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_F7(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_F7_CFG_TAP_ADV_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_F7_CFG_TAP_ADV_GEN2(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_F8(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_F8_CFG_TAP_ADV_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_F8_CFG_TAP_ADV_GEN3(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_F9(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_F9_CFG_TAP_ADV_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_F9_CFG_TAP_ADV_GEN4(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_04(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_04_CFG_TAP_DLY_4_0_M,
			   PCIE_PHY_PMA_PMA_LANE_04_CFG_TAP_DLY_4_0(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_FA(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_FA_CFG_TAP_DLY_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_FA_CFG_TAP_DLY_GEN2(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_FB(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_FB_CFG_TAP_DLY_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_FB_CFG_TAP_DLY_GEN3(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_FC(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_FC_CFG_TAP_DLY_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_FC_CFG_TAP_DLY_GEN4(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_0B(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_0B_CFG_EQ_RES_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_0B_CFG_EQ_RES_3_0(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_FD(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_FD_CFG_EQ_RES_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_FD_CFG_EQ_RES_GEN2(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_FD(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_FD_CFG_EQ_RES_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_FD_CFG_EQ_RES_GEN3(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_FE(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_FE_CFG_EQ_RES_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_FE_CFG_EQ_RES_GEN4(0x3));

	/* Config application note Table 2.1-7 */
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_40(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_40_CFG_LANE_RESERVE_7_0_M,
			   PCIE_PHY_PMA_PMA_LANE_40_CFG_LANE_RESERVE_7_0(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_41(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_41_CFG_LANE_RESERVE_15_8_M,
			   PCIE_PHY_PMA_PMA_LANE_41_CFG_LANE_RESERVE_15_8(0xE1));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_2A(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_2A_CFG_ISCAN_EXT_QRT_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_2A_CFG_ISCAN_EXT_QRT_1_0(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_18(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_18_CFG_PI_EXT_QRT_1_0_M,
			   PCIE_PHY_PMA_PMA_LANE_18_CFG_PI_EXT_QRT_1_0(0x0));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_05(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_05_CFG_TAP_DLY2_3_0_M,
			   PCIE_PHY_PMA_PMA_LANE_05_CFG_TAP_DLY2_3_0(0xA));

	/* Setting maximum amplitude
	 * Amplitude settings of 4 from configuration note caused link
	 * failure on partner. Setting to 3 for maximum amplitude
	 */
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_33(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_33_CFG_ITX_IPDRIVER_BASE_2_0_M,
			   PCIE_PHY_PMA_PMA_LANE_33_CFG_ITX_IPDRIVER_BASE_2_0(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_73(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_73_CFG_ITX_IPDRIVER_BASE_GEN2_M,
			   PCIE_PHY_PMA_PMA_LANE_73_CFG_ITX_IPDRIVER_BASE_GEN2(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_73(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_73_CFG_ITX_IPDRIVER_BASE_GEN3_M,
			   PCIE_PHY_PMA_PMA_LANE_73_CFG_ITX_IPDRIVER_BASE_GEN3(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_AE(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_AE_CFG_ITX_IPDRIVER_BASE_GEN4_M,
			   PCIE_PHY_PMA_PMA_LANE_AE_CFG_ITX_IPDRIVER_BASE_GEN4(0x3));
	mmio_clrsetbits_32(PCIE_PHY_PMA_PMA_LANE_52(pcie_phy_pma),
			   PCIE_PHY_PMA_PMA_LANE_52_CFG_IBIAS_TUNE_RESERVE_5_0_M,
			   PCIE_PHY_PMA_PMA_LANE_52_CFG_IBIAS_TUNE_RESERVE_5_0(0x3F));
	return 0;
}


static void pcie_ep_phy_pcs_tx_margins(void)
{
	uintptr_t pcie_phy_pcs = SPARX5_PCIE_PHY_PCS_BASE;

	/* Add tx margin writes */
	mmio_clrsetbits_32(PCIE_PHY_PCS_PHY_LINK_2C(pcie_phy_pcs),
			   PCIE_PHY_PCS_PHY_LINK_2C_R_TXMARGIN_0_B7_B0_M |
			   PCIE_PHY_PCS_PHY_LINK_2C_R_TXMARGIN_1_B7_B0_M,
			   PCIE_PHY_PCS_PHY_LINK_2C_R_TXMARGIN_0_B7_B0(0xd0) |
			   PCIE_PHY_PCS_PHY_LINK_2C_R_TXMARGIN_1_B7_B0(0xff));
	mmio_clrsetbits_32(PCIE_PHY_PCS_PHY_LINK_2D(pcie_phy_pcs),
			   PCIE_PHY_PCS_PHY_LINK_2D_R_TXMARGIN_2_B7_B0_M |
			   PCIE_PHY_PCS_PHY_LINK_2D_R_TXMARGIN_3_B7_B0_M,
			   PCIE_PHY_PCS_PHY_LINK_2D_R_TXMARGIN_2_B7_B0(0xff) |
			   PCIE_PHY_PCS_PHY_LINK_2D_R_TXMARGIN_3_B7_B0(0xf0));
	mmio_clrsetbits_32(PCIE_PHY_PCS_PHY_LINK_2E(pcie_phy_pcs),
			   PCIE_PHY_PCS_PHY_LINK_2E_R_TXMARGIN_4_B7_B0_M |
			   PCIE_PHY_PCS_PHY_LINK_2E_R_TXMARGIN_5_B7_B0_M,
			   PCIE_PHY_PCS_PHY_LINK_2E_R_TXMARGIN_4_B7_B0(0xb0) |
			   PCIE_PHY_PCS_PHY_LINK_2E_R_TXMARGIN_5_B7_B0(0xa0));
	mmio_clrsetbits_32(PCIE_PHY_PCS_PHY_LINK_2F(pcie_phy_pcs),
			   PCIE_PHY_PCS_PHY_LINK_2F_R_TXMARGIN_6_B7_B0_M |
			   PCIE_PHY_PCS_PHY_LINK_2F_R_TXMARGIN_7_B7_B0_M,
			   PCIE_PHY_PCS_PHY_LINK_2F_R_TXMARGIN_6_B7_B0(0x90) |
			   PCIE_PHY_PCS_PHY_LINK_2F_R_TXMARGIN_7_B7_B0(0x10));
}


static bool pcie_ep_wait_for_cmu_lock(const struct pcie_ep_config *cfg)
{
	uintptr_t pcie_phy_pma = SPARX5_PCIE_PHY_PMA_BASE;
	uint32_t lol = 1;

	INFO("pcie: Wait for CMU lock\n");
	mmio_write_32(PCIE_PHY_PMA_PMA_CMU_FF(pcie_phy_pma), PCIE_CMU_ACCESS);
	while (lol != 0) {
		lol = PCIE_PHY_PMA_PMA_CMU_E0_PLL_LOL_UDL_X(
			mmio_read_32(PCIE_PHY_PMA_PMA_CMU_E0(pcie_phy_pma)));
	}
	INFO("pcie: CMU in lock\n");
	return true;
}


static int pcie_ep_set_mode(const struct pcie_ep_config *cfg)
{
	/* Set PCIe controller to endpoint mode */
	// wr_fld(CPU, PCIERST_CFG, POWERONRST_VAL, 0);
	// wr_fld(CPU, PCIERST_CFG, POWERONRST_FORCE, 1);
	// wr_fld(CPU, PCIE_SYS_CFG, PCIE_RC_EP_MODE, 0);
	// expect_fld(CPU, PCIE_SYS_CFG, PCIE_RC_EP_MODE, 0);
	// expect_fld(CPU, PCIERST_CFG, POWERONRST_FORCE, 1);
	// wr_fld(CPU, PCIERST_CFG, POWERONRST_FORCE, 0);
	// expect_fld(CPU, PCIERST_CFG, POWERONRST_FORCE, 0);
	return 0;
}


void pcie_ep_init(const struct pcie_ep_config *cfg)
{
	/* Measurements shows that PERST goes high before there is a clock
	 * signal, and the EP needs to be completely configured after maximum
	 * 20ms after PERST goes high, so this is the procedure:
	 *
	 * 1) Set PCIe PHY macro reset (PIPE reset)
	 * 2) Configure CMU+LANE, and set
	 *    pcie_phy_pma pma_cmu_42 r_en_pre_cal_vco 1
	 * 3) Wait on PERST=1 (quick polling!)
	 * 4) Release PHY macro reset
	 * 5) Check for CMU lock and PERST 0 (quick polling!)
	 * 5a) If PERST=0: goto step 1
	 * 5b) If CMU not locked  goto 5
	 * 6) Wait 1us to ensure PHY reset is completed
	 * 7) Configure PCIe controller
	 * 8) Wait for PERST=0: reset phy and wait for PERST=1
	 */
	NOTICE("pcie: Config EP\n");
	pcie_ep_serdes_reset();
reset_phy:
	pcie_ep_reset_pipe(true);
// 	pcie_ep_config_perst(cfg);
	pcie_ep_ssc_clock();
	pcie_ep_serdes_init();
	pcie_ep_phy_pcs_tx_margins();
	while (true) {
		// pcie_ep_wait_for_perst_high(cfg);
		pcie_ep_reset_pipe(false);
		mdelay(1);
		if (!pcie_ep_wait_for_cmu_lock(cfg)) {
			goto reset_phy;
		}
		INFO("pcie: Wait 1us to ensure PHY reset is completed\n");
		udelay(1);
		pcie_ep_set_mode(cfg);
		// pcie_ep_ctrl_init(cfg);
		// pcie_ep_state();
		// /* EP is operational so watch for for PERST going low */
		// pcie_ep_wait_for_perst_low(cfg);
		// pcie_ep_reset_pipe(true);
		/* Go to sleep */
		asm volatile (
			"sleep_loop:"
			"    wfi ;"
			"    b sleep_loop;"
			);
	}
}

__weak int board_early_init_f(void)
{
	struct pcie_ep_config cfg = {
		.max_link_speed = 3,
		.vendor_id = 0x101b,
		.device_id = 0xb006,
	};

	printascii("Sparx5: Ready to setup EP\n");
	pcie_ep_init(&cfg);
	return 0;
}
