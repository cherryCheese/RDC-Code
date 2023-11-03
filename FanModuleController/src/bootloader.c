/*
 * bootloader.c
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#ifdef BOOTLOADER

#include <asf.h>
#include <stdio.h>

#include "config.h"
#include "spi_flash.h"
#include "bootloader.h"
#include "uart.h"
#include "upgrade.h"

static void jump_to_firmware(uint32_t addr)
{
	uint32_t stack = *(volatile uint32_t *)addr;
	uint32_t start = *(volatile uint32_t *)(addr + 4);
	
	printf("Starting Firmware @ 0x%08lx (stack pointer @ 0x%08lx)...\r\n", start, stack);
	
	/* Reset peripherals to make sure they are re-initialized properly when the firmware runs */
	uart_reset(CFG_CONSOLE_CHANNEL);
	spi_flash_reset();
	__DSB();
	__ISB();
	/* Re-base the stack pointer */
	__set_MSP(stack);
	/* Re-base the exception vectors */
	SCB->VTOR = addr & SCB_VTOR_TBLOFF_Msk;
	__DSB();
	__ISB();
	/* Jump to the firmware reset vector */
	asm("bx %0"::"r"(start));
	/* NOTREACHED */
}

void bootloader(void)
{
	printf("\r\n\r\nFan Module Controller Boot Loader\r\n");
	spi_flash_init();
	upgrade_copy_to_nvm();
	jump_to_firmware(CFG_FIRMWARE_START);
	/* NOTREACHED */
}

#endif /* BOOTLOADER */