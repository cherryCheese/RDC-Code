/*
 * alarm.c
 *
 * Created: 08.01.2021 15:12:59
 *  Author: E1130513
 */ 
#include <asf.h>

#include "alarm.h"
#include "config.h"
#include "sys_timer.h"
#include "fan.h"
#include "modbus.h"

#ifndef BOOTLOADER

static uint32_t alarm_timer = 0;

/*
 * Set General alarm status dependent on the discret inputs
 */
void do_alarms(void)
{
	if (get_jiffies() - alarm_timer >= 100)
	{
		alarm_timer = get_jiffies();
		
				
		if(((modbus_get_discrete_input(DIS_INPUT__TEMP_SENSOR_BROKEN) == 1) || \
		(modbus_get_discrete_input(DIS_INPUT__HUMIDITY_SENSOR_BROKEN) == 1) || \
		(modbus_get_discrete_input(DIS_INPUT__VOLTAGE_SENSOR_BROKEN) == 1) || \
		(modbus_get_discrete_input(DIS_INPUT__CURRENT_SENSOR_BROKEN) == 1)))
		{
			modbus_set_discrete_input(DIS_INPUT__UNIT_GENERAL_ALARM_STATUS, 1);
		}
		else
		{
			modbus_set_discrete_input(DIS_INPUT__UNIT_GENERAL_ALARM_STATUS, 0);
		}
	}
}
#endif /* BOOTLOADER */