/*
 * modbus.h
 *
 * Created: 11/9/2020 3:00:10 PM
 *  Author: E1210640
 */ 


#ifndef MODBUS_H_
#define MODBUS_H_

#define DIS_INPUT__UNIT_GENERAL_ALARM_STATUS		0x00
#define DIS_INPUT__TEMP_SENSOR_BROKEN				0x64
#define DIS_INPUT__HUMIDITY_SENSOR_BROKEN			0x65
#define DIS_INPUT__VOLTAGE_SENSOR_BROKEN			0x66
#define DIS_INPUT__CURRENT_SENSOR_BROKEN			0x67

#define INPUT_REG__DEVICE_PN_7_6					0x00
#define INPUT_REG__DEVICE_PN_5_4					0x01
#define INPUT_REG__DEVICE_PN_3_2					0x02
#define INPUT_REG__DEVICE_PN_1_0					0x03

#define INPUT_REG__DEVICE_SN_11_10					0x04
#define INPUT_REG__DEVICE_SN_9_8					0x05
#define INPUT_REG__DEVICE_SN_7_6					0x06
#define INPUT_REG__DEVICE_SN_5_4					0x07
#define INPUT_REG__DEVICE_SN_3_2					0x08
#define INPUT_REG__DEVICE_SN_1_0					0x09

#define INPUT_REG__CONTROLLER_PN_7_6				0x0A
#define INPUT_REG__CONTROLLER_PN_5_4				0x0B
#define INPUT_REG__CONTROLLER_PN_3_2				0x0C
#define INPUT_REG__CONTROLLER_PN_1_0				0x0D

#define INPUT_REG__CONTROLLER_SN_11_10				0x0E
#define INPUT_REG__CONTROLLER_SN_9_8				0x0F
#define INPUT_REG__CONTROLLER_SN_7_6				0x10
#define INPUT_REG__CONTROLLER_SN_5_4				0x11
#define INPUT_REG__CONTROLLER_SN_3_2				0x12
#define INPUT_REG__CONTROLLER_SN_1_0				0x13

#define INPUT_REG__FIRMWARE_PN_7_6					0x14
#define INPUT_REG__FIRMWARE_PN_5_4					0x15
#define INPUT_REG__FIRMWARE_PN_3_2					0x16
#define INPUT_REG__FIRMWARE_PN_1_0					0x17
#define INPUT_REG__FIRMWARE_REV_1_0					0x18

#define INPUT_REG__FAN_CURRENT_PWM					0x19
#define INPUT_REG__TEMP_SENSOR						0x1A
#define INPUT_REG__HUMIDITY_SENSOR					0x1B
#define INPUT_REG__FAN_CURRENT_SPEED				0x1C
#define INPUT_REG__VOLTAGE_SENSOR_SPEED_3_2			0x1D
#define INPUT_REG__VOLTAGE_SENSOR_SPEED_1_0			0x1E
#define INPUT_REG__CURRENT_SENSOR					0x1F
#define INPUT_REG__OPERATING_HOURS_3_2				0x20
#define INPUT_REG__OPERATING_HOURS_1_0				0x21
#define INPUT_REG__RPM_DEVIATION_1_0				0x22
#define INPUT_REG__MANUFACTURER_5_4					0x37
#define INPUT_REG__MANUFACTURER_3_2					0x38
#define INPUT_REG__MANUFACTURER_1_0					0x39
#define INPUT_REG__DEVICE_NAME_9_8					0x3A
#define INPUT_REG__DEVICE_NAME_7_6					0x3B
#define INPUT_REG__DEVICE_NAME_5_4					0x3C
#define INPUT_REG__DEVICE_NAME_3_2					0x3D
#define INPUT_REG__DEVICE_NAME_1_0					0x3E
#define INPUT_REG__UPGRADE_STATUS					0x3F

#define HOLD_REG__UNIT_OFF_ON						0x00
#define HOLD_REG__PRECONFIG_FAN_REQUEST				0x01
#define HOLD_REG__GREEN_LED							0x02
#define HOLD_REG__RED_LED							0x03
#define HOLD_REG__FAN_REQUEST						0x04
#define HOLD_REG__FAN_REUEST_MIN					0x05
#define HOLD_REG__FAN_REUEST_MAX					0x06
#define HOLD_REG__PWM_FREQUENCY						0x07
#define HOLD_REG__PWM_DELAY							0x08
#define HOLD_REG__PULSES_PER_REVOLUTION				0x09
#define HOLD_REG__FAN_CURVE_PWM_0					0x0a
#define HOLD_REG__MODBUS_DEAD_TIME					0x6F
#define HOLD_REG__SOFTWARE_RESET					0x70
#define HOLD_REG__UPGRADE_FUNCTION					0x8F


int modbus_init(void);
void modbus_pin_init(void);
void modbus_receive(uint8_t ch);
uint8_t modbus_get_discrete_input(uint16_t nr);
void modbus_set_discrete_input(uint16_t nr, uint8_t val);
uint16_t modbus_get_input_reg(uint16_t nr);
void modbus_set_input_reg(uint16_t nr, uint16_t val);
uint16_t modbus_get_holding_reg(uint16_t nr);
void modbus_set_holding_reg(uint16_t nr, uint16_t val);
uint8_t modbus_watchdog (void);
void do_modbus(void);

#endif /* MODBUS_H_ */