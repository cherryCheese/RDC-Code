/*
 * eeprom.c
 *
 * Created: 12/6/2020 11:05:07 AM
 *  Author: E1210640
 */ 

#include <asf.h>

#include "config.h"
#include "eeprom_driver.h"
#include "uart.h"

#if defined(CFG_EEPROM_ENABLE) && !defined(BOOTLOADER)

static uint8_t eeprom_valid;

void SYSCTRL_Handler(void)
{
	if (SYSCTRL->INTFLAG.reg & SYSCTRL_INTFLAG_BOD33DET) {
		SYSCTRL->INTFLAG.reg |= SYSCTRL_INTFLAG_BOD33DET;
		eeprom_emulator_commit_page_buffer();
	}
}

static void configure_bod(void)
{
	struct bod_config config_bod33;
	bod_get_config_defaults(&config_bod33);
	config_bod33.action = BOD_ACTION_INTERRUPT;
	config_bod33.level = CFG_EEPROM_BOD33_LEVEL;
	bod_set_config(BOD_BOD33, &config_bod33);
	bod_enable(BOD_BOD33);
	
	SYSCTRL->INTENSET.reg |= SYSCTRL_INTENCLR_BOD33DET;
	system_interrupt_enable(SYSTEM_INTERRUPT_MODULE_SYSCTRL);
}

void eeprom_init(void)
{
	enum status_code error_code = eeprom_emulator_init();
	if (error_code == STATUS_ERR_NO_MEMORY) {
		PRINTF("ERROR: no EEPROM section has been set in device fuses, disabling EEPROM\r\n");
		return;
	} else if (error_code != STATUS_OK) {
		PRINTF("ERROR: EEPROM emulator init failed, re-formatting...\r\n");
		eeprom_emulator_erase_memory();
		eeprom_emulator_init();
	}
	eeprom_valid = 1;
	configure_bod();
	PRINTF("EEPROM initialized\r\n");
}

int eeprom_read(uint8_t *buf, int offset, int len)
{
	if (!eeprom_valid) {
		return -1;
	}
	if (eeprom_emulator_read_buffer(offset, buf, len) != STATUS_OK) {
		return -1;
	}
	
	return len;
}

int eeprom_write(const uint8_t *buf, int offset, int len)
{
	if (!eeprom_valid) {
		return -1;
	}
	if (eeprom_emulator_write_buffer(offset, buf, len) != STATUS_OK) {
		return -1;
	}
	if (offset < CFG_EEPROM_HOLDING_OFFSET || offset >= CFG_EEPROM_HOLDING_OFFSET + 5*EEPROM_PAGE_SIZE) {
		/* If writing outside of the holding area, commit page buffer immediately */
		eeprom_emulator_commit_page_buffer();
	}
	
	return 0;
}

#endif /* BOOTLOADER */