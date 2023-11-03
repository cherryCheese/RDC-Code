/*
 * i2c_local.c
 *
 * Created: 31.08.2020 10:40:39
 *  Author: E1130513
 */ 

#include <asf.h>

#include "i2c_local.h"
#include "config.h"
#include "uart.h"
#include "cli.h"
#include "sys_timer.h"
#include "modbus.h"

#ifndef BOOTLOADER

static struct i2c_master_packet wr_packet;
static struct i2c_master_packet rd_packet;
static struct i2c_master_module i2c_master_instance;
static volatile bool i2c_master_write_complete = false;
static volatile bool i2c_master_read_complete = false;
static volatile uint32_t cnt_i2c_master_timeout;
static uint32_t ina226_1sec_timer = 0;
static uint32_t ina226_current = 0,  ina226_voltage=0;
static uint16_t	t_h_temperature, t_h_humidity;


/* Forward declarations */
static void i2c_master_write_complete_callback(struct i2c_master_module *const module);
static void i2c_master_read_complete_callback(struct i2c_master_module *const module);
static void i2c_master_error_callback(struct i2c_master_module *const module);
static int i2c_master_write (uint8_t *write_buffer, uint16_t data_length, uint16_t slave_address);
static int i2c_master_read (uint8_t *read_buffer, uint16_t data_length, uint16_t slave_address);
static void i2c_local_sync_to_modbus(void);
static uint8_t send_buffer[10], read_buffer[10];

static void i2c_get_values(void)
{
	send_buffer[0] = 1;
	i2c_master_write(send_buffer, 1, CFG_I2C_ADDRESS_INA226);
	if(i2c_master_read(read_buffer, 2, CFG_I2C_ADDRESS_INA226) == 0)
	{
		ina226_current = (uint32_t)(1000 * ((float)((read_buffer[0]<<8) | read_buffer[1])) * 0.0000025 / 0.001);
		modbus_set_discrete_input(DIS_INPUT__CURRENT_SENSOR_BROKEN, 0);
	}
	else
	{
		ina226_current = 0;
		modbus_set_discrete_input(DIS_INPUT__CURRENT_SENSOR_BROKEN, 1);
	}
	
	send_buffer[0] = 2;
	i2c_master_write(send_buffer, 1, CFG_I2C_ADDRESS_INA226);
	if(i2c_master_read(read_buffer, 2, CFG_I2C_ADDRESS_INA226) == 0)
	{
		ina226_voltage = (uint32_t)(1000* ((float)((read_buffer[0]<<8) | read_buffer[1]))  * 0.00125 / 0.2130); //theoretically 0.21541318, but there is a offset.
		modbus_set_discrete_input(DIS_INPUT__VOLTAGE_SENSOR_BROKEN, 0);
	}
	else
	{
		ina226_voltage = 0;
		modbus_set_discrete_input(DIS_INPUT__VOLTAGE_SENSOR_BROKEN, 1);
	}
		
	if(i2c_master_read(read_buffer, 6, CFG_I2C_ADDRESS_T_H) == 0)
	{
		t_h_temperature = (uint16_t)(100*(-45+175*((float)((read_buffer[0]<<8) | read_buffer[1]))/(65536-1)));
		t_h_humidity = (uint16_t)(100*100*((float)((read_buffer[3]<<8) | read_buffer[4]))/(65536-1));
		modbus_set_discrete_input(DIS_INPUT__TEMP_SENSOR_BROKEN, 0);
		modbus_set_discrete_input(DIS_INPUT__HUMIDITY_SENSOR_BROKEN, 0);
	}
	else
	{
		t_h_temperature = 0;
		t_h_humidity = 0;
		modbus_set_discrete_input(DIS_INPUT__TEMP_SENSOR_BROKEN, 1);
		modbus_set_discrete_input(DIS_INPUT__HUMIDITY_SENSOR_BROKEN, 1);
		ioport_set_pin_level(CFG_RESET_SHT31, IOPORT_PIN_LEVEL_HIGH);
		delay_cycles_ms(1);
		ioport_set_pin_level(CFG_RESET_SHT31, IOPORT_PIN_LEVEL_LOW);
		delay_cycles_ms(5);
	}
	send_buffer[0] = 0x24;
	send_buffer[1] = 0x0B;
	i2c_master_write(send_buffer, 2, CFG_I2C_ADDRESS_T_H);	
}

static void i2c_master_write_complete_callback(struct i2c_master_module *const module)
{
	i2c_master_write_complete = true;
}

static void i2c_master_read_complete_callback(struct i2c_master_module *const module)
{
	i2c_master_read_complete = true;
}

void i2c_local_init(void)
{
	struct i2c_master_config config_i2c_master;
	i2c_master_get_config_defaults(&config_i2c_master);
	config_i2c_master.buffer_timeout = 65535;
	config_i2c_master.pinmux_pad0 = CFG_I2C_SERCOM_PINMUX_PAD0;
	config_i2c_master.pinmux_pad1 =  CFG_I2C_SERCOM_PINMUX_PAD1;
	while(i2c_master_init(&i2c_master_instance, CFG_I2C_MODULE, &config_i2c_master)!= STATUS_OK);
	i2c_master_enable(&i2c_master_instance);
	
	i2c_master_register_callback(&i2c_master_instance, i2c_master_write_complete_callback,I2C_MASTER_CALLBACK_WRITE_COMPLETE);
	i2c_master_enable_callback(&i2c_master_instance,I2C_MASTER_CALLBACK_WRITE_COMPLETE);
	i2c_master_register_callback(&i2c_master_instance, i2c_master_read_complete_callback, I2C_MASTER_CALLBACK_READ_COMPLETE);
	i2c_master_enable_callback(&i2c_master_instance,I2C_MASTER_CALLBACK_READ_COMPLETE);
	
	ioport_set_pin_dir(CFG_RESET_SHT31, IOPORT_DIR_OUTPUT);
	ioport_set_pin_level(CFG_RESET_SHT31, IOPORT_PIN_LEVEL_LOW);
	
	send_buffer[0] = 0x24;
	send_buffer[1] = 0x0B;
	i2c_master_write(send_buffer, 2, CFG_I2C_ADDRESS_T_H);
}


static int i2c_master_write (uint8_t *write_buffer, uint16_t data_length, uint16_t slave_address)
{
	wr_packet.data			= write_buffer;
	wr_packet.address		= slave_address;
	wr_packet.data_length	= data_length;
	
	if(STATUS_OK!=i2c_master_write_packet_job(&i2c_master_instance, &wr_packet))
	{
		return -1;
	}
	
	i2c_master_write_complete = false;
	cnt_i2c_master_timeout = 0;
	
	
	while(i2c_master_write_complete == false)
	{
		delay_cycles_ms(1);
		if(cnt_i2c_master_timeout > CFG_I2C_MASTER_TIMEOUT)
		{
			i2c_master_write_complete = true;
			return -1;
		}
		else
		{
			cnt_i2c_master_timeout++;
		}
	}
	return 0;
}


static int i2c_master_read (uint8_t *read_buffer, uint16_t data_length, uint16_t slave_address)
{
	rd_packet.address     = slave_address;
	rd_packet.data_length = data_length;
	rd_packet.data        = read_buffer;
	
	if(STATUS_OK!=i2c_master_read_packet_job(&i2c_master_instance, &rd_packet))
	{
		return -1;
	}
	
	i2c_master_read_complete = false;
	cnt_i2c_master_timeout = 0;
	
	
	while(i2c_master_read_complete == false)
	{
		delay_cycles_ms(1);
		if(cnt_i2c_master_timeout > CFG_I2C_MASTER_TIMEOUT)
		{
			i2c_master_read_complete = true;//Also set true in the i2c_master_write_complete_callback
			return -1;
		}
		else
		{
			cnt_i2c_master_timeout++;
		}
	}
	return 0;
}

static void i2c_local_sync_to_modbus(void)
{
	modbus_set_input_reg(INPUT_REG__VOLTAGE_SENSOR_SPEED_3_2, ((ina226_voltage)>>16 & 0xFFFF));
	modbus_set_input_reg(INPUT_REG__VOLTAGE_SENSOR_SPEED_1_0, ((ina226_voltage) & 0xFFFF));
	modbus_set_input_reg(INPUT_REG__CURRENT_SENSOR, (uint16_t)(ina226_current));
	modbus_set_input_reg(INPUT_REG__TEMP_SENSOR, t_h_temperature);
	modbus_set_input_reg(INPUT_REG__HUMIDITY_SENSOR, t_h_humidity);
}

void do_i2c_local(void)
{
	if (get_jiffies() - ina226_1sec_timer >= 1000) {
		ina226_1sec_timer = get_jiffies();
		i2c_get_values();
		i2c_local_sync_to_modbus();
	}
}

#endif /* BOOTLOADER */