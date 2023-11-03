/*
 * watchdog.h
 *
 * Created: 8/16/2020 1:51:18 PM
 *  Author: E1210640
 */ 

/*
 * NOTE: this driver does not use ASF
 * since the ASF WDT implementation is painfully slow.
 */

#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include "config.h"
#include "watchdog.h"
#include "eeprom.h"

void wdt_disable(void);

#if defined(CFG_WDT_TIMEOUT) && !defined(BOOTLOADER)

void wdt_init(uint32_t timeout_sec);
void wdt_reset(void);

#define WDT_ENABLE \
	wdt_init(CFG_WDT_TIMEOUT)

#define WDT_RESET \
	wdt_reset()

#else /* CFG_WDT_TIMEOUT && !BOOTLOADER */

#define WDT_ENABLE \
	/* DO NOTHING */
	
#define WDT_RESET \
	/* DO NOTHING */

#endif

#define SYSTEM_RESET \
	do { \
		eeprom_emulator_commit_page_buffer(); \
		while (1) { \
			system_reset(); \
		} \
	} while (0)

#define WDT_DISABLE \
	wdt_disable()

#endif /* __WATCHDOG_H__ */