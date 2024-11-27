// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024 Microchip
 */

#include <common.h>
#include <debug_uart.h>
#include <spl.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/bitops.h>

DECLARE_GLOBAL_DATA_PTR;

u32 spl_boot_device(void)
{
	return 0;
}

__weak int board_early_init_f(void)
{
	return 0;
}

__weak int arch_cpu_init(void)
{
	return 0;
}

void board_init_f(ulong dummy)
{
#ifdef CONFIG_DEBUG_UART
	/*
	 * Debug UART can be used from here if required:
	 *
	 * debug_uart_init();
	 * printch('a');
	 * printhex8(0x1234);
	 * printascii("string");
	 */
	debug_uart_init();
	printch('!');
	debug("\nspl:debug uart enabled in %s\n", __func__);
#endif

	board_early_init_f();
}
