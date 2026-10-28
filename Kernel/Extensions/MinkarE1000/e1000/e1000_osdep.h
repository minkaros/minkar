/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * What Intel's shared e1000 code (the other files of this directory,
 * FreeBSD's sys/dev/e1000, BSD-3-Clause) expects from the system, for
 * MinkarE1000 on XNU: types, delays, debug output, register and flash
 * access through the mapped BAR0, and PCI configuration (MinkarE1000.cpp).
 */
#ifndef _MINKAR_E1000_OSDEP_H_
#define _MINKAR_E1000_OSDEP_H_

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/param.h>
#include <string.h>
#include <IOKit/IOLib.h>

typedef uint64_t	u64;
typedef uint32_t	u32;
typedef uint16_t	u16;
typedef uint8_t		u8;
typedef int64_t		s64;
typedef int32_t		s32;
typedef int16_t		s16;
typedef int8_t		s8;

u32 e1000_translate_register_82542(u32 reg);

#define	__le16		u16
#define	__le32		u32
#define	__le64		u64

#define	STATIC		static
#ifndef ETHER_ADDR_LEN
#define	ETHER_ADDR_LEN	6
#endif
#define	CMD_MEM_WRT_INVALIDATE	0x0010	/* PCI command: memory write and invalidate */
#define	PCI_COMMAND_REGISTER	0x04

/* Waits: sleeping ones where the code may sleep, busy ones where not. */
#define	usec_delay(x)		IODelay(x)
#define	usec_delay_irq(x)	IODelay(x)
#define	msec_delay(x)		IOSleep(x)
#define	msec_delay_irq(x)	IODelay((x) * 1000)

/* Debug output, off unless the driver sets minkar_e1000_debug. */
extern int minkar_e1000_debug;
#define	DEBUGOUT(...)	do { if (minkar_e1000_debug) IOLog(__VA_ARGS__); } while (0)
#define	DEBUGOUT1(...)	DEBUGOUT(__VA_ARGS__)
#define	DEBUGOUT2(...)	DEBUGOUT(__VA_ARGS__)
#define	DEBUGOUT3(...)	DEBUGOUT(__VA_ARGS__)
#define	DEBUGOUT7(...)	DEBUGOUT(__VA_ARGS__)
#define	DEBUGFUNC(F)	DEBUGOUT("e1000: " F "\n")

#define	ASSERT(x)
#define	ASSERT_CTX_LOCK_HELD(hw)
#define	ASSERT_NO_LOCKS()

/* hw->back: the driver, for PCI configuration. */
struct e1000_osdep {
	void	*driver;
};

/* 82542 registers sit at other offsets (e1000_translate_register_82542). */
#define	E1000_REGISTER(hw, reg) (((hw)->mac.type >= e1000_82543) ? \
	(u32)(reg) : e1000_translate_register_82542(reg))

#define	E1000_READ_REG(hw, reg) \
	(*(volatile u32 *)((hw)->hw_addr + E1000_REGISTER(hw, reg)))
#define	E1000_WRITE_REG(hw, reg, value) \
	(*(volatile u32 *)((hw)->hw_addr + E1000_REGISTER(hw, reg)) = (u32)(value))
#define	E1000_WRITE_FLUSH(hw)	((void)E1000_READ_REG(hw, E1000_STATUS))

#define	E1000_READ_REG_ARRAY(hw, reg, index) \
	(*(volatile u32 *)((hw)->hw_addr + E1000_REGISTER(hw, reg) + ((index) << 2)))
#define	E1000_WRITE_REG_ARRAY(hw, reg, index, value) \
	(*(volatile u32 *)((hw)->hw_addr + E1000_REGISTER(hw, reg) + ((index) << 2)) = (u32)(value))
#define	E1000_READ_REG_ARRAY_DWORD	E1000_READ_REG_ARRAY
#define	E1000_WRITE_REG_ARRAY_DWORD	E1000_WRITE_REG_ARRAY
#define	E1000_READ_REG_ARRAY_BYTE(hw, reg, index) \
	(*(volatile u8 *)((hw)->hw_addr + E1000_REGISTER(hw, reg) + (index)))
#define	E1000_WRITE_REG_ARRAY_BYTE(hw, reg, index, value) \
	(*(volatile u8 *)((hw)->hw_addr + E1000_REGISTER(hw, reg) + (index)) = (u8)(value))
#define	E1000_WRITE_REG_ARRAY_WORD(hw, reg, index, value) \
	(*(volatile u16 *)((hw)->hw_addr + E1000_REGISTER(hw, reg) + ((index) << 1)) = (u16)(value))
/* The I/O BAR is not mapped: the same register through memory. */
#define	E1000_WRITE_REG_IO(hw, reg, value)	E1000_WRITE_REG(hw, reg, value)

/* The NVM flash: BAR1 on older PCHs, BAR0 + 0xE000 from Sunrise Point on. */
#define	E1000_READ_FLASH_REG(hw, reg) \
	(*(volatile u32 *)((hw)->flash_address + (reg)))
#define	E1000_READ_FLASH_REG16(hw, reg) \
	(*(volatile u16 *)((hw)->flash_address + (reg)))
#define	E1000_WRITE_FLASH_REG(hw, reg, value) \
	(*(volatile u32 *)((hw)->flash_address + (reg)) = (u32)(value))
#define	E1000_WRITE_FLASH_REG16(hw, reg, value) \
	(*(volatile u16 *)((hw)->flash_address + (reg)) = (u16)(value))

#endif /* _MINKAR_E1000_OSDEP_H_ */
