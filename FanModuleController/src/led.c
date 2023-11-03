/*
 * led.c
 *
 * Created: 10.01.2021 00:39:45
 *  Author: E1130513
 */ 
#include <asf.h>

#include "led.h"
#include "sys_timer.h"
#include "modbus.h"
#include "config.h"

#ifndef BOOTLOADER

static uint32_t last_led;

void do_led(void)
{
	static uint8_t init_done;
	static bool led_toggle_value_slow = 0;
	static bool led_toggle_value_fast = 0;
	static uint8_t toggle_time = 0;
	
	if (!init_done) {
		ioport_set_pin_dir(CFG_LED_GREEN, IOPORT_DIR_OUTPUT);
		ioport_set_pin_level(CFG_LED_GREEN, IOPORT_PIN_LEVEL_HIGH);
		ioport_set_pin_dir(CFG_LED_RED, IOPORT_DIR_OUTPUT);
		ioport_set_pin_level(CFG_LED_RED, IOPORT_PIN_LEVEL_LOW);
		init_done = 1;
	}
	

	if (get_jiffies() - last_led >= 125) {
		last_led = get_jiffies();
		
		if(toggle_time > 3)
		{
			led_toggle_value_slow = !led_toggle_value_slow; //common toggle value for red LED and green LED for synch blinking.
			toggle_time = 0;
		}
		else
		{
			toggle_time++;
		}
		
		led_toggle_value_fast = !led_toggle_value_fast; //common toggle value for red LED and green LED for synch blinking.
		
		switch(modbus_get_holding_reg(HOLD_REG__GREEN_LED))
		{
			case 0: ioport_set_pin_level(CFG_LED_GREEN, IOPORT_PIN_LEVEL_HIGH);
					break;

			case 1: ioport_set_pin_level(CFG_LED_GREEN, led_toggle_value_slow);
					break;
					
			case 2: ioport_set_pin_level(CFG_LED_GREEN, led_toggle_value_fast);
					break;		
			
			case 3:	ioport_set_pin_level(CFG_LED_GREEN, IOPORT_PIN_LEVEL_LOW);
					break;

			default:ioport_set_pin_level(CFG_LED_GREEN, IOPORT_PIN_LEVEL_LOW);
					break;
		}
		
		
		if(modbus_watchdog() == 1)
		{
			ioport_set_pin_level(CFG_LED_RED, led_toggle_value_fast);
		}
		else
		{
			switch(modbus_get_holding_reg(HOLD_REG__RED_LED))
			{
				case 0: ioport_set_pin_level(CFG_LED_RED, IOPORT_PIN_LEVEL_HIGH);
				break;

				case 1: ioport_set_pin_level(CFG_LED_RED, led_toggle_value_slow);
				break;

				case 2: ioport_set_pin_level(CFG_LED_RED, led_toggle_value_fast);
				break;

				case 3:	ioport_set_pin_level(CFG_LED_RED, IOPORT_PIN_LEVEL_LOW);
				break;

				default:ioport_set_pin_level(CFG_LED_RED, IOPORT_PIN_LEVEL_LOW);
				break;
			}
		}
	}
}

#endif /* BOOTLOADER */