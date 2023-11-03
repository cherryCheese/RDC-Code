/*
 * modbus.c
 *
 * Created: 11/9/2020 3:02:37 PM
 *  Author: E1210640
 */ 

#include <asf.h>

#include "config.h"
#include "uart.h"
#include "crc.h"
#include "modbus.h"
#include "eeprom_driver.h"
#include "upgrade.h"
#include "sys_timer.h"
#include "env.h"


#ifndef BOOTLOADER

#define MODBUS_FUNC_READ_DISCRETE_INPUTS	2
#define MODBUS_FUNC_READ_HOLDING_REGS		3
#define MODBUS_FUNC_READ_INPUT_REGS			4
#define MODBUS_FUNC_WRITE_SINGLE_REG		6
#define MODBUS_FUNC_WRITE_MULTIPLE_REGS		16
#define MODBUS_FUNC_RW_MULTIPLE_REGS		23
#define MODBUS_FUNC_MASK_WRITE_REG			22

#define MODBUS_UPGRADE_DATA_ADDRESS			0x1000

/* Upgrade function codes */
#define MODBUS_UPGRADE_FUNCTION_PREPARE		0x55AA	/* Prepare for upgrade (erase Flash) */
#define MODBUS_UPGRADE_FUNCTION_VERIFY		0x5A5A	/* Verify firmware */
#define MODBUS_UPGRADE_FUNCTION_ACTIVATE	0xAA55	/* Activate the new firmware */

/* Upgrade status codes */
#define MODBUS_UPGRADE_STATUS_NO_UPGRADE	0		/* Upgrade has not started */
#define MODBUS_UPGRADE_STATUS_IN_PROGRESS	1		/* Upgrade is in progress */
#define MODBUS_UPGRADE_STATUS_VERIFIED		2		/* Firmware verified and ready for activation */
#define MODBUS_UPGRADE_STATUS_ERROR			3		/* Verification error */

#define MODBUS_EX_INVALID_FUNCTION			1
#define MODBUS_EX_INVALID_ADDRESS			2
#define MODBUS_EX_INVALID_DATA				3
#define MODBUS_EX_DEVICE_FAILURE			4

static uint8_t slave_address;
static uint8_t rtu_buf[256];
static uint8_t rtu_ptr;

static uint8_t discrete_inputs[(CFG_MODBUS_DISCRETE_INPUTS + 7)/8];
static uint16_t input_regs[CFG_MODBUS_INPUT_REGS];
static uint16_t holding_regs[CFG_MODBUS_HOLDING_REGS];
static uint8_t frame_len;
static uint8_t modbus_watchdog_triggered;
static uint32_t last_modbus_watchdog_period = 0;

static struct tc_module tc_instance;
static int baud_rate;
static uint32_t operating_minutes;
static uint32_t last_1_minute;

/*
 * MODBUS uses a reverse CRC algorithm,
 * so we cannot use the generic crc16() function here.
 */
static uint16_t modbus_crc16(const uint8_t *buf, uint32_t len)
{
	int i;
	uint8_t c, flag;
	uint16_t crc = 0xFFFF;
	
	while (len--) {
		c = *buf++;
		crc ^= c;
		for (i = 0; i < 8; i++) {
			flag = (crc & 1);
			crc >>= 1;
			if (flag) {
				crc ^= 0xA001;
			}
		}
	}

	return crc;
}

/*
 * The following callback will be called after a silent period
 * (no characters received for 3.5 character lengths).
 */
static void modbus_tc_callback(struct tc_module *const module_inst)
{
	/* Clear the status and stop the timer */
	tc_stop_counter(&tc_instance);
	if (rtu_ptr) {
		/* Check if a valid frame has been received and the previous frame has been processed (frame_len == 0) */
		if ((!*rtu_buf || *rtu_buf == slave_address) && rtu_ptr >= 5 && !frame_len) {
			frame_len = rtu_ptr;
		}
		rtu_ptr = 0;
	}
}

static int modbus_configure_timeout(void)
{
	uint32_t silent_timeout_us;
	/*
	 * As per the MODBUS spec, the silent timeout is 3.5 characters.
	 * Assuming 11 bits per character, this gives a timeout of 11*3.5/baudrate sec,
	 * or 38500000/baudrate us.  Also, according to the spec, the minimum value
	 * of the timeout should be 1.75 ms, regardless of the baud rate.
	 */
	silent_timeout_us = 38500000/baud_rate;
	if (silent_timeout_us < 1750) {
		silent_timeout_us = 1750;
	}
	
	/* The compare value is based on the clock generator frequency of 8 MHz */
	return tc_set_compare_value(&tc_instance, TC_COMPARE_CAPTURE_CHANNEL_0, 8*silent_timeout_us);
}

int modbus_init(void)
{
	uint8_t eeprom_data[(5*EEPROM_PAGE_SIZE) - 4];		/* -4 to avoid overwriting the operating hours data at the end of the page */
	enum system_reset_cause reset_cause;
	int i;

	baud_rate = env_get("modbus_baud_rate");
	uart_set_baud_rate(CFG_MODBUS_CHANNEL, baud_rate);
	slave_address = CFG_MODBUS_SLAVE_ADDRESS + (!ioport_get_pin_level(CFG_MODBUS_ADDRESS_4)<<3) + (!ioport_get_pin_level(CFG_MODBUS_ADDRESS_3)<<2) + (!ioport_get_pin_level(CFG_MODBUS_ADDRESS_2)<<1) + !ioport_get_pin_level(CFG_MODBUS_ADDRESS_1);
	PRINTF("MODBUS slave address: %d\r\n", slave_address);
	PRINTF("MODBUS baud rate: %d\r\n", baud_rate);

		
	struct tc_config config;
	tc_get_config_defaults(&config);
	config.counter_size = TC_COUNTER_SIZE_16BIT;
	config.clock_source = GCLK_GENERATOR_0;				/* 8 MHz */
	config.clock_prescaler = TC_CLOCK_PRESCALER_DIV1;
	config.counter_16_bit.value = 0;
	
	tc_init(&tc_instance, CFG_MODBUS_TC_MODULE, &config);
	tc_enable(&tc_instance);
	tc_register_callback(&tc_instance, modbus_tc_callback, TC_CALLBACK_CC_CHANNEL0);
	tc_enable_callback(&tc_instance, TC_CALLBACK_CC_CHANNEL0);
	modbus_configure_timeout();
	
	/* Initialize part/device numbers */
	eeprom_read(eeprom_data, CFG_EEPROM_PN_OFFSET, sizeof(eeprom_data));
	for (i = 0; i < 46; i++) {
		input_regs[i] = (eeprom_data[i*2] << 8) | eeprom_data[i*2+1];
	}
	
	input_regs[INPUT_REG__DEVICE_PN_7_6] = input_regs[0];
	input_regs[INPUT_REG__DEVICE_PN_5_4] = input_regs[1];
	input_regs[INPUT_REG__DEVICE_PN_3_2] = input_regs[2];
	input_regs[INPUT_REG__DEVICE_PN_1_0] = input_regs[3];
	
	input_regs[INPUT_REG__DEVICE_SN_11_10] = input_regs[4];
	input_regs[INPUT_REG__DEVICE_SN_9_8] = input_regs[5];
	input_regs[INPUT_REG__DEVICE_SN_7_6] = input_regs[6];
	input_regs[INPUT_REG__DEVICE_SN_5_4] = input_regs[7];
	input_regs[INPUT_REG__DEVICE_SN_3_2] = input_regs[8];
	input_regs[INPUT_REG__DEVICE_SN_1_0] = input_regs[9];
	
	input_regs[INPUT_REG__CONTROLLER_PN_7_6] = input_regs[10];
	input_regs[INPUT_REG__CONTROLLER_PN_5_4] = input_regs[11];
	input_regs[INPUT_REG__CONTROLLER_PN_3_2] = input_regs[12];
	input_regs[INPUT_REG__CONTROLLER_PN_1_0] = input_regs[13];
	
	input_regs[INPUT_REG__CONTROLLER_SN_11_10] = input_regs[14];
	input_regs[INPUT_REG__CONTROLLER_SN_9_8] = input_regs[15];
	input_regs[INPUT_REG__CONTROLLER_SN_7_6] = input_regs[16];
	input_regs[INPUT_REG__CONTROLLER_SN_5_4] = input_regs[17];
	input_regs[INPUT_REG__CONTROLLER_SN_3_2] = input_regs[18];
	input_regs[INPUT_REG__CONTROLLER_SN_1_0] = input_regs[19];
	
	char fw_number[8] = CFG_FIRMWARE_NUMBER;
	input_regs[INPUT_REG__FIRMWARE_PN_7_6] = (fw_number[0]<<8) | fw_number[1];
	input_regs[INPUT_REG__FIRMWARE_PN_5_4] = (fw_number[2]<<8) | fw_number[3];
	input_regs[INPUT_REG__FIRMWARE_PN_3_2] = (fw_number[4]<<8) | fw_number[5];
	input_regs[INPUT_REG__FIRMWARE_PN_1_0] = (fw_number[6]<<8) | fw_number[7];
	
	char fw_version[2] = CFG_FIRMWARE_VERSION;
	input_regs[INPUT_REG__FIRMWARE_REV_1_0] = (fw_version[0]<<8) | fw_version[1];
	
	char manufacturer[6] = CFG_MANUFACTURER;
	input_regs[INPUT_REG__MANUFACTURER_5_4] = (manufacturer[0]<<8) | manufacturer[1];
	input_regs[INPUT_REG__MANUFACTURER_3_2] = (manufacturer[2]<<8) | manufacturer[3];
	input_regs[INPUT_REG__MANUFACTURER_1_0] = (manufacturer[4]<<8) | manufacturer[5];
	
	char device_name[10] = CFG_DEVICE_NAME;
	input_regs[INPUT_REG__DEVICE_NAME_9_8] = (device_name[0]<<8) | device_name[1];
	input_regs[INPUT_REG__DEVICE_NAME_7_6] = (device_name[2]<<8) | device_name[3];
	input_regs[INPUT_REG__DEVICE_NAME_5_4] = (device_name[4]<<8) | device_name[5];
	input_regs[INPUT_REG__DEVICE_NAME_3_2] = (device_name[6]<<8) | device_name[7];
	input_regs[INPUT_REG__DEVICE_NAME_1_0] = (device_name[8]<<8) | device_name[9];
	
	input_regs[INPUT_REG__FAN_CURRENT_PWM] = 0;
	input_regs[INPUT_REG__TEMP_SENSOR] = 0;
	input_regs[INPUT_REG__HUMIDITY_SENSOR] = 0;
	input_regs[INPUT_REG__FAN_CURRENT_SPEED] = 0;
	input_regs[INPUT_REG__VOLTAGE_SENSOR_SPEED_3_2] = 0;
	input_regs[INPUT_REG__VOLTAGE_SENSOR_SPEED_1_0] = 0;
	input_regs[INPUT_REG__CURRENT_SENSOR] = 0;
	input_regs[INPUT_REG__RPM_DEVIATION_1_0] = 0;
	
	eeprom_read(eeprom_data, CFG_EEPROM_HOLDING_OFFSET, sizeof(eeprom_data));
	
	reset_cause = system_get_reset_cause();
	if (reset_cause == SYSTEM_RESET_CAUSE_WDT || reset_cause == SYSTEM_RESET_CAUSE_SOFTWARE) {
		/* Soft/WDT reset: restore from EEPROM */
		PRINTF("MODBUS: restoring holding registers\r\n");
		for (i = 0; i < CFG_MODBUS_HOLDING_REGS; i++) {
			holding_regs[i] = (eeprom_data[i*2] << 8) | eeprom_data[i*2 + 1];
		}
		holding_regs[HOLD_REG__SOFTWARE_RESET] = CFG_MODBUS_HLD_SOFTWARE_RESET;
	} else {
		/* Power-on: initialize to default values */
		for (i = 0; i < CFG_MODBUS_HOLDING_REGS; i++) {
			holding_regs[i] = (eeprom_data[i*2] << 8) | eeprom_data[i*2 + 1];
		}
		if(env_get("first_start_done") == 0) 
		{		
				holding_regs[HOLD_REG__UNIT_OFF_ON] = CFG_MODBUS_HLD_UNIT_ON_OFF;
				holding_regs[HOLD_REG__PRECONFIG_FAN_REQUEST] = CFG_MODBUS_HLD_PRECONFIG_FAN_REQUEST;
				holding_regs[HOLD_REG__GREEN_LED] = CFG_MODBUS_HLD_GREEN_LED;
				holding_regs[HOLD_REG__RED_LED] = CFG_MODBUS_HLD_RED_LED;
				holding_regs[HOLD_REG__FAN_REQUEST] = holding_regs[HOLD_REG__PRECONFIG_FAN_REQUEST];
				holding_regs[HOLD_REG__FAN_REUEST_MIN] = CFG_MODBUS_HLD_FAN_REUEST_MIN;
				holding_regs[HOLD_REG__FAN_REUEST_MAX] = CFG_MODBUS_HLD_FAN_REUEST_MAX;
				holding_regs[HOLD_REG__PWM_FREQUENCY] = CFG_MODBUS_HLD_PWM_FREQUENCY;
				holding_regs[HOLD_REG__PWM_DELAY] = CFG_MODBUS_HLD_PWM_DELAY;
				holding_regs[HOLD_REG__PULSES_PER_REVOLUTION] = CFG_MODBUS_HLD_PULSES_PER_REVOLUTION;
				holding_regs[HOLD_REG__MODBUS_DEAD_TIME] = CFG_MODBUS_HLD_MODBUS_DEAD_TIME;
				holding_regs[HOLD_REG__SOFTWARE_RESET] = CFG_MODBUS_HLD_SOFTWARE_RESET;
				holding_regs[HOLD_REG__UPGRADE_FUNCTION] = CFG_MODBUS_HLD_UPGRADE_FUNCTION;
				env_set("first_start_done", 1);
				PRINTF("MODBUS: initialized to default values first start\r\n");
		}
		else
		{
				holding_regs[HOLD_REG__UNIT_OFF_ON] = CFG_MODBUS_HLD_UNIT_ON_OFF;
				holding_regs[HOLD_REG__GREEN_LED] = CFG_MODBUS_HLD_GREEN_LED;
				holding_regs[HOLD_REG__RED_LED] = CFG_MODBUS_HLD_RED_LED;
				holding_regs[HOLD_REG__FAN_REQUEST] = holding_regs[HOLD_REG__PRECONFIG_FAN_REQUEST];
				holding_regs[HOLD_REG__SOFTWARE_RESET] = CFG_MODBUS_HLD_SOFTWARE_RESET;
				holding_regs[HOLD_REG__UPGRADE_FUNCTION] = CFG_MODBUS_HLD_UPGRADE_FUNCTION;
				PRINTF("MODBUS: restoring holding registers\r\n");
				PRINTF("MODBUS: initialized to default values\r\n");
		}

		for (i = 0; i < CFG_MODBUS_HOLDING_REGS; i++) {
			eeprom_data[i*2] = (holding_regs[i] >> 8) & 0xFF;
			eeprom_data[i*2 + 1] = holding_regs[i] & 0xFF;
		}
		eeprom_write(eeprom_data, CFG_EEPROM_HOLDING_OFFSET, sizeof(eeprom_data));
	}
	/* Initialize operating hours */
	eeprom_read(eeprom_data, CFG_EEPROM_HOLDING_OFFSET + 5* EEPROM_PAGE_SIZE - 4, 4);
	operating_minutes = (eeprom_data[0] << 24) | (eeprom_data[1] << 16) | (eeprom_data[2] << 8) | eeprom_data[3];
	if (operating_minutes == 0xFFFFFFFF) {
		operating_minutes = 0;
	}
	input_regs[INPUT_REG__OPERATING_HOURS_3_2] = ((operating_minutes/60) >> 16) & 0xFFFF;
	input_regs[INPUT_REG__OPERATING_HOURS_1_0] = (operating_minutes/60) & 0xFFFF;

	
	/* Initialize upgrade registers */
	input_regs[INPUT_REG__UPGRADE_STATUS] = MODBUS_UPGRADE_STATUS_NO_UPGRADE;
	holding_regs[HOLD_REG__UPGRADE_FUNCTION] = 0;
	
	modbus_watchdog_triggered = 0;
	
	return 0;
}

uint8_t modbus_get_discrete_input(uint16_t nr)
{
	uint8_t ret;
	
	system_interrupt_enter_critical_section();
	ret = (discrete_inputs[nr/8] >> (nr & 7)) & 1;
	system_interrupt_leave_critical_section();
	
	return ret;
}

void modbus_set_discrete_input(uint16_t nr, uint8_t val)
{
	system_interrupt_enter_critical_section();
	if (val) {
		discrete_inputs[nr/8] |= (1 << (nr & 7));
	} else {
		discrete_inputs[nr/8] &= ~(1 << (nr & 7));	
	}
	system_interrupt_leave_critical_section();
}

uint16_t modbus_get_input_reg(uint16_t nr)
{
	uint16_t ret;
	
	system_interrupt_enter_critical_section();
	ret = input_regs[nr];
	system_interrupt_leave_critical_section();
	
	return ret;
}

void modbus_set_input_reg(uint16_t nr, uint16_t val)
{
	system_interrupt_enter_critical_section();
	input_regs[nr] = val;
	system_interrupt_leave_critical_section();
}

uint16_t modbus_get_holding_reg(uint16_t nr)
{
	uint16_t ret;

	system_interrupt_enter_critical_section();
	ret = holding_regs[nr];
	system_interrupt_leave_critical_section();
	
	return ret;
}

void modbus_set_holding_reg(uint16_t nr, uint16_t val)
{
	uint8_t tmp[2];
	
	system_interrupt_enter_critical_section();
	if (holding_regs[nr] != val) {
		/* Save to the EEPROM holding area (cached; will only be committed before a WDT or soft reset) */
		tmp[0] = (val >> 8) & 0xFF;
		tmp[1] = val & 0xFF;
		eeprom_write(tmp, CFG_EEPROM_HOLDING_OFFSET + nr*2, 2);
		holding_regs[nr] = val;
	}
	system_interrupt_leave_critical_section();
}

static void modbus_send_response(uint8_t len)
{
	uint16_t cksum;
	
	cksum = modbus_crc16((const uint8_t *)rtu_buf, len + 2);
	rtu_buf[len + 3] = cksum >> 8;
	rtu_buf[len + 2] = cksum & 0xff;
	/* Enable the driver and disable the receiver */
	ioport_set_pin_level(CFG_MODBUS_DE_PIN, IOPORT_PIN_LEVEL_HIGH);
	ioport_set_pin_level(CFG_MODBUS_RE_PIN, IOPORT_PIN_LEVEL_HIGH);
	uart_write(CFG_MODBUS_CHANNEL, (const uint8_t *)rtu_buf, len + 4);
	/* Disable the driver and enable the receiver */
	ioport_set_pin_level(CFG_MODBUS_DE_PIN, IOPORT_PIN_LEVEL_LOW);
	ioport_set_pin_level(CFG_MODBUS_RE_PIN, IOPORT_PIN_LEVEL_LOW);
}

static void modbus_parse_frame(void)
{
	uint16_t cksum_calc, cksum_frame;
	uint16_t read_addr, read_qty, write_addr, write_qty, val, and_mask, or_mask, i;
	uint8_t resp_len = 0, exception = 0;

	cksum_calc = modbus_crc16((const uint8_t *)rtu_buf, frame_len - 2);
	cksum_frame = (rtu_buf[frame_len - 1] << 8) | rtu_buf[frame_len - 2];
		
	if (cksum_calc != cksum_frame) {
		/* Invalid checksum: discard */
		PRINTF("MODBUS request: invalid checksum, dropping frame\r\n");
		return;
	}
	
	modbus_watchdog_triggered = 0;
	last_modbus_watchdog_period = get_jiffies(); //Reset Modbus Watchdog
	
	switch (rtu_buf[1]) {
		case MODBUS_FUNC_READ_DISCRETE_INPUTS:
			read_addr = (rtu_buf[2] << 8) | rtu_buf[3];
			read_qty = (rtu_buf[4] << 8) | rtu_buf[5];
			if (read_addr >= CFG_MODBUS_DISCRETE_INPUTS) {
				exception = MODBUS_EX_INVALID_ADDRESS;
			} else if (!read_qty || read_addr + read_qty > CFG_MODBUS_DISCRETE_INPUTS) {
				exception = MODBUS_EX_INVALID_DATA;
			} else {
				rtu_buf[2] = (read_qty + 7)/8;
				for (i = 0; i < read_qty; i++) {
					if ((i & 7) == 0) {
						rtu_buf[3 + i/8] = 0;
					}
					rtu_buf[3 + i/8] |= modbus_get_discrete_input(read_addr + i) << (i & 7);
				}
				resp_len = rtu_buf[2] + 1;
			}
			break;
		case MODBUS_FUNC_READ_INPUT_REGS:
			read_addr = (rtu_buf[2] << 8) | rtu_buf[3];
			read_qty = (rtu_buf[4] << 8) | rtu_buf[5];
			if (read_addr >= CFG_MODBUS_INPUT_REGS) {
				exception = MODBUS_EX_INVALID_ADDRESS;
			} else if (!read_qty || read_addr + read_qty > CFG_MODBUS_INPUT_REGS) {
				exception = MODBUS_EX_INVALID_DATA;
			} else {
				rtu_buf[2] = read_qty*2;
				for (i = 0; i < read_qty; i++) {
					val = modbus_get_input_reg(read_addr + i);
					rtu_buf[3 + i*2] = val >> 8;
					rtu_buf[4 + i*2] = val & 0xff;
				}
				resp_len = read_qty*2 + 1;
			}
			break;
		case MODBUS_FUNC_READ_HOLDING_REGS:
			read_addr = (rtu_buf[2] << 8) | rtu_buf[3];
			read_qty = (rtu_buf[4] << 8) | rtu_buf[5];
			if (read_addr >= CFG_MODBUS_HOLDING_REGS) {
				exception = MODBUS_EX_INVALID_ADDRESS;
			} else if (!read_qty || read_addr + read_qty > CFG_MODBUS_HOLDING_REGS) {
				exception = MODBUS_EX_INVALID_DATA;
			} else {
				rtu_buf[2] = read_qty*2;
				for (i = 0; i < read_qty; i++) {
					val = modbus_get_holding_reg(read_addr + i);
					rtu_buf[3 + i*2] = val >> 8;
					rtu_buf[4 + i*2] = val & 0xff;
				}
				resp_len = read_qty*2 + 1;
			}
			break;
		case MODBUS_FUNC_WRITE_SINGLE_REG:
			write_addr = (rtu_buf[2] << 8) | rtu_buf[3];
			val = (rtu_buf[4] << 8) | rtu_buf[5];
			if (write_addr >= CFG_MODBUS_HOLDING_REGS) {
				exception = MODBUS_EX_INVALID_ADDRESS;
			} else {
				modbus_set_holding_reg(write_addr, val);
				resp_len = 4;
			}
			break;
		case MODBUS_FUNC_WRITE_MULTIPLE_REGS:
			write_addr = (rtu_buf[2] << 8) | rtu_buf[3];
			write_qty = (rtu_buf[4] << 8) | rtu_buf[5];
			if (write_addr >= MODBUS_UPGRADE_DATA_ADDRESS) {
				if (modbus_get_input_reg(INPUT_REG__UPGRADE_STATUS) != MODBUS_UPGRADE_STATUS_IN_PROGRESS) {
					exception = MODBUS_EX_INVALID_DATA;
				} else if (upgrade_write_data((write_addr - MODBUS_UPGRADE_DATA_ADDRESS)*2, rtu_buf + 7, write_qty*2) < 0) {
					PRINTF("MODBUS: upgrade_write_data failed\r\n");
					exception = MODBUS_EX_DEVICE_FAILURE;
				} else {
					resp_len = 4;
				}
			} else if (write_addr >= CFG_MODBUS_HOLDING_REGS) {
				exception = MODBUS_EX_INVALID_ADDRESS;
			} else if (!write_qty || write_addr + write_qty > CFG_MODBUS_HOLDING_REGS || rtu_buf[6] != 2*write_qty) {
				exception = MODBUS_EX_INVALID_DATA;
			} else {
				for (i = 0; i < write_qty; i++) {
					val = (rtu_buf[7 + i*2] << 8) | rtu_buf[8 + i*2];
					modbus_set_holding_reg(write_addr + i, val);
				}
				resp_len = 4;
			}
			break;
		case MODBUS_FUNC_RW_MULTIPLE_REGS:
			read_addr = (rtu_buf[2] << 8) | rtu_buf[3];
			read_qty = (rtu_buf[4] << 8) | rtu_buf[5];
			write_addr = (rtu_buf[6] << 8) | rtu_buf[7];
			write_qty = (rtu_buf[8] << 8) | rtu_buf[9];
			if (read_addr >= CFG_MODBUS_HOLDING_REGS || write_addr >= CFG_MODBUS_HOLDING_REGS) {
				exception = MODBUS_EX_INVALID_ADDRESS;
			} else if (!read_qty || read_addr + read_qty > CFG_MODBUS_HOLDING_REGS
					|| rtu_buf[10] != 2*write_qty || !write_qty || write_addr + write_qty > CFG_MODBUS_HOLDING_REGS) {
				exception = MODBUS_EX_INVALID_DATA;
			} else {
				for (i = 0; i < write_qty; i++) {
					val = (rtu_buf[11 + i*2] << 8) | rtu_buf[12 + i*2];
					modbus_set_holding_reg(write_addr + i, val);
				}
				rtu_buf[2] = read_qty*2;
				for (i = 0; i < read_qty; i++) {
					val = modbus_get_holding_reg(read_addr + i);
					rtu_buf[3 + i*2] = val >> 8;
					rtu_buf[4 + i*2] = val & 0xff;
				}
				resp_len = read_qty*2 + 1;
			}
			break;
		case MODBUS_FUNC_MASK_WRITE_REG:
			write_addr = (rtu_buf[2] << 8) | rtu_buf[3];
			and_mask = (rtu_buf[4] << 8) | rtu_buf[5];
			or_mask = (rtu_buf[6] << 8) | rtu_buf[7];
			if (write_addr >= CFG_MODBUS_HOLDING_REGS) {
				exception = MODBUS_EX_INVALID_ADDRESS;
			} else {
				val = modbus_get_holding_reg(write_addr);
				val = (val & and_mask) | (or_mask & ~and_mask);
				modbus_set_holding_reg(write_addr, val);
				resp_len = 6;
			}
			break;
		default:
			exception = MODBUS_EX_INVALID_FUNCTION;
			break;
	}
	if (exception) {
		rtu_buf[1] |= 0x80;
		rtu_buf[2] = exception;
		resp_len = 1;
	}
	modbus_send_response(resp_len);
}

void modbus_receive(uint8_t ch)
{
	/*
	 * A new character has been received: store it in the buffer
	 * (if the last frame has been processed) and re-start the
	 * idle timer.
	 */
	if (rtu_ptr < sizeof(rtu_buf) && !frame_len) {
		rtu_buf[rtu_ptr++] = ch;
	}
	tc_start_counter(&tc_instance);
}

uint8_t modbus_watchdog (void)
{
	return modbus_watchdog_triggered;
}

static void update_operating_hours(void)
{
	uint8_t tmp[4];
	
	if (get_jiffies() - last_1_minute >= 60000) {
		last_1_minute = get_jiffies();
		operating_minutes++;
		tmp[0] = (operating_minutes >> 24) & 0xFF;
		tmp[1] = (operating_minutes >> 16) & 0xFF;
		tmp[2] = (operating_minutes >> 8) & 0xFF;
		tmp[3] = operating_minutes & 0xFF;
		
		/*
		 * Write the updated operating minutes to the EEPROM:
		 * note that the EEPROM page will NOT be committed right away, but only
		 * during a power-down or reset.
		 */
		eeprom_write(tmp, CFG_EEPROM_HOLDING_OFFSET + 5*EEPROM_PAGE_SIZE - 4, 4);
		/* Update input registers */
		modbus_set_input_reg(INPUT_REG__OPERATING_HOURS_3_2, ((operating_minutes/60) >> 16) & 0xFFFF);
		modbus_set_input_reg(INPUT_REG__OPERATING_HOURS_1_0, (operating_minutes/60) & 0xFFFF);
	}
}

/* MODBUS processing (main loop callback) */
void do_modbus(void)
{
	uint8_t len;
	int new_baud_rate, new_slave_address; 

	update_operating_hours();
	
	new_slave_address = env_get("modbus_slave_addr") + (!ioport_get_pin_level(CFG_MODBUS_ADDRESS_4)<<3) + (!ioport_get_pin_level(CFG_MODBUS_ADDRESS_3)<<2) + (!ioport_get_pin_level(CFG_MODBUS_ADDRESS_2)<<1) + !ioport_get_pin_level(CFG_MODBUS_ADDRESS_1);
	if (new_slave_address != slave_address) {
		PRINTF("MODBUS slave address: %d\r\n", new_slave_address);
		slave_address = new_slave_address;
	}
		
	if (get_jiffies() - last_modbus_watchdog_period >= (1000*(1+ modbus_get_holding_reg(HOLD_REG__MODBUS_DEAD_TIME)))) {
		last_modbus_watchdog_period = get_jiffies();
		modbus_watchdog_triggered = 1;
	}
	
	new_baud_rate = env_get("modbus_baud_rate");
	if (new_baud_rate != baud_rate) {
		PRINTF("MODBUS: changing baud rate to %d\r\n", new_baud_rate);
		baud_rate = new_baud_rate;
		uart_set_baud_rate(CFG_MODBUS_CHANNEL, baud_rate);
		modbus_configure_timeout();
	}
	
	system_interrupt_enter_critical_section();
	len = frame_len;
	system_interrupt_leave_critical_section();
	
	if (len) {
		modbus_parse_frame();
		system_interrupt_enter_critical_section();
		/* Allow further input */
		frame_len = 0;
		system_interrupt_leave_critical_section();
	}
	
	if(env_get("disable_update_ability") == 0)
	{	
		if (modbus_get_holding_reg(HOLD_REG__UPGRADE_FUNCTION) == MODBUS_UPGRADE_FUNCTION_PREPARE) {
			PRINTF("MODBUS: starting upgrade\r\n");
			if (upgrade_start() < 0) {
				modbus_set_input_reg(INPUT_REG__UPGRADE_STATUS, MODBUS_UPGRADE_STATUS_ERROR);
			} else {
				modbus_set_input_reg(INPUT_REG__UPGRADE_STATUS, MODBUS_UPGRADE_STATUS_IN_PROGRESS);
			}
		} else if (modbus_get_holding_reg(HOLD_REG__UPGRADE_FUNCTION) == MODBUS_UPGRADE_FUNCTION_VERIFY) {
			if (modbus_get_input_reg(INPUT_REG__UPGRADE_STATUS) == MODBUS_UPGRADE_STATUS_IN_PROGRESS) {
				PRINTF("MODBUS: verifying firmware\r\n");
				if (upgrade_verify() < 0) {
					PRINTF("MODBUS: firmware verification failed\r\n");
					modbus_set_input_reg(INPUT_REG__UPGRADE_STATUS, MODBUS_UPGRADE_STATUS_ERROR);
				} else {
					PRINTF("MODBUS: firmware verification successful\r\n");
					modbus_set_input_reg(INPUT_REG__UPGRADE_STATUS, MODBUS_UPGRADE_STATUS_VERIFIED);
				}
			} else {
				modbus_set_input_reg(INPUT_REG__UPGRADE_STATUS, MODBUS_UPGRADE_STATUS_ERROR);
			}
		} else if (modbus_get_holding_reg(HOLD_REG__UPGRADE_FUNCTION) == MODBUS_UPGRADE_FUNCTION_ACTIVATE) {
			if (modbus_get_input_reg(INPUT_REG__UPGRADE_STATUS) == MODBUS_UPGRADE_STATUS_VERIFIED) {
				PRINTF("MODBUS: activating new firmware\r\n");
				upgrade_activate();
				/* NOTREACHED */
			} else {
				modbus_set_input_reg(INPUT_REG__UPGRADE_STATUS, MODBUS_UPGRADE_STATUS_ERROR);
			}
		}
		modbus_set_holding_reg(HOLD_REG__UPGRADE_FUNCTION, 0);
	}
}

#endif /* BOOTLOADER */

void modbus_pin_init(void)
{
	ioport_set_pin_dir(CFG_MODBUS_ADDRESS_1, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(CFG_MODBUS_ADDRESS_2, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(CFG_MODBUS_ADDRESS_3, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(CFG_MODBUS_ADDRESS_4, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(CFG_MODBUS_DE_PIN, IOPORT_DIR_OUTPUT);
	ioport_set_pin_level(CFG_MODBUS_DE_PIN, IOPORT_PIN_LEVEL_LOW);
	ioport_set_pin_dir(CFG_MODBUS_RE_PIN, IOPORT_DIR_OUTPUT);
	ioport_set_pin_level(CFG_MODBUS_RE_PIN, IOPORT_PIN_LEVEL_HIGH);
}