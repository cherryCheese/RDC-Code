/*
 * main.c: bootloader/firmware entry point
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#include <asf.h>

#include "config.h"
#include "fuses.h"
#include "ring_buffer.h"
#include "uart.h"
#include "cli.h"
#include "spi_flash.h"
#include "bootloader.h"
#include "heartbeat.h"
#include "watchdog.h"
#include "sys_timer.h"
#include "eeprom_driver.h"
#include "fan.h"
#include "i2c_local.h"
#include "modbus.h"
#include "alarm.h"
#include "led.h"
#include "env.h"

int main (void)
{
	/* Initialize all modules */
	system_init();
	delay_init();
	ioport_init();
	
	/* Set Modbus RX and Modbus TX to high-Z state. Otherwise the controller stops, if bytes are send while initializing */
	modbus_pin_init();
	
	uart_init();
	
	WDT_DISABLE;
	
#ifdef BOOTLOADER
	bootloader();
#else
	program_fuses();
	
	PRINTF("\r\n\r\n");
	PRINTF("============================================\r\n");
	PRINTF(" Fan Module Controller Firmware, %s%s\r\n", CFG_FIRMWARE_NUMBER, CFG_FIRMWARE_VERSION);
	PRINTF("============================================\r\n");

	enum system_reset_cause reset_cause = system_get_reset_cause();
    PRINTF("Reset cause: %s\r\n",
		reset_cause == SYSTEM_RESET_CAUSE_WDT ? "WDT" :
		reset_cause == SYSTEM_RESET_CAUSE_BOD12 ? "BOD12" :
		reset_cause == SYSTEM_RESET_CAUSE_BOD33 ? "BOD33" :
		reset_cause == SYSTEM_RESET_CAUSE_EXTERNAL_RESET ? "EXT" :
		reset_cause == SYSTEM_RESET_CAUSE_POR ? "POR" :
		reset_cause == SYSTEM_RESET_CAUSE_SOFTWARE ? "SOFT" : "N/A");
	
	eeprom_init();
	env_init();
	modbus_init();
	fan_init();
	i2c_local_init();
	spi_flash_init();
	
	/* Enable global interrupts */
	system_interrupt_enable_global();
	
#ifdef CFG_WDT_TIMEOUT
	wdt_init(CFG_WDT_TIMEOUT);
#endif
	sys_timer_init();

	PRINTF("\r\n");
	
	ioport_set_pin_level(CFG_MODBUS_DE_PIN, IOPORT_PIN_LEVEL_LOW);
	ioport_set_pin_level(CFG_MODBUS_RE_PIN, IOPORT_PIN_LEVEL_LOW);
		
		
	/* Main loop */
	while (1) {
		WDT_RESET;
		do_env();
		do_heartbeat(10000);
		do_fan();
		do_i2c_local();
		do_cli();
		do_modbus();
		do_alarms();
		do_led();
		if(modbus_get_holding_reg(HOLD_REG__SOFTWARE_RESET)>0)
		{
			SYSTEM_RESET;
		}
	}
#endif /* BOOTLOADER */
}
