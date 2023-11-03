/*
 * watchdog.c: watchdog timer driver
 *
 * Created: 8/16/2020 6:01:02 PM
 *  Author: E1210640
 */ 

/*
 * NOTE: this driver does not use ASF
 * since the ASF WDT implementation is slow.
 */

#include <asf.h>

#include "watchdog.h"
#include "sys_timer.h"
#include "eeprom.h"
#include "uart.h"

#if defined(CFG_WDT_TIMEOUT) && !defined(BOOTLOADER)

void wdt_init(uint32_t timeout_sec)
{
	uint8_t p;
	
	wdt_disable();
	
	/*
	 * To allow watchdog timeouts longer than 0.5sec, we need to
	 * enable a /32 divisor for the low-power 32KHz oscillator.
	 * This yields a 1024 Hz WDT clock.
	 */
	GCLK->GENDIV.reg = GCLK_GENDIV_ID(2) | GCLK_GENDIV_DIV(4);
	GCLK->GENCTRL.reg = GCLK_GENCTRL_ID(2) | GCLK_GENCTRL_GENEN | GCLK_GENCTRL_SRC_OSCULP32K | GCLK_GENCTRL_DIVSEL;
	while (GCLK->STATUS.bit.SYNCBUSY)
		;
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_WDT | GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2;
	
	/* Find a close match for the timeout configuration */
	for (p = 0; p < 0xb; p++) {
		if (timeout_sec*1000 < (uint32_t)(8 << p)*1000/1024) {
			break;
		}
	}
	WDT->CONFIG.reg = p;
	while (WDT->STATUS.bit.SYNCBUSY)
		;
	/* Enable early warning interrupt */
	WDT->EWCTRL.reg = p - 1;
	WDT->INTENSET.reg = WDT_INTENSET_EW;
	system_interrupt_enable(SYSTEM_INTERRUPT_MODULE_WDT);
	NVIC_EnableIRQ(SYSTEM_INTERRUPT_MODULE_WDT);
	/* Enable watchdog */
	WDT->CTRL.reg = 0x02;
	while (WDT->STATUS.bit.SYNCBUSY)
		;
}

ISR(WDT_Handler)
{
	eeprom_emulator_commit_page_buffer();
	uart_puts(CFG_CONSOLE_CHANNEL, "WDT: EARLY WARNING HANDLER CALLED!\r\n");
	WDT->INTFLAG.reg = WDT_INTFLAG_EW;
}

void wdt_reset(void)
{
	/* Avoid writing to the WDT if clock syncing is in progress */
	if (!WDT->STATUS.bit.SYNCBUSY) {
		WDT->CLEAR.reg = WDT_CLEAR_CLEAR_KEY;
	}
}

#endif /* CFG_WDT_TIMEOUT && !BOOTLOADER */

void wdt_disable(void)
{
	WDT->CTRL.reg = 0;
	while (WDT->STATUS.bit.SYNCBUSY)
		;
}