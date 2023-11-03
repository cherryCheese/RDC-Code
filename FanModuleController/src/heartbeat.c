/*
 * heartbeat.c
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#include <asf.h>

#include "config.h"
#include "heartbeat.h"
#include "sys_timer.h"
#include "modbus.h"
#include "env.h"

/* Heartbeat: toggle the LED */
void do_heartbeat(uint32_t skip)
{
	static volatile uint32_t cnt;
	static uint8_t init_done;
	
	if (!init_done) {
		ioport_set_pin_dir(CFG_HEARTBEAT_LED, IOPORT_DIR_OUTPUT);
		ioport_set_pin_level(CFG_HEARTBEAT_LED, IOPORT_PIN_LEVEL_LOW);
		init_done = 1;
	}
	
	if ((cnt++ % skip) == 0) {
		ioport_toggle_pin_level(CFG_HEARTBEAT_LED);
	}
}