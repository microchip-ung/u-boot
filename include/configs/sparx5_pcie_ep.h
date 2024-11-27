/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2018 Microsemi Corporation
 */

#ifndef __INCL_INCLUDE_CONFIGS_SPARX5__
#define __INCL_INCLUDE_CONFIGS_SPARX5__

#include <linux/sizes.h>

#define PHYS_SPI			UL(0x000000000)
#define PHYS_NAND			UL(0x400000000)
#define PHYS_DEVICE_REG			UL(0x600000000)
#define PHYS_DDR			UL(0x700000000)
#define VIRT_DDR			UL(0x700000000) /* 1:1 */
#define PHYS_SDRAM_1_SIZE		SZ_2G
#define PHYS_PCIE_TARGET		UL(0x900000000)

#define PHYS_SDRAM_1                    PHYS_DDR
#define VIRT_SDRAM_1                    VIRT_DDR

#define CFG_SYS_INIT_RAM_ADDR		PHYS_SRAM_ADDR
#define CFG_SYS_INIT_RAM_SIZE		PHYS_SRAM_SIZE
#define CFG_SYS_SDRAM_BASE		VIRT_SDRAM_1

#ifdef CONFIG_GICV3
#define GICD_BASE                       UL(0x600300000)
#define GICR_BASE                       UL(0x600340000)
#endif

#define PHYS_SRAM_ADDR			UL(0x630000000)
#define PHYS_SRAM_SIZE			SZ_64K
#define PHYS_SRAM_MEM_ADDR		UL(0x632000000)
#define PHYS_SRAM_MEM_SIZE		SZ_32K

#define ENV_PCB		"pcb:sc,pcb_rev:do,"

#if defined(CONFIG_MMC_SDHCI)
#define CFG_ENV_CALLBACK_LIST_STATIC ENV_PCB "mmc_cur:mmc_cur,"
#else
#define CFG_ENV_CALLBACK_LIST_STATIC ENV_PCB "nand_cur:nand_cur,"
#endif

#define CFG_ENV_FLAGS_LIST_STATIC "pcb:sc,pcb_rev:do"

#if defined(CONFIG_MMC_SDHCI)
/* Need writeb */
#define CONFIG_MMC_SDHCI_IO_ACCESSORS
#endif

#define CFG_SYS_BOOTMAPSZ	SZ_64M	/* Initial map for Linux*/

#define PHY_ANEG_TIMEOUT		20000

#endif	/* __INCL_INCLUDE_CONFIGS_SPARX5__ */
