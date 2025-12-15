// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Microsemi Corporation
 */

//#define DEBUG

#include <common.h>
#include <config.h>
#include <dm.h>
#include <dm/devres.h>
#include <dm/of_access.h>
#include <dm/of_addr.h>
#include <fdt_support.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <miiphy.h>
#include <net.h>
#include <wait_bit.h>
#include <command.h>

#include "sparx5_switch.h"

#include "sparx5_regs.h"
#include "sparx5_reg_offset.h"

#include <dt-bindings/mscc/sparx5_data.h>

static struct sparx5_private *dev_priv;

#define MAC_VID			1 /* Also = FID 1 */
#define ETH_ALEN		6

#define PGID_L2_UC(priv)		(priv->data->num_ports + 0)
#define PGID_L2_MC(priv)		(priv->data->num_ports + 1)
#define PGID_BROADCAST(priv)		(priv->data->num_ports + 2)
#define PGID_HOST(priv)			(priv->data->num_ports + 3)

#define CONFIG_IFH_FMT_NONE	0

#define DSM_CAL_MAX_DEVS_PER_TAXI	10
#define DSM_CAL_TAXIS			5
#define DSM_CAL_LEN			64

#define SPX5_RGMII_TX_CLK_125MHZ	1   /* 1000Mbps */
#define SPX5_RGMII_TX_CLK_25MHZ		2   /* 100Mbps */
#define SPX5_RGMII_TX_CLK_2M5MHZ	3   /* 10Mbps */

static const char * const sparx5_reg_names[] = {
	"ana_ac", "ana_cl", "ana_l2", "ana_l3",
	"asm", "lrn", "qfwd", "qs",
	"qsys", "rew", "vop", "dsm", "eacl",
	"vcap_super", "hsch", "port_conf", "xqs", "hsio", "gcb", "cpu", "ptp",
	"port0", "port1", "port2", "port3", "port4", "port5", "port6",
	"port7", "port8", "port9", "port10", "port11", "port12", "port13",
	"port14", "port15", "port16", "port17", "port18", "port19", "port20",
	"port21", "port22", "port23", "port24", "port25", "port26", "port27",
	"port28", "port29", "port30", "port31", "port32", "port33", "port34",
	"port35", "port36", "port37", "port38", "port39", "port40", "port41",
	"port42", "port43", "port44", "port45", "port46", "port47", "port48",
	"port49", "port50", "port51", "port52", "port53", "port54", "port55",
	"port56", "port57", "port58", "port59", "port60", "port61", "port62",
	"port63", "port64",
};

static const char * const lan969x_reg_names[] = {
	"ana_ac", "ana_cl", "ana_l2", "ana_l3",
	"asm", "lrn", "qfwd", "qs",
	"qsys", "rew", "vop", "dsm", "eacl",
	"vcap_super", "hsch", "port_conf", "xqs", "hsio", "gcb", "cpu", "ptp",
	"port0", "port1", "port2", "port3", "port4", "port5", "port6",
	"port7", "port8", "port9", "port10", "port11", "port12", "port13",
	"port14", "port15", "port16", "port17", "port18", "port19", "port20",
	"port21", "port22", "port23", "port24", "port25", "port26", "port27",
	"port28", "port29",
};

enum sparx5_ctrl_regs {
	TARGET_ANA_AC,
	TARGET_ANA_CL,
	TARGET_ANA_L2,
	TARGET_ANA_L3,
	TARGET_ASM,
	TARGET_LRN,
	TARGET_QFWD,
	TARGET_QS,
	TARGET_QSYS,
	TARGET_REW,
	TARGET_VOP,
	TARGET_DSM,
	TARGET_EACL,
	TARGET_VCAP_SUPER,
	TARGET_HSCH,
	TARGET_PORT_CONF,
	TARGET_XQS,
	TARGET_HSIO,
	TARGET_GCB,
	TARGET_CPU,
	TARGET_PTP,
	__REG_MAX,
	/* This is used for all the ports even if it a 2.5G or RGMII */
	TARGET_DEV2G5 = __REG_MAX,
};

static const unsigned long sparx5_regs_qs[] = {
	[MSCC_QS_XTR_RD] = 0x8,
	[MSCC_QS_XTR_FLUSH] = 0x18,
	[MSCC_QS_XTR_DATA_PRESENT] = 0x1c,
	[MSCC_QS_INJ_WR] = 0x2c,
	[MSCC_QS_INJ_CTRL] = 0x34,
};

enum {
	SERDES_ARG_MAC_TYPE,
	SERDES_ARG_SERDES,
	SERDES_ARG_SER_IDX,
	SERDES_ARG_MAX,
};

static struct mscc_match_data mscc_sparx5_data = {
	.reg_names = sparx5_reg_names,
	.regs = {
		.reggrp_addr = sparx5_reggrp_addr,
		.reggrp_cnt = sparx5_reggrp_cnt,
		.reggrp_size = sparx5_reggrp_sz,
		.reg_addr = sparx5_reg_addr,
		.reg_cnt = sparx5_reg_cnt,
		.regfield_addr = sparx5_regfield_addr,
	},
	.num_regs = 86,
	.num_ports = 65,
	.num_bus = 4,
	.cpu_port = 65,
	.npi_port = 64,
	.ifh_len = 9,
	.num_cal_auto = 7,
	.target = SPARX5_TARGET,
};

static struct mscc_match_data mscc_lan969x_data = {
	.reg_names = lan969x_reg_names,
	.regs = {
		.reggrp_addr = lan969x_reggrp_addr,
		.reggrp_cnt = lan969x_reggrp_cnt,
		.reggrp_size = lan969x_reggrp_sz,
		.reg_addr = lan969x_reg_addr,
		.reg_cnt = lan969x_reg_cnt,
		.regfield_addr = lan969x_regfield_addr,
	},
	.num_regs = 51,
	.num_ports = 30,
	.num_bus = 2,
	.cpu_port = 30,
	.npi_port = 24,
	.ifh_len = 9,
	.num_cal_auto = 4,
	.target = LAN969X_TARGET,
};

/* Keep the id, tinst and tcnt just to be able to use the same macros
 * as in the linux kernel
 */
static u32 spx5_rd(struct sparx5_private *priv,
		   int id, int tinst, int tcnt,
		   u32 gbase, u32 ginst, u32 gcnt, u32 gwidth,
		   u32 raddr, u32 rinst, u32 rcnt, u32 rwidth)
{
	return readl(priv->regs[id + tinst] +
		     gbase + ((ginst) * gwidth) +
		     raddr + ((rinst) * rwidth));
}

static void spx5_wr(u32 val, struct sparx5_private *priv,
		    int id, int tinst, int tcnt,
		    u32 gbase, u32 ginst, u32 gcnt, u32 gwidth,
		    u32 raddr, u32 rinst, u32 rcnt, u32 rwidth)
{
	writel(val,
	       priv->regs[id + tinst] + gbase + ((ginst) * gwidth) + raddr + ((rinst) * rwidth));
}

static void spx5_rmw(u32 val, u32 mask, struct sparx5_private *priv,
		     int id, int tinst, int tcnt,
		     u32 gbase, u32 ginst, u32 gcnt, u32 gwidth,
		     u32 raddr, u32 rinst, u32 rcnt, u32 rwidth)
{
	u32 nval;

	nval = readl(priv->regs[id + tinst] + gbase + ((ginst) * gwidth) + raddr + ((rinst) * rwidth));
	nval = (nval & ~mask) | (val & mask);
	writel(nval, priv->regs[id + tinst] + gbase + ((ginst) * gwidth) + raddr + ((rinst) * rwidth));
}

static void __iomem *spx5_offset(struct sparx5_private *priv,
				 u32 id, u32 tinst, u32 tcnt,
				 u32 gbase, u32 ginst, u32 gcnt, u32 gwidth,
				 u32 raddr, u32 rinst, u32 rcnt, u32 rwidth)
{
	return priv->regs[id + tinst] + gbase + ((ginst) * gwidth) + raddr + ((rinst) * rwidth);
}

static int ram_init(void __iomem *addr)
{
	writel(BIT(1), addr);

	if (wait_for_bit_le32(addr, BIT(1), false, 2000, false)) {
		printf("Timeout in memory reset, addr: %p = 0x%08x\n", addr, readl(addr));
		return 1;
	}

	return 0;
}

static int sparx5_switch_init(struct sparx5_private *priv)
{
	/* Initialize memories */
	ram_init(spx5_offset(priv, QSYS_RAM_INIT));
	ram_init(spx5_offset(priv, ASM_RAM_INIT));
	ram_init(spx5_offset(priv, ANA_AC_RAM_INIT));
	ram_init(spx5_offset(priv, REW_RAM_INIT));
	ram_init(spx5_offset(priv, DSM_RAM_INIT));
	ram_init(spx5_offset(priv, EACL_RAM_INIT));
	ram_init(spx5_offset(priv, VCAP_SUPER_RAM_INIT));
	ram_init(spx5_offset(priv, VOP_RAM_INIT));

	/* Reset counters */
	spx5_wr(0x1, priv, ANA_AC_STAT_RESET);
	spx5_wr(0x1, priv, ASM_STAT_CFG);

	return 0;
}

static void sparx5_taxi2ports(u32 taxi, u32 *port_ptr) {
	u32 taxi_ports[DSM_CAL_TAXIS][DSM_CAL_MAX_DEVS_PER_TAXI] = {
		{0,4,1,2,3,5,6,7,28,29},
		{8,12,9,13,10,11,14,15,99,99},
		{16,20,17,21,18,19,22,23,99,99},
		{24,25,99,99,99,99,99,99,99,99},
		{26,27,99,99,99,99,99,99,99,99}};

	memcpy(port_ptr, &taxi_ports[taxi],
	       sizeof(u32) * DSM_CAL_MAX_DEVS_PER_TAXI);
}

static int sparx5_dsm_calc_calendar(u32 *speeds, u32 ports, u32 freq_mhz,
				    u32 *cal, u32 *cal_len) {
	int bw = freq_mhz * 128 / 1.05;
	int grps = 3;
	int grp[ports];
	int cnt[30];
	int grpspd = 10000;
	int bwavail[3], s_values[] = {5000, 2500, 1000};
	int i, j, p, sp, win, grplen, lcs, s, found;

	if (bw < 30000) {
		for (i = 0; i < ports && speeds[i] != 10000; i++);
		if (i == ports)
		    grpspd = 5000;
		else
		    grps = 2;
	}

	lcs = grpspd;
	for (i = 0; i < 3; i++) {
		s = s_values[i];
		found = 0;
		for (j = 0; j < ports; j++) {
			if (speeds[j] == s) {
				found = 1;
				break;
			}
		}

		if (found) {
			if (lcs == 2500) {
				lcs = 500;
			} else {
				lcs = s;
			}
		}
	}
	grplen = grpspd / lcs;

	for (i = 0; i < grps; bwavail[i++] = grpspd);

	for (i = 0; i < ports; i++) {
		if (!speeds[i]) {
			continue;
		}
		for (j = 0; j < grps && bwavail[j] < speeds[i]; j++);
		if (j == grps) {
			printf("Could not generate calendar at taxibw %d\n", bw);
			return -1;
		}
		grp[i] = j;
		bwavail[j] -= speeds[i];
	}

	memset(cnt, 0, sizeof(cnt));
	for (i = 0; i < grplen; i++) {
		for (j = 0; j < grps; j++) {
			sp = 1;
			win = ports;
			for (p = 0; p < ports; p++) {
				if (grp[p] != j) {
					continue;
				}
				cnt[p] -= (cnt[p] > 0);
				if (speeds[p] > sp && !cnt[p]) {
					win = p;
					sp = speeds[p];
				}
			}
			if (win == ports) {
				win = 10;
			}

			cnt[win] = grpspd/sp;
			cal[i * grps + j] = win;
		}
	}

	*cal_len = (cal[0] >= ports) ? 1 : (grps * grplen);
	return 0;
}

static void sparx5_dsm_set_calendar(struct sparx5_private *priv,
				    u32 taxi, u32 *calendar, u32 len)
{
	u32 active_calendar;
	u32 val;
	u32 i;

	val = spx5_rd(priv, DSM_TAXI_CAL_CFG(taxi));
	active_calendar = DSM_TAXI_CAL_CFG_CAL_SEL_STAT_GET(val);

	spx5_rmw(DSM_TAXI_CAL_CFG_CAL_PGM_SEL_SET(!active_calendar),
		 DSM_TAXI_CAL_CFG_CAL_PGM_SEL,
		 priv, DSM_TAXI_CAL_CFG(taxi));

	spx5_rmw(DSM_TAXI_CAL_CFG_CAL_PGM_ENA_SET(1),
		 DSM_TAXI_CAL_CFG_CAL_PGM_ENA,
		 priv, DSM_TAXI_CAL_CFG(taxi));

	for (i = 0; i < len; i++) {
		spx5_rmw(DSM_TAXI_CAL_CFG_CAL_IDX_SET(i),
			 DSM_TAXI_CAL_CFG_CAL_IDX,
			 priv, DSM_TAXI_CAL_CFG(taxi));

		spx5_rmw(DSM_TAXI_CAL_CFG_CAL_PGM_VAL_SET(calendar[i]),
			 DSM_TAXI_CAL_CFG_CAL_PGM_VAL,
			 priv, DSM_TAXI_CAL_CFG(taxi));
	}

	spx5_rmw(DSM_TAXI_CAL_CFG_CAL_PGM_ENA_SET(0),
		 DSM_TAXI_CAL_CFG_CAL_PGM_ENA,
		 priv, DSM_TAXI_CAL_CFG(taxi));

	val = spx5_rd(priv, DSM_TAXI_CAL_CFG(taxi));
	val = DSM_TAXI_CAL_CFG_CAL_CUR_LEN_GET(val);
	if (val != len - 1) {
		printf("Calendar length is not correct (%d) %d\n", val, len);
	}

	spx5_rmw(DSM_TAXI_CAL_CFG_CAL_SWITCH_SET(1),
		 DSM_TAXI_CAL_CFG_CAL_SWITCH,
		 priv, DSM_TAXI_CAL_CFG(taxi));
}

static void sparx5_lan969x_cal_cfg(struct sparx5_private *priv)
{
	u32 taxi_speeds[DSM_CAL_MAX_DEVS_PER_TAXI] = {};
	u32 taxi_ports[DSM_CAL_MAX_DEVS_PER_TAXI] = {0};
	u32 calendar[DSM_CAL_LEN], taxi;
	u32 freq_mhz = 328;
	u32 *dev_speeds;
	u32 cal_len, p;

	dev_speeds = devm_kzalloc(priv->dev,
				  priv->data->num_ports * sizeof(u32),
				  GFP_KERNEL);
	if (!dev_speeds)
		return;

	for (p = 0; p < priv->data->num_ports; p++) {
		dev_speeds[p] = 1000;
	}

	for (taxi = 0; taxi < DSM_CAL_TAXIS; taxi++) {
		sparx5_taxi2ports(taxi, taxi_ports);
		for (p = 0; p < DSM_CAL_MAX_DEVS_PER_TAXI; p++) {
			if (taxi_ports[p] < priv->data->num_ports) {
				taxi_speeds[p] = dev_speeds[taxi_ports[p]];
			} else {
				break;
			}
		}
		sparx5_dsm_calc_calendar(taxi_speeds, p, freq_mhz,
					 calendar, &cal_len);
		sparx5_dsm_set_calendar(priv, taxi, calendar, cal_len);
	}

	devm_kfree(priv->dev, dev_speeds);
}

static void sparx5_switch_config(struct sparx5_private *priv)
{
	int i;

	/* Halt the calendar while changing it */
	spx5_rmw(QSYS_CAL_CTRL_CAL_MODE_SET(10),
		 QSYS_CAL_CTRL_CAL_MODE,
		 priv, QSYS_CAL_CTRL);

	for (i = 0; i < priv->data->num_cal_auto; i++)
		/* All ports to '001' - 1Gb/s */
		spx5_wr(0x09249249, priv, QSYS_CAL_AUTO(i));

	/* Enable Auto mode */
	spx5_rmw(QSYS_CAL_CTRL_CAL_MODE_SET(8),
		 QSYS_CAL_CTRL_CAL_MODE,
		 priv, QSYS_CAL_CTRL);

	if (priv->data->target == LAN969X_TARGET) {
		sparx5_lan969x_cal_cfg(priv);

		spx5_wr(0x18624dd2, priv, PTP_CLK_PER_CFG(0, 1));
		spx5_rmw(PTP_PTP_DOM_CFG_PTP_ENA_SET(1),
			 PTP_PTP_DOM_CFG_PTP_ENA,
			 priv, PTP_PTP_DOM_CFG);
	}

	/* Configure NPI port */
	switch (priv->ports[priv->data->npi_port].mac_type) {
	case IF_SGMII:
		if (priv->data->target == SPARX5_TARGET) {
			spx5_rmw(PORT_CONF_DEV5G_MODES_DEV5G_D64_MODE_SET(1),
				 PORT_CONF_DEV5G_MODES_DEV5G_D64_MODE,
				 priv, PORT_CONF_DEV5G_MODES);
		}
		break;
	case IF_RGMII:
	case IF_SGMII_CISCO:
		/* Nothing to do here */
		break;
	default:
		spx5_rmw(PORT_CONF_DEV5G_MODES_DEV5G_D64_MODE_SET(0),
			 PORT_CONF_DEV5G_MODES_DEV5G_D64_MODE,
			 priv, PORT_CONF_DEV5G_MODES);
		break;
	}

	/* Enable all 5G, 10G and 25G ports to behave as 2.5G port */
	if (priv->data->target == SPARX5_TARGET) {
		spx5_wr(0x1fff, priv, PORT_CONF_DEV5G_MODES);
		spx5_wr(0xfff, priv, PORT_CONF_DEV10G_MODES);
		spx5_wr(0xff, priv, PORT_CONF_DEV25G_MODES);
	}

	/* As we currently support speeds of 1G or less, then it is OK to set
	 * all the ports that we support lower speeds than 2.5G.
	 */
	if (priv->data->target == LAN969X_TARGET) {
		spx5_wr(0xf117001, priv, PORT_CONF_DEV10G_MODES);
		spx5_wr(0x222200, priv, PORT_CONF_DEV5G_MODES);
	}

	for (i = 0; i < priv->data->num_ports; i++) {
		struct sparx5_phy_port *p = &priv->ports[i];

		if (p->active) {
			/* Enable 10G shadow interfaces */
			spx5_rmw(DSM_DEV_TX_STOP_WM_CFG_DEV10G_SHADOW_ENA_SET(1),
				 DSM_DEV_TX_STOP_WM_CFG_DEV10G_SHADOW_ENA,
				 priv, DSM_DEV_TX_STOP_WM_CFG(i));
		}

		if (p->mac_type == IF_QSGMII) {
			/* Enable the QSGMII interface */
			u32 val = spx5_rd(priv, PORT_CONF_QSGMII_ENA);
			val |= BIT(i / 4);
			spx5_wr(val, priv, PORT_CONF_QSGMII_ENA);

			/* Must take the PCS out of reset for all 4 QSGMII instances,
			 */
			for (u32 cnt = 0; cnt < 4; ++cnt) {
				u32 base = (i / 4) * 4;
				spx5_rmw(DEV2G5_DEV_RST_CTRL_PCS_TX_RST_SET(0),
					 DEV2G5_DEV_RST_CTRL_PCS_TX_RST,
					 priv, DEV2G5_DEV_RST_CTRL(base + cnt));
			}

			if (priv->data->target == SPARX5_TARGET) {
				if (i < 12)
					spx5_rmw(BIT(i), BIT(i),
						 priv, PORT_CONF_DEV5G_MODES);
				if (i >= 12 && i <= 15)
					spx5_rmw(BIT(i - 12), BIT(i - 12),
						 priv, PORT_CONF_DEV10G_MODES);

				if ((i / 4 % 2) == 0)
					/* Affects d0-d3,d8-d11..d40-d43 */
					spx5_wr(0x332, priv, PORT_CONF_USGMII_CFG(i / 8));
			}
		}
	}

	/* BCAST/CPU pgid */
	if (priv->data->target == SPARX5_TARGET) {
		spx5_wr(0xffffffff, priv, ANA_AC_PGID_CFG(PGID_BROADCAST(priv)));
		spx5_wr(0xffffffff, priv, ANA_AC_PGID_CFG1(PGID_BROADCAST(priv)));
		spx5_wr(0x00000001, priv, ANA_AC_PGID_CFG2(PGID_BROADCAST(priv)));
		spx5_wr(0x00000000, priv, ANA_AC_PGID_CFG(PGID_HOST(priv)));
		spx5_wr(0x00000000, priv, ANA_AC_PGID_CFG1(PGID_HOST(priv)));
		spx5_wr(0x00000000, priv, ANA_AC_PGID_CFG2(PGID_HOST(priv)));
	}

	if (priv->data->target == LAN969X_TARGET) {
		spx5_wr(0xffffffff, priv, ANA_AC_PGID_CFG(PGID_BROADCAST(priv)));
		spx5_wr(0x00000000, priv, ANA_AC_PGID_CFG(PGID_HOST(priv)));
	}

	/* Disable port-to-port by switching
	 * Put front ports in "port isolation modes" - i.e. they can't send
	 * to other ports - via the PGID sorce masks.
	 */
	for (i = 0; i < priv->data->num_ports; i++) {
		if (priv->data->target == SPARX5_TARGET) {
			spx5_wr(0, priv, ANA_AC_SRC_CFG(i));
			spx5_wr(0, priv, ANA_AC_SRC_CFG1(i));
			spx5_wr(0, priv, ANA_AC_SRC_CFG2(i));
		}

		if (priv->data->target == LAN969X_TARGET) {
			spx5_wr(0, priv, ANA_AC_SRC_CFG(i));
		}
	}

	spx5_wr(priv->data->cpu_port << 5, priv, XQS_STAT_CFG);

	/* VLAN aware CPU port */
	spx5_wr(ANA_CL_VLAN_CTRL_VLAN_AWARE_ENA_SET(1) |
		ANA_CL_VLAN_CTRL_VLAN_POP_CNT_SET(1) |
		ANA_CL_VLAN_CTRL_PORT_VID_SET(MAC_VID),
		priv, ANA_CL_VLAN_CTRL(priv->data->cpu_port));

	/* Map PVID = FID, DISABLE LEARNING  */
	spx5_wr(ANA_L3_VLAN_CFG_VLAN_FID_SET(MAC_VID) |
		ANA_L3_VLAN_CFG_VLAN_LRN_DIS_SET(1),
		priv, ANA_L3_VLAN_CFG(MAC_VID));

	/* Enable VLANs */
	spx5_wr(ANA_L3_VLAN_CTRL_VLAN_ENA_SET(1),
		priv, ANA_L3_VLAN_CTRL);

	/* Enable switch-core and queue system */
	spx5_wr(HSCH_RESET_CFG_CORE_ENA_SET(1),
		priv, HSCH_RESET_CFG);

	/* Flush queues */
	mscc_flush(priv->regs[TARGET_QS], sparx5_regs_qs);
}

static void sparx5_cpu_capture_setup(struct sparx5_private *priv)
{
	/* ASM CPU port: No preamble/IFH, enable padding */
	spx5_wr(ASM_PORT_CFG_NO_PREAMBLE_ENA_SET(1) |
		ASM_PORT_CFG_PAD_ENA_SET(1) |
		ASM_PORT_CFG_INJ_FORMAT_CFG_SET(CONFIG_IFH_FMT_NONE),
		priv, ASM_PORT_CFG(priv->data->cpu_port));

	/* Set Manual injection via DEVCPU_QS registers for CPU queue 0 */
	spx5_wr(QS_INJ_GRP_CFG_MODE_SET(1) |
		QS_INJ_GRP_CFG_BYTE_SWAP_SET(1),
		priv, QS_INJ_GRP_CFG(0));

	/* Set Manual extraction via DEVCPU_QS registers for CPU queue 0 */
	spx5_wr(QS_XTR_GRP_CFG_MODE_SET(1) |
		QS_XTR_GRP_CFG_STATUS_WORD_POS_SET(1) |
		QS_XTR_GRP_CFG_BYTE_SWAP_SET(1),
		priv, QS_XTR_GRP_CFG(0));

	/* Enable CPU port for any frame transfer */
	spx5_rmw(QFWD_SWITCH_PORT_MODE_PORT_ENA_SET(1),
		 QFWD_SWITCH_PORT_MODE_PORT_ENA,
		 priv, QFWD_SWITCH_PORT_MODE(priv->data->cpu_port));

	/* Recalc injected frame FCS */
	spx5_rmw(ANA_CL_FILTER_CTRL_FORCE_FCS_UPDATE_ENA_SET(1),
		 ANA_CL_FILTER_CTRL_FORCE_FCS_UPDATE_ENA,
		 priv, ANA_CL_FILTER_CTRL(priv->data->cpu_port));

	/* Send a copy to CPU when found as forwarding entry */
	spx5_rmw(ANA_L2_FWD_CFG_CPU_DMAC_COPY_ENA_SET(1),
		 ANA_L2_FWD_CFG_CPU_DMAC_COPY_ENA,
		 priv, ANA_L2_FWD_CFG);
}

static void sparx5_port_sgmii_init(struct sparx5_private *priv, int port)
{
	sparx5_serdes_port_init(priv->ports[port].serdes_phy,
				priv->ports[port].mac_type);

	/* Enable PCS */
	spx5_wr(DEV2G5_PCS1G_CFG_PCS_ENA_SET(1),
		priv, DEV2G5_PCS1G_CFG(port));

	/* Disable Signal Detect */
	spx5_wr(0, priv, DEV2G5_USXGMII_PCS_SD_CFG(port));

	/* Enable MAC RX and TX */
	spx5_wr(DEV2G5_MAC_ENA_CFG_TX_ENA_SET(1) |
		DEV2G5_MAC_ENA_CFG_RX_ENA_SET(1),
		priv, DEV2G5_MAC_ENA_CFG(port));

	/* Enable sgmii_mode_ena */
	spx5_wr(DEV2G5_PCS1G_MODE_CFG_SGMII_MODE_ENA_SET(1),
		priv, DEV2G5_PCS1G_MODE_CFG(port));

	if (priv->ports[port].mac_type == IF_SGMII ||
	    priv->ports[port].mac_type == IF_QSGMII) {
		/*
		 * Clear sw_resolve_ena(bit 0) and set adv_ability to
		 * something meaningful just in case
		 */
		spx5_wr(DEV2G5_PCS1G_ANEG_CFG_ADV_ABILITY_SET(0x20),
			priv, DEV2G5_PCS1G_ANEG_CFG(port));
	} else {
		/* IF_SGMII_CISCO */
		spx5_wr(DEV2G5_PCS1G_ANEG_CFG_ADV_ABILITY_SET(0x0001) |
			DEV2G5_PCS1G_ANEG_CFG_SW_RESOLVE_ENA_SET(1) |
			DEV2G5_PCS1G_ANEG_CFG_ANEG_ENA_SET(1) |
			DEV2G5_PCS1G_ANEG_CFG_ANEG_RESTART_ONE_SHOT_SET(1),
			priv, DEV2G5_PCS1G_ANEG_CFG(port));
	}

	/* Set MAC IFG Gaps */
	spx5_wr(DEV2G5_MAC_IFG_CFG_TX_IFG_SET(4) |
		DEV2G5_MAC_IFG_CFG_RX_IFG1_SET(5) |
		DEV2G5_MAC_IFG_CFG_RX_IFG2_SET(1),
		priv, DEV2G5_MAC_IFG_CFG(port));

	/* Set link speed and release all resets but USX */
	spx5_wr(DEV2G5_DEV_RST_CTRL_SPEED_SEL_SET(2) |
		DEV2G5_DEV_RST_CTRL_USX_PCS_TX_RST_SET(1) |
		DEV2G5_DEV_RST_CTRL_USX_PCS_RX_RST_SET(1),
		priv, DEV2G5_DEV_RST_CTRL(port));
}

static void sparx5_port_rgmii_init(struct sparx5_private *priv, int port)
{
	/* This function is called only on lan969x, and the RGMII ports are only
	 * on the D28 and D29. They map in the DEVRGMII
	 */
	int rgmii_index = port - 28;
	struct phy_device *phydev = priv->ports[port].phy;
	int spd = phydev->speed;
	int tx_clk_freq;
	u32 clk_spd;

	clk_spd = spd == SPEED_10 ? 0 : spd == SPEED_100 ? 1 : 2;
	tx_clk_freq = (spd == SPEED_10	? SPX5_RGMII_TX_CLK_2M5MHZ :
		       spd == SPEED_100	? SPX5_RGMII_TX_CLK_25MHZ :
					  SPX5_RGMII_TX_CLK_125MHZ);

	/* Enable the RGMII0 on the GPIOs */
	spx5_wr(HSIO_WRAP_XMII_CFG_GPIO_XMII_CFG_SET(1),
		priv, HSIO_WRAP_XMII_CFG(!rgmii_index));

	/* Take the RGMII out of reset and set speed to 1G */
	spx5_wr(HSIO_WRAP_RGMII_CFG_TX_CLK_CFG_SET(tx_clk_freq),
		priv, HSIO_WRAP_RGMII_CFG(rgmii_index));

	/* Enable the RGMII delays on the MAC both on the RX and TX.
	 * The signal is shft by 90 degress
	 */
	spx5_rmw(HSIO_WRAP_DLL_CFG_DLL_RST_SET(0) |
		 HSIO_WRAP_DLL_CFG_DLL_ENA_SET(1) |
		 HSIO_WRAP_DLL_CFG_DLL_CLK_ENA_SET(0) |
		 HSIO_WRAP_DLL_CFG_DLL_CLK_SEL_SET(3),
		 HSIO_WRAP_DLL_CFG_DLL_ENA |
		 HSIO_WRAP_DLL_CFG_DLL_RST |
		 HSIO_WRAP_DLL_CFG_DLL_CLK_ENA |
		 HSIO_WRAP_DLL_CFG_DLL_CLK_SEL,
		 priv, HSIO_WRAP_DLL_CFG(rgmii_index, 0));

	spx5_rmw(HSIO_WRAP_DLL_CFG_DLL_RST_SET(0) |
		 HSIO_WRAP_DLL_CFG_DLL_ENA_SET(1) |
		 HSIO_WRAP_DLL_CFG_DLL_CLK_ENA_SET(1) |
		 HSIO_WRAP_DLL_CFG_DLL_CLK_SEL_SET(3),
		 HSIO_WRAP_DLL_CFG_DLL_ENA |
		 HSIO_WRAP_DLL_CFG_DLL_RST |
		 HSIO_WRAP_DLL_CFG_DLL_CLK_ENA |
		 HSIO_WRAP_DLL_CFG_DLL_CLK_SEL,
		 priv, HSIO_WRAP_DLL_CFG(rgmii_index, 1));

	/* Configure the port now */
	spx5_wr(DEVRGMII_MAC_ENA_CFG_RX_ENA_SET(1) |
		DEVRGMII_MAC_ENA_CFG_TX_ENA_SET(1),
		priv, DEVRGMII_MAC_ENA_CFG(port));

	spx5_wr(DEVRGMII_MAC_IFG_CFG_TX_IFG_SET(4) |
		DEVRGMII_MAC_IFG_CFG_RX_IFG1_SET(5) |
		DEVRGMII_MAC_IFG_CFG_RX_IFG2_SET(1),
		priv, DEVRGMII_MAC_IFG_CFG(port));

	spx5_wr(DEVRGMII_DEV_RST_CTRL_SPEED_SEL_SET(clk_spd),
		priv, DEVRGMII_DEV_RST_CTRL(port));
}

static void sparx5_port_init(struct sparx5_private *priv, int port)
{
	switch (priv->ports[port].mac_type) {
	case IF_SGMII:
	case IF_QSGMII:
	case IF_SGMII_CISCO:
		sparx5_port_sgmii_init(priv, port);
		break;
	case IF_RGMII:
		sparx5_port_rgmii_init(priv, port);
		break;
	default:
		printf("Unknown interface type: %d\n",
		       priv->ports[port].mac_type);
		return;
	}

	/* Make VLAN aware for CPU traffic */
	spx5_wr(ANA_CL_VLAN_CTRL_VLAN_AWARE_ENA_SET(1) |
		ANA_CL_VLAN_CTRL_VLAN_POP_CNT_SET(1) |
		ANA_CL_VLAN_CTRL_PORT_VID_SET(MAC_VID),
		priv, ANA_CL_VLAN_CTRL(port));

	/* Enable CPU port for any frame transfer */
	spx5_rmw(QFWD_SWITCH_PORT_MODE_PORT_ENA_SET(1),
		 QFWD_SWITCH_PORT_MODE_PORT_ENA,
		 priv, QFWD_SWITCH_PORT_MODE(port));
}

static inline int sparx5_vlant_wait_for_completion(struct sparx5_private *priv)
{
	if (wait_for_bit_le32(spx5_offset(priv, LRN_COMMON_ACCESS_CTRL),
			      LRN_COMMON_ACCESS_CTRL_MAC_TABLE_ACCESS_SHOT,
			      false, 2000, false))
		return -ETIMEDOUT;

	return 0;
}

static void mac_table_write_entry(struct sparx5_private *priv,
				  const unsigned char *mac,
				  u16 vid)
{
	u32 macl = 0, mach = 0;

	/*
	 * Set the MAC address to handle and the vlan associated in a format
	 * understood by the hardware.
	 */
	mach |= vid << 16;
	mach |= ((u32)mac[0]) << 8;
	mach |= ((u32)mac[1]) << 0;
	macl |= ((u32)mac[2]) << 24;
	macl |= ((u32)mac[3]) << 16;
	macl |= ((u32)mac[4]) << 8;
	macl |= ((u32)mac[5]) << 0;

	spx5_wr(mach, priv, LRN_MAC_ACCESS_CFG_0);
	spx5_wr(macl, priv, LRN_MAC_ACCESS_CFG_1);
}

static int sparx5_mac_table_add(struct sparx5_private *priv,
				 const unsigned char *mac, int pgid)
{
	mac_table_write_entry(priv, mac, MAC_VID);

	spx5_wr(LRN_MAC_ACCESS_CFG_2_MAC_ENTRY_ADDR_SET(pgid - priv->data->num_ports) |
		LRN_MAC_ACCESS_CFG_2_MAC_ENTRY_ADDR_TYPE_SET(0x3) |
		LRN_MAC_ACCESS_CFG_2_MAC_ENTRY_CPU_COPY_SET(1) |
		LRN_MAC_ACCESS_CFG_2_MAC_ENTRY_CPU_QU_SET(0) |
		LRN_MAC_ACCESS_CFG_2_MAC_ENTRY_VLD_SET(1) |
		LRN_MAC_ACCESS_CFG_2_MAC_ENTRY_LOCKED_SET(1),
		priv, LRN_MAC_ACCESS_CFG_2);

	spx5_wr(LRN_COMMON_ACCESS_CTRL_MAC_TABLE_ACCESS_SHOT_SET(1),
		priv, LRN_COMMON_ACCESS_CTRL);

	return sparx5_vlant_wait_for_completion(priv);
}

static int sparx5_mac_table_getnext(struct sparx5_private *priv,
				     unsigned char *mac,
				     int *addr,
				     u32 *pvid,
				     u32 *cfg0p,
				     u32 *cfg1p,
				     u32 *cfg2p)
{
	int ret;

	mac_table_write_entry(priv, mac, *pvid);

	spx5_wr(LRN_SCAN_NEXT_CFG_SCAN_NEXT_UNTIL_FOUND_ENA_SET(1) |
		LRN_SCAN_NEXT_CFG_SCAN_NEXT_IGNORE_LOCKED_ENA_SET(1),
		priv, LRN_SCAN_NEXT_CFG);

	spx5_wr(LRN_COMMON_ACCESS_CTRL_CPU_ACCESS_CMD_SET(6) |
		LRN_COMMON_ACCESS_CTRL_MAC_TABLE_ACCESS_SHOT_SET(1),
		priv, LRN_COMMON_ACCESS_CTRL);

	ret = sparx5_vlant_wait_for_completion(priv);

	if (ret == 0) {
		u32 cfg0, cfg1, cfg2;
		cfg2 = spx5_rd(priv, LRN_MAC_ACCESS_CFG_2);
		if (cfg2 & LRN_MAC_ACCESS_CFG_2_MAC_ENTRY_VLD) {
			cfg0 = spx5_rd(priv, LRN_MAC_ACCESS_CFG_0);
			cfg1 = spx5_rd(priv, LRN_MAC_ACCESS_CFG_1);
			mac[0] = ((cfg0 >> 8)  & 0xff);
			mac[1] = ((cfg0 >> 0)  & 0xff);
			mac[2] = ((cfg1 >> 24) & 0xff);
			mac[3] = ((cfg1 >> 16) & 0xff);
			mac[4] = ((cfg1 >> 8)  & 0xff);
			mac[5] = ((cfg1 >> 0)  & 0xff);
			*addr = LRN_MAC_ACCESS_CFG_2_MAC_ENTRY_ADDR & cfg2;
			*pvid = cfg0 >> 16;
			*cfg0p = cfg0;
			*cfg1p = cfg1;
			*cfg2p = cfg2;
		} else {
			ret = 1;
		}
	}

	return ret;
}

static int sparx5_initialize(struct sparx5_private *priv)
{
	int ret, i;

	/* Initialize switch memories, enable core */
	ret = sparx5_switch_init(priv);
	if (ret)
		return ret;

	board_init();
	sparx5_switch_config(priv);

	for (i = 0; i < priv->data->num_ports; i++)
		if (priv->ports[i].active)
			sparx5_port_init(priv, i);

	sparx5_cpu_capture_setup(priv);

	return 0;
}

static bool sparx5_port_has_link(struct sparx5_private *priv, int port)
{
	u32 mask, val;

	if (priv->ports[port].mac_type == IF_RGMII)
		return priv->ports[port].phy->link;

	if (priv->ports[port].bus) {
		val = spx5_rd(priv, DEV2G5_PCS1G_LINK_STATUS(port));
		mask = DEV2G5_PCS1G_LINK_STATUS_LINK_STATUS;
	} else {
		val = spx5_rd(priv, DEV2G5_PCS1G_ANEG_STATUS(port));
		mask = DEV2G5_PCS1G_ANEG_STATUS_ANEG_COMPLETE;
	}

	return !!(val & mask);
}

static int sparx5_start(struct udevice *dev)
{
	const u8 mac[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	struct sparx5_private *priv = dev_get_priv(dev);
	struct eth_pdata *pdata = dev_get_plat(dev);
	int i, ret, phy_ok = 0, ret_err = 0;

	/* Set MAC address tables entries for CPU redirection */
	ret = sparx5_mac_table_add(priv, mac, PGID_BROADCAST(priv));
	if (ret)
		return ret;

	ret = sparx5_mac_table_add(priv, pdata->enetaddr, PGID_HOST(priv));
	if (ret)
		return ret;

	for (i = 0; i < priv->data->num_ports; i++) {
		struct phy_device *phy = priv->ports[i].phy;

		if (!priv->ports[i].active || !phy)
			continue;

		/* Start up the PHY */
		phy_config(phy);
		ret = phy_startup(phy);
		if (ret) {
			printf("Could not initialize PHY %s (port %d)\n",
			       phy->dev->name, i);
			ret_err = ret;
			continue; /* try all phys */
		} else {
			phy_ok = 1;
			if (i == priv->data->npi_port)
				printf("NPI Port: ");
			else
				printf("Port %3d: ", i);

			printf("%s (internal)\n", sparx5_port_has_link(priv, i) ? "Up" : "Down");
		}

		sparx5_port_init(priv, i);
	}

	return phy_ok ? 0 : ret_err;
}

static void sparx5_stop(struct udevice *dev)
{
	struct sparx5_private *priv = dev_get_priv(dev);
	int i;

	for (i = 0; i < priv->data->num_ports; i++) {
		struct phy_device *phy = priv->ports[i].phy;

		if (phy)
			phy_shutdown(phy);
	}
}

static int sparx5_send(struct udevice *dev, void *packet, int length)
{
	struct sparx5_private *priv = dev_get_priv(dev);

	return mscc_send(priv->regs[TARGET_QS], sparx5_regs_qs,
			 NULL, 0, packet, length);
}

static int sparx5_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct sparx5_private *priv = dev_get_priv(dev);
	u32 *rxbuf = (u32 *)net_rx_packets[0];
	int byte_cnt = 0;

	byte_cnt = mscc_recv(priv->regs[TARGET_QS], sparx5_regs_qs, rxbuf,
			     priv->data->ifh_len, false);

	*packetp = net_rx_packets[0];

	if (byte_cnt > 0) {
		if (byte_cnt >= ETH_FCS_LEN)
			byte_cnt -= ETH_FCS_LEN;
		else
			byte_cnt = 0; /* Runt? */
	}

	return byte_cnt;
}

static struct mii_dev *sparx5_get_mdiobus(struct sparx5_private *priv,
					   phys_addr_t base, unsigned long size)
{
	int i = 0;

	for (i = 0; i < priv->data->num_bus; ++i)
		if (priv->miim[i].miim_base == base &&
		    priv->miim[i].miim_size == size)
			return priv->miim[i].bus;

	return NULL;
}

static phy_interface_t sparx5_mac_to_phy_interface(u32 mac_type)
{
	switch (mac_type) {
	case IF_SGMII:
	case IF_SGMII_CISCO:
		return PHY_INTERFACE_MODE_SGMII;
	case IF_QSGMII:
		return PHY_INTERFACE_MODE_QSGMII;
	default:
		return PHY_INTERFACE_MODE_NA;
	};
}

static int sparx5_probe(struct udevice *dev)
{
	u32 serdes_args[SERDES_ARG_MAX];
	struct sparx5_private *priv;
	int miim_count = 0;
	int i;
	int ret;
	struct resource res;
	phys_addr_t addr_base;
	unsigned long addr_size;
	ofnode eth_node, node, mdio_node;
	size_t phy_addr;
	struct mii_dev *bus;
	struct ofnode_phandle_args phandle;

	priv = dev_get_priv(dev);
	if (!priv)
		return -EINVAL;

	priv->dev = dev;

	priv->data = (struct mscc_match_data*)dev_get_driver_data(dev);
	if (!priv->data)
		return -EINVAL;

	/* Allocate the resources dynamically */
	priv->regs = devm_kzalloc(dev,
				  priv->data->num_regs * sizeof(void __iomem *),
				  GFP_KERNEL);
	if (!priv->regs)
		return -ENOMEM;

	priv->miim = devm_kzalloc(dev,
				  priv->data->num_bus * sizeof(struct mscc_miim_dev),
				  GFP_KERNEL);
	if (!priv->miim)
		return -ENOMEM;

	priv->bus = devm_kzalloc(dev,
				 priv->data->num_bus * sizeof(struct mii_dev*),
				 GFP_KERNEL);
	if (!priv->bus)
		return -ENOMEM;

	priv->ports = devm_kzalloc(dev,
				   priv->data->num_ports * sizeof(struct sparx5_phy_port),
				   GFP_KERNEL);
	if (!priv->ports)
		return -ENOMEM;

	/* Get registers and map them to the private structure */
	for (i = 0; i < priv->data->num_regs; i++) {
		priv->regs[i] = dev_remap_addr_name(dev,
						    priv->data->reg_names[i]);
		if (!priv->regs[i]) {
			printf("Error can't get regs base addresses for %s\n",
			       priv->data->reg_names[i]);
			return -ENOMEM;
		}
	}

	priv->serdes = sparx5_serdes_probe(dev);
	if (IS_ERR(priv->serdes))
		return PTR_ERR(priv->serdes);

	/* iterate all the ports and find out on which bus they are */
	i = 0;
	eth_node = dev_read_first_subnode(dev);
	for (node = ofnode_first_subnode(eth_node);
	     ofnode_valid(node);
	     node = ofnode_next_subnode(node)) {
		if (ofnode_read_resource(node, 0, &res))
			return -ENOMEM;
		i = res.start;

		bus = NULL;
		ret = ofnode_parse_phandle_with_args(node, "phy-handle", NULL,
						     0, 0, &phandle);

		/* Do we have a PHY to worry about? */
		if (ret == 0) {
			/* Get phy address on mdio bus */
			if (ofnode_read_resource(phandle.node, 0, &res))
				return -ENOMEM;
			phy_addr = res.start;

			/* Get mdio node */
			mdio_node = ofnode_get_parent(phandle.node);

			if (ofnode_read_resource(mdio_node, 0, &res))
				return -ENOMEM;

			addr_base = res.start;
			addr_size = resource_size(&res);

			/* If the bus is new then create a new bus */
			bus = sparx5_get_mdiobus(priv, addr_base, addr_size);
			if (!bus) {
				bus = mscc_mdiobus_init(priv->miim, &miim_count,
							addr_base, addr_size);
				priv->bus[miim_count - 1] = bus;
				mscc_mdiobus_pinctrl_apply(mdio_node);
			}
		} else {
			bus = NULL;
			phy_addr = -1;
		}

		priv->ports[i].active = true;
		priv->ports[i].phy_addr = phy_addr;
		priv->ports[i].bus = bus;

		/* Get serdes info */
		ret = ofnode_read_u32_array(node, "phys", serdes_args, SERDES_ARG_MAX);
		if (ret) {
			ret = ofnode_read_u32_array(node, "phys", serdes_args, 1);
			if (ret) {
				printf("%s: Port %d no 'phys' properties?\n",
				       __func__, i);
				return -EINVAL;
			} else {
				priv->ports[i].mac_type = serdes_args[SERDES_ARG_MAC_TYPE];
			}
		} else {
			priv->ports[i].mac_type = serdes_args[SERDES_ARG_MAC_TYPE];
			priv->ports[i].serdes_type = serdes_args[SERDES_ARG_SERDES];
			priv->ports[i].serdes_index = serdes_args[SERDES_ARG_SER_IDX];
			priv->ports[i].serdes_phy = sparx5_serdes_phy_get(priv->serdes,
									  priv->ports[i].serdes_type,
									  priv->ports[i].serdes_index);
		}

		debug("%s: Add port %d bus %s addr %zd serdes %s serdes# %d\n",
		      __func__, i, bus ? bus->name : "(none)", phy_addr,
		      priv->ports[i].serdes_type == FA_SERDES_TYPE_6G ? "6g" : "10g",
		      priv->ports[i].serdes_index);
	}

	for (i = 0; i < priv->data->num_ports; i++) {
		struct phy_device *phy;

		if (!priv->ports[i].bus)
			continue;

		phy = phy_connect(priv->ports[i].bus,
				  priv->ports[i].phy_addr, dev,
				  sparx5_mac_to_phy_interface(priv->ports[i].mac_type));
		if (phy)
			priv->ports[i].phy = phy;
	}

	sparx5_initialize(priv);

	dev_priv = priv;

	return 0;
}

static int sparx5_remove(struct udevice *dev)
{
	struct sparx5_private *priv = dev_get_priv(dev);
	int i;

	for (i = 0; i < priv->data->num_bus; i++) {
		mdio_unregister(priv->bus[i]);
		mdio_free(priv->bus[i]);
	}

	/* Make sure the core is PROTECTED from reset */
	spx5_rmw(CPU_RESET_PROT_STAT_SYS_RST_PROT_VCORE,
		 CPU_RESET_PROT_STAT_SYS_RST_PROT_VCORE,
		 priv, CPU_RESET_PROT_STAT);

	/* Reset switch core */
	spx5_wr(GCB_SOFT_RST_SOFT_CHIP_RST_SET(1),
		priv, GCB_SOFT_RST);

	dev_priv = NULL;

	return 0;
}

static const struct eth_ops sparx5_ops = {
	.start        = sparx5_start,
	.stop         = sparx5_stop,
	.send         = sparx5_send,
	.recv         = sparx5_recv,
};

static const struct udevice_id mscc_sparx5_ids[] = {
	{.compatible = "mscc,vsc7558-switch", .data = (ulong)&mscc_sparx5_data },
	{.compatible = "microchip,lan969x-switch", .data = (ulong)&mscc_lan969x_data },
	{ /* Sentinel */ }
};

U_BOOT_DRIVER(sparx5) = {
	.name				= "sparx5-switch",
	.id				= UCLASS_ETH,
	.of_match			= mscc_sparx5_ids,
	.probe				= sparx5_probe,
	.remove				= sparx5_remove,
	.ops				= &sparx5_ops,
	.priv_auto			= sizeof(struct sparx5_private),
	.plat_auto			= sizeof(struct eth_pdata),
};

static int do_switch(struct cmd_tbl *cmdtp, int flag, int argc,
		     char * const argv[])
{
	struct sparx5_private *priv = dev_priv;
	u8 mac[ETH_ALEN];
	int i, addr, cnt = 0;
	u32 cfg0, cfg1, cfg2;

	if (priv) {
		memset(mac, 0, sizeof(mac));
		u32 vid = 0;
		while (sparx5_mac_table_getnext(priv, mac, &addr, &vid, &cfg0, &cfg1, &cfg2) == 0) {
			printf("%4d: %02x:%02x:%02x:%02x:%02x:%02x %d:%d 0x%08x 0x%08x 0x%08x\n",
			       cnt++,
			       mac[0], mac[1], mac[2],
			       mac[3], mac[4], mac[5], vid, addr, cfg0, cfg1, cfg2);
		}
		for (i = 0; i < priv->data->num_ports; i++)
			if (priv->ports[i].active) {
				u32 mask, val;
				if (priv->ports[i].bus) {
					val = spx5_rd(priv, DEV2G5_PCS1G_LINK_STATUS(i));
					mask = DEV2G5_PCS1G_LINK_STATUS_LINK_STATUS;
				} else {
					val = spx5_rd(priv, DEV2G5_PCS1G_ANEG_STATUS(i));
					mask = DEV2G5_PCS1G_ANEG_STATUS_ANEG_COMPLETE;
				}
				printf("%2d: Link %s (0x%08x)\n", i,
				       val & mask ? "Up" : "--",
				       val);
			}
	}

	return 0;
}

U_BOOT_CMD(
	switch,	3,	1,	do_switch,
	"display switch status",
	""
);
