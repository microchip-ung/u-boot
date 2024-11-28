/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __DDR_PLATFORM_H
#define __DDR_PLATFORM_H

#include <stdint.h>
#include <asm/io.h>
#include <common.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <debug_uart.h>

//#define READ_DEBUG
//#define WRITE_DEBUG

#if !defined(DEBUG)
# define DEBUG(x...)
#endif

#if !defined(VERBOSE)
# define VERBOSE(...)
#endif

#if !defined(INFO)
# define INFO(...) printf(__VA_ARGS__)
#endif

#if !defined(NOTICE)
# define NOTICE(...) printf(__VA_ARGS__)
#endif

#if !defined(TRACE)
# define TRACE(s)
#endif

#if !defined(PANIC)
# define PANIC(...) panic(__VA_ARGS__)
#endif

#if !defined(BIT_32)
#define BIT_32(x)	BIT(x)
#endif

#if !defined(GENMASK_32)
#define GENMASK_32(h, l)	GENMASK(h, l)
#endif

extern void ddr_nsleep(u32 t_nsec);

static inline void ddr_usleep(int usec)
{
	udelay(usec);
}

typedef unsigned long ddr_timeout_t;

static inline ddr_timeout_t timeout_set_us(uint32_t val)
{
	return timer_get_us() + val;
}

static inline bool timeout_elapsed(ddr_timeout_t *timeout)
{
	ddr_timeout_t now = timer_get_us();
	return now > *timeout;
}

static inline uint32_t mmio_read_32(uintptr_t addr)
{
	uint32_t val = readl(addr);
#if defined(READ_DEBUG)
	printch('R');
	printhex2(((u64)addr) >> 32);
	printhex8((ulong) addr);
	printch('=');
	printhex8(val);
	printch('\n');
#endif
	return val;
}

static	inline void mmio_write_32(uintptr_t addr, uint32_t val)
{
	writel(val, addr);
#if defined(WRITE_DEBUG)
	printch('W');
	printhex2(((u64)addr) >> 32);
	printhex8((ulong) addr);
	printch(',');
	printhex8(val);
	printch('\n');
#endif
}

static inline void mmio_setbits_32(uintptr_t addr, uint32_t set)
{
	mmio_write_32(addr, mmio_read_32(addr) | set);
}

static inline void mmio_clrbits_32(uintptr_t addr, uint32_t clr)
{
	mmio_write_32(addr, mmio_read_32(addr) & ~clr);
}

static inline void mmio_clrsetbits_32(uintptr_t addr, uint32_t clr, uint32_t set)
{
	mmio_write_32(addr, (mmio_read_32(addr) & ~clr) | set);
}

#endif /* __DDR_PLATFORM_H */
