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

#define NOTICE printascii

struct pcie_ep_config {
	uint32_t max_link_speed;
	uint32_t vendor_id;
	uint32_t device_id;
};


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
// 	pcie_ep_serdes_reset();
// reset_phy:
// 	pcie_ep_reset_pipe(true);
// 	pcie_ep_config_perst(cfg);
// 	pcie_ep_ssc_clock();
// 	pcie_ep_serdes_init();
// 	pcie_ep_phy_pcs_tx_margins();
// 	while (true) {
// 		pcie_ep_wait_for_perst_high(cfg);
// 		pcie_ep_reset_pipe(false);
// 		mdelay(1);
// 		if (!pcie_ep_wait_for_cmu_lock(cfg)) {
// 			goto reset_phy;
// 		}
// 		INFO("pcie: Wait 1us to ensure PHY reset is completed");
// 		udelay(1);
// 		pcie_ep_ctrl_init(cfg);
// 		pcie_ep_state();
// 		/* EP is operational so watch for for PERST going low */
// 		pcie_ep_wait_for_perst_low(cfg);
// 		pcie_ep_reset_pipe(true);
// 	}
}

void sparx5_pcie_ep_init(void)
{
	struct pcie_ep_config cfg = {
		.max_link_speed = 3,
		.vendor_id = 0x101b,
		.device_id = 0xb006,
	};

	pcie_ep_init(&cfg);
}
