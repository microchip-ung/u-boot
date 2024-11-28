// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2024 Microchip Technology Inc. and its subsidiaries.
 *
 * Boot Sparx5 in PCIe Endpoint mode
 *
 */

#define DEBUG

#include <common.h>
#include <miiphy.h>
#include <asm/armv8/mmu.h>
#include <asm/io.h>
#include <linux/sizes.h>
#include <led.h>
#include <debug_uart.h>
#include <spi.h>
#include <asm/sections.h>
#include <linux/delay.h>
#include <linux/bitfield.h>

#include <sparx5_regs.h>
#include <mscc_sparx5_regs_devcpu_gcb.h>

#if !defined(CONFIG_ENABLE_ARM_SOC_BOOT0_HOOK)
#error "Sorry, we need this"
#endif

extern void sparx5_pcie_ep_init(void);

static struct mm_region fa_mem_map[] = {
	{
		.virt = VIRT_SDRAM_1,
		.phys = PHYS_SDRAM_1,
		.size = PHYS_SDRAM_1_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		.virt = PHYS_SPI,
		.phys = PHYS_SPI,
		.size = 1UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 (UL(3) << 6) | /* Read-only */
			 PTE_BLOCK_INNER_SHARE
	}, {
		.virt = PHYS_DEVICE_REG,
		.phys = PHYS_DEVICE_REG,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = fa_mem_map;

void reset_cpu(void)
{
}

static inline void mscc_gpio_set_alternate_0(int gpio, int mode)
{
	u32 mask = BIT(gpio);
	u32 val0, val1;

	val0 = readl(GCB_GPIO_ALT(SPARX5_GCB_BASE, 0));
	val1 = readl(GCB_GPIO_ALT(SPARX5_GCB_BASE, 1));
	if (mode == 1) {
		val0 |= mask;
		val1 &= ~mask;
	} else if (mode == 2) {
		val0 &= ~mask;
		val1 |= mask;
	} else if (mode == 3) {
		val0 |= mask;
		val1 |= mask;
	} else {
		val0 &= ~mask;
		val1 &= ~mask;
	}
	writel(val0, GCB_GPIO_ALT(SPARX5_GCB_BASE, 0));
	writel(val1, GCB_GPIO_ALT(SPARX5_GCB_BASE, 1));
}

static inline void mscc_gpio_set_alternate_1(int gpio, int mode)
{
	u32 mask = BIT(gpio);
	u32 val0, val1;

	val0 = readl(GCB_GPIO_ALT1(SPARX5_GCB_BASE, 0));
	val1 = readl(GCB_GPIO_ALT1(SPARX5_GCB_BASE, 1));
	if (mode == 1) {
		val0 |= mask;
		val1 &= ~mask;
	} else if (mode == 2) {
		val0 &= ~mask;
		val1 |= mask;
	} else if (mode == 3) {
		val0 |= mask;
		val1 |= mask;
	} else {
		val0 &= ~mask;
		val1 &= ~mask;
	}
	writel(val0, GCB_GPIO_ALT1(SPARX5_GCB_BASE, 0));
	writel(val1, GCB_GPIO_ALT1(SPARX5_GCB_BASE, 1));
}

static inline void mscc_gpio_set_alternate(int gpio, int mode)
{
	if (gpio < 32)
		mscc_gpio_set_alternate_0(gpio, mode);
	else
		mscc_gpio_set_alternate_1(gpio - 32, mode);
}

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
void board_debug_uart_init(void)
{
	mscc_gpio_set_alternate(10, 1);
	mscc_gpio_set_alternate(11, 1);
}
#endif

int print_cpuinfo(void)
{
	printf("CPU:   ARM A53\n");
	return 0;
}

static inline void early_mmu_setup(void)
{
	unsigned int el = current_el();

	gd->arch.tlb_addr = PHYS_SRAM_MEM_ADDR;
	gd->arch.tlb_fillptr = gd->arch.tlb_addr;
	gd->arch.tlb_size = PHYS_SRAM_MEM_SIZE;

	/* Create early page tables */
	setup_pgtables();

	/* point TTBR to the new table */
	set_ttbr_tcr_mair(el, gd->arch.tlb_addr,
			  get_tcr(NULL, NULL) &
			  ~(TCR_ORGN_MASK | TCR_IRGN_MASK),
			  MEMORY_ATTRIBUTES);

	set_sctlr(get_sctlr() | CR_M);
}

int arch_cpu_init(void)
{
	/*
	 * This function is called before U-Boot relocates itself to speed up
	 * on system running. It is not necessary to run if performance is not
	 * critical. Skip if MMU is already enabled by SPL or other means.
	 */
	if (get_sctlr() & CR_M)
		return 0;

	invalidate_dcache_all();
	__asm_invalidate_tlb_all();
	early_mmu_setup();
	set_sctlr(get_sctlr() | CR_C);

	return 0;
}

void enable_caches(void)
{
	/* Enable D-cache. I-cache is already enabled in boot0 */
	dcache_enable();
}

struct serial_device *default_serial_console(void)
{
	return NULL;
}

int board_init(void)
{
	return 0;
}

int board_late_init(void)
{
	return 0;
}

int dram_init(void)
{
	return 0;
}

void boot0(void)
{
	uintptr_t syscnt = SPARX5_CPU_SYSCNT_BASE;

	/* Speed up Flash reads */
	clrsetbits_le32(CPU_SPI_MST_CFG(SPARX5_CPU_BASE),
			CPU_SPI_MST_CFG_CLK_DIV_M |
			CPU_SPI_MST_CFG_FAST_READ_ENA_M,
			CPU_SPI_MST_CFG_CLK_DIV(28) | /* 250Mhz/X */
			CPU_SPI_MST_CFG_FAST_READ_ENA(1));

	/* Speed up instructions */
	icache_enable();

	/* Init armv8 timer ticks  */
	writel(0, CPU_SYSCNT_CNTCVL(syscnt)); /* Low */
	writel(0, CPU_SYSCNT_CNTCVU(syscnt)); /* High */
	writel(CPU_SYSCNT_CNTCR_CNTCR_EN(1),
	       CPU_SYSCNT_CNTCR(syscnt));     /* Enable */

	/* Release GIC reset */
	clrbits_le32(CPU_RESET(SPARX5_CPU_BASE), CPU_RESET_GIC_RST(1));
}
