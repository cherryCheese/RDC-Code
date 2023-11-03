/*
 * sys_time.c
 *
 * Created: 8/16/2020 5:52:18 PM
 *  Author: E1210640
 */ 

#include <asf.h>
#include <stdio.h>

#include "sys_timer.h"
#include "uart.h"

static uint32_t jiffies;

ISR(SysTick_Handler)
{
	jiffies++;
}

void sys_timer_init(void)
{
	SysTick_Config(system_cpu_clock_get_hz() / 1000);
	PRINTF("System timer: %ld Hz\r\n", system_cpu_clock_get_hz());
}

uint32_t get_jiffies(void)
{
	uint32_t tmp;
	
	system_interrupt_enter_critical_section();
	tmp = jiffies;
	system_interrupt_leave_critical_section();
	
	return tmp;
}