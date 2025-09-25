// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2018 Microsemi Corporation
 */

#include <common.h>
#include <miiphy.h>
#include <dm/uclass.h>
#include <dm/uclass-internal.h>
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

#include "mscc_sparx5_regs_devcpu_gcb.h"

#if !defined(CONFIG_ENABLE_ARM_SOC_BOOT0_HOOK)
#error "Sorry, we need this"
#endif

DECLARE_GLOBAL_DATA_PTR;

enum {
	BOARD_TYPE_PCB134 = 134,
	BOARD_TYPE_PCB135,
};

#if defined(CONFIG_DESIGNWARE_SPI)
static bool spi_cs_init;
static u32 spi2mask;
#endif

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
	/* Make sure the core is UN-protected from reset */
	clrbits_le32(CPU_RESET_PROT_STAT(SPARX5_CPU_BASE),
		     CPU_RESET_PROT_STAT_SYS_RST_PROT_VCORE_M);

	writel(GCB_SOFT_RST_SOFT_CHIP_RST_M, GCB_SOFT_RST(SPARX5_GCB_BASE));
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
	printch('C');
	if (get_sctlr() & CR_M)
		return 0;

	printch('I');
	invalidate_dcache_all();
	__asm_invalidate_tlb_all();
	early_mmu_setup();
	printch('i');
	set_sctlr(get_sctlr() | CR_C);
	printch('c');

	return 0;
}

void enable_caches(void)
{
	/* Enable D-cache. I-cache is already enabled in boot0 */
	dcache_enable();
}

#if defined(CONFIG_ARCH_MISC_INIT)
int arch_misc_init(void)
{
	int val;
	uintptr_t lcpll = SPARX5_LCPLL28_1_BASE;

#if defined(CONFIG_ARMV8_MULTIENTRY)
	u64 r_start = (u64)_start;

	debug("Enable CPU1: Start at 0x%08llx\n", r_start);
	writel((u32)((r_start >> 0) >> 2), CPU_CPU1_RVBAR_LSB(SPARX5_CPU_BASE));
	writel((u32)((r_start >> 32) >> 2), CPU_CPU1_RVBAR_MSB(SPARX5_CPU_BASE));
	debug("Reset vector: 0x%08x:0x%08x\n",
	      readl(CPU_CPU1_RVBAR_MSB(SPARX5_CPU_BASE)),
	      readl(CPU_CPU1_RVBAR_LSB(SPARX5_CPU_BASE)));
	clrbits_le32(CPU_RESET(SPARX5_CPU_BASE), CPU_RESET_CPU_CORE_1_COLD_RST(1));
#endif

	/* Enable DPLL fractional mode */
	val = readl(LCPLL28_LCPLL_CONFIG2(lcpll));
	if (FIELD_GET(LCPLL28_LCPLL_CONFIG2_F_M, val) == 0) {
		val = readl(GCB_HW_STAT(SPARX5_GCB_BASE));

		switch (FIELD_GET(GCB_HW_STAT_PLL0_CONF_M, val)) {
		case 0: val = 80;  break;  /* 125Mhz   */
		case 1: val = 64;  break;  /* 156.2Mhz */
		case 4: val = 400; break;  /* 25Mhz    */
		default:
			printf("PLL0 value not supported");
			return -ENOTSUPP;
		}

		/* Configure F(x) + R(511) + PDSIG(0) */
		clrsetbits_le32(LCPLL28_LCPLL_CONFIG2(lcpll),
				LCPLL28_LCPLL_CONFIG2_F_M,
				LCPLL28_LCPLL_CONFIG2_F(val));

		clrsetbits_le32(LCPLL28_LCPLL_CONFIG3(lcpll),
				LCPLL28_LCPLL_CONFIG3_R_M,
				LCPLL28_LCPLL_CONFIG3_R(511));

		clrbits_le32(LCPLL28_LCPLL_CONFIG3(lcpll),
			     LCPLL28_LCPLL_CONFIG3_PDSIG_M);
	}

	return 0;
}
#endif

#if defined(CONFIG_DESIGNWARE_SPI)
#define SPI_OWNER_MASK				\
	CPU_GENERAL_CTRL_IF_SI2_OWNER_M |	\
	CPU_GENERAL_CTRL_IF_SI_OWNER_M

#define SPI_OWNER_SPI1_MASK		   \
	CPU_GENERAL_CTRL_IF_SI2_OWNER(1) | \
	CPU_GENERAL_CTRL_IF_SI_OWNER(2)

#define SPI_OWNER_SPI2_MASK		   \
	CPU_GENERAL_CTRL_IF_SI2_OWNER(2) | \
	CPU_GENERAL_CTRL_IF_SI_OWNER(1)

static inline void spi_set_owner(u32 cs)
{
	u32 owbits;

	if (spi2mask & BIT(cs)) {
		owbits = SPI_OWNER_SPI2_MASK;
	} else {
		owbits = SPI_OWNER_SPI1_MASK;
	}
	clrsetbits_le32(CPU_GENERAL_CTRL(SPARX5_CPU_BASE),
			SPI_OWNER_MASK, owbits);
	udelay(1);
}

static void spi_manage_init(struct udevice *dev)
{
	struct udevice *paren = dev_get_parent(dev);

	spi2mask = dev_read_u32_default(paren, "interface-mapping-mask", 0);
	debug("spi2mask = %08x\n", spi2mask);
}

void external_cs_manage(struct udevice *dev, bool nen)
{
	bool enable = !nen;
	u32 cs = spi_chip_select(dev);

	if (!spi_cs_init) {
		spi_manage_init(dev);
		spi_cs_init = true;
	}
	if (enable) {
		u32 mask = ~BIT(cs); /* Active low */
		/* CS - start disabled */
		clrsetbits_le32(CPU_SS_FORCE(SPARX5_CPU_BASE),
				CPU_SS_FORCE_SS_FORCE_M,
				CPU_SS_FORCE_SS_FORCE(0xffff));
		/* CS override drive enable */
		clrsetbits_le32(CPU_SS_FORCE_ENA(SPARX5_CPU_BASE),
				CPU_SS_FORCE_ENA_SS_FORCE_ENA_M,
				CPU_SS_FORCE_ENA_SS_FORCE_ENA(true));
		udelay(1);
		/* Set owner */
		spi_set_owner(cs);
		/* CS - enable now */
		clrsetbits_le32(CPU_SS_FORCE(SPARX5_CPU_BASE),
				CPU_SS_FORCE_SS_FORCE_M,
				CPU_SS_FORCE_SS_FORCE(mask));

	} else {
		/* CS value */
		clrsetbits_le32(CPU_SS_FORCE(SPARX5_CPU_BASE),
				CPU_SS_FORCE_SS_FORCE_M,
				CPU_SS_FORCE_SS_FORCE(0xffff));
		udelay(1);
		/* CS override drive disable */
		clrsetbits_le32(CPU_SS_FORCE_ENA(SPARX5_CPU_BASE),
				CPU_SS_FORCE_ENA_SS_FORCE_ENA_M,
				CPU_SS_FORCE_ENA_SS_FORCE_ENA(false));
		udelay(1);
		/* SI owner = SIBM */
		clrsetbits_le32(CPU_GENERAL_CTRL(SPARX5_CPU_BASE),
				SPI_OWNER_MASK,
				CPU_GENERAL_CTRL_IF_SI2_OWNER(2) |
				CPU_GENERAL_CTRL_IF_SI_OWNER(1));
	}
}
#endif

static int _probe_gpio(uintptr_t gpio_cfg, uintptr_t gpio_in, u32 mask)
{
	u32 val;

	/* Enable pull-down */
	clrsetbits_le32(gpio_cfg,
			HSIO_WRAP_GPIO_CFG_G_PU(1),
			HSIO_WRAP_GPIO_CFG_G_PD(1));

	/* Settle delay */
	mdelay(1);

	val = readl(gpio_in);

	/* Reset to default - pull-up */
	clrsetbits_le32(gpio_cfg,
			HSIO_WRAP_GPIO_CFG_G_PD(1),
			HSIO_WRAP_GPIO_CFG_G_PU(1));

	return !!(val & mask);
}

static int probe_gpio(unsigned int gpio)
{
	assert(gpio < 64);
	if (gpio < 32)
		return _probe_gpio(HSIO_WRAP_GPIO_CFG(SPARX5_HSIO_WRAP_BASE, gpio),
				   GCB_GPIO_IN(SPARX5_GCB_BASE),
				   BIT(gpio));
	if (gpio < 64)
		return _probe_gpio(HSIO_WRAP_GPIO_CFG(SPARX5_HSIO_WRAP_BASE, gpio),
				   GCB_GPIO_IN1(SPARX5_GCB_BASE),
				   BIT(gpio - 32u));
	return 0;
}

static void do_board_detect(void)
{
	/* GPIO20 has extern pull-down, so will still be high */
	if (probe_gpio(20))
		gd->board_type = BOARD_TYPE_PCB134;
	else
		gd->board_type = BOARD_TYPE_PCB135;
}

#if defined(CONFIG_MULTI_DTB_FIT)
int board_fit_config_name_match(const char *name)
{
#if defined(CONFIG_MMC_SDHCI)
#define PCB_SUFFIX "_emmc"
#else
#define PCB_SUFFIX "_nand"
#endif
	if (gd->board_type == BOARD_TYPE_PCB134 &&
	    strcmp(name, "sparx5_pcb134" PCB_SUFFIX) == 0)
		return 0;

	if (gd->board_type == BOARD_TYPE_PCB135 &&
	    strcmp(name, "sparx5_pcb135" PCB_SUFFIX) == 0)
		return 0;

	return -1;
#undef PCB_SUFFIX
}
#endif

#if defined(CONFIG_DTB_RESELECT)
int embedded_dtb_select(void)
{
	do_board_detect();
	fdtdec_setup();

	return 0;
}
#endif

int board_init(void)
{
	int i;

	debug("Board init\n");

	/* CS0 alone on boot master region 0 - SPI NOR flash */
	writel((u16)~BIT(0), GCB_SPI_MASTER_SS0_MASK(SPARX5_GCB_BASE));

	switch (gd->board_type) {
	case BOARD_TYPE_PCB135:
		// Take pcb135 out of reset
		writel(0x93000, MSCC_DEVCPU_GCB_GPIO_OE);
		writel(0x80000, MSCC_DEVCPU_GCB_GPIO_OUT_CLR);
		writel(0x80000, MSCC_DEVCPU_GCB_GPIO_OUT_SET);
		break;
	case BOARD_TYPE_PCB134:
		writel(0x00060051, MSCC_DEVCPU_GCB_SIO_CFG(2));
		writel(0x00001410, MSCC_DEVCPU_GCB_SIO_CLOCK(2));
		writel(0xfffff000, MSCC_DEVCPU_GCB_SIO_PORT_ENA(2));
		writel(0x00000fff, MSCC_DEVCPU_GCB_SIO_INTR_POL(2, 0));
		writel(0xffffffff, MSCC_DEVCPU_GCB_SIO_INTR_POL(2, 1));
		writel(0x00000fff, MSCC_DEVCPU_GCB_SIO_INTR_POL(2, 2));
		writel(0xffffffff, MSCC_DEVCPU_GCB_SIO_INTR_POL(2, 3));
		for (i = 12; i < 32; i++)
			writel(0x00049000, MSCC_DEVCPU_GCB_SIO_PORT_CFG(2, i));

		break;
	}

	return 0;
}

int board_late_init(void)
{
#if defined(CONFIG_MMC_SDHCI)
#define PCB_SUFFIX "_emmc"
#else
#define PCB_SUFFIX ""
#endif
	if (env_get("pcb"))
		return 0;	/* Overridden, it seems */
	switch (gd->board_type) {
	case BOARD_TYPE_PCB134:
		env_set("pcb", "pcb134" PCB_SUFFIX);
		break;
	case BOARD_TYPE_PCB135:
		env_set("pcb", "pcb135" PCB_SUFFIX);
		break;
	default:
		env_set("pcb", "unknown");
	}
	return 0;
#undef PCB_SUFFIX
}

void board_quiesce_devices(void)
{
	struct uclass *uc_dev;
	int ret;

	/* Removes all RVU PF devices */
	ret = uclass_get(UCLASS_ETH, &uc_dev);
	if (uc_dev)
		ret = uclass_destroy(uc_dev);
	if (ret)
		printf("couldn't remove ethernet devices\n");
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

	debug_uart_init();

	printch('*');

	/* Init armv8 timer ticks  */
	writel(0, CPU_SYSCNT_CNTCVL(syscnt)); /* Low */
	writel(0, CPU_SYSCNT_CNTCVU(syscnt)); /* High */
	writel(CPU_SYSCNT_CNTCR_CNTCR_EN(1),
	       CPU_SYSCNT_CNTCR(syscnt));     /* Enable */

	/* Release GIC reset */
	clrbits_le32(CPU_RESET(SPARX5_CPU_BASE), CPU_RESET_GIC_RST(1));
}

// Helper for MMC backup image
static int on_mmc_cur(const char *name, const char *value, enum env_op op,
		      int flags)
{
	ulong mmc_cur = simple_strtoul(value, NULL, 16);

	debug("%s is %s\n", name, value);

	env_set_ulong("mmc_bak", mmc_cur == 1 ? 2 : 1);

	return 0;
}

U_BOOT_ENV_CALLBACK(mmc_cur, on_mmc_cur);

// Helper for NAND backup image
static int on_nand_cur(const char *name, const char *value, enum env_op op,
		       int flags)
{
	ulong nand_cur = simple_strtoul(value, NULL, 16);

	debug("%s is %s\n", name, value);

	env_set_ulong("nand_bak", nand_cur == 0 ? 1 : 0);

	return 0;
}

U_BOOT_ENV_CALLBACK(nand_cur, on_nand_cur);
