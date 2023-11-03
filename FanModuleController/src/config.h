/*
 * config.h
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <asf.h>

/******************************************************************
 *              Firmware configuration file                       *
 ******************************************************************/

#if 0
#define CFG_DEBUG_ENABLE
#endif

/*
 * Starting address of the firmware in NVM (after bootloader).
 * Must match the ".text=" definition in the linker memory settings!!!
 */
#define CFG_FIRMWARE_START			0x4000

/* Fuses (determined via Atmel Studio->Tools->Device Programming->Fuses) */
#define CFG_FUSES_USER_WORD_0		0xD8E0C7AF
#define CFG_FUSES_USER_WORD_1		0xFFFF3F5D

/* EEPROM emulation */
#define CFG_EEPROM_ENABLE
#define CFG_EEPROM_BOD33_LEVEL		39						/* Brown-out level: 2.84V */
#define CFG_EEPROM_PN_OFFSET		(0*EEPROM_PAGE_SIZE)	/* Part/serial numbers in page 0 */
#define CFG_EEPROM_HOLDING_OFFSET	(1*EEPROM_PAGE_SIZE)	/* Holding registers in page 1+ */
#define CFG_EEPROM_ENV_OFFSET		(6*EEPROM_PAGE_SIZE)	/* Environment variables in page 6+ */

#define CFG_WDT_TIMEOUT				3 /* seconds */

/* Enable development and debugging commands */
#define CFG_DEVEL_COMMANDS_ENABLE

/* Firmware */
#define CFG_FIRMWARE_NUMBER			"63998290" 
#define CFG_FIRMWARE_VERSION		"51"
#define CFG_MANUFACTURER			"nVent "
#define CFG_DEVICE_NAME				"Fan Module"


/* LEDs */
#define CFG_HEARTBEAT_LED			PIN_PB30
#define CFG_LED_GREEN				PIN_PB00
#define CFG_LED_RED					PIN_PB31


/*
 * UART/console configuration:
 *
 * CFG_UART_CHANNEL(channel, SERCOMx, baud_rate, parity, mux_setting, pinmux_pad0, pinmux_pad1, pinmux_pad2, pinmux_pad3)
 */
#define CFG_UART_RING_SIZE			1024
#ifdef BOOTLOADER
#define CFG_UART_CHANNELS			CFG_UART_CHANNEL(0, SERCOM3, CFG_CONSOLE_BAUD_RATE, USART_PARITY_NONE, USART_RX_3_TX_2_XCK_3, PINMUX_UNUSED, PINMUX_UNUSED, PINMUX_PA24C_SERCOM3_PAD2, PINMUX_PA25C_SERCOM3_PAD3)
#else
#define CFG_UART_CHANNELS			CFG_UART_CHANNEL(0, SERCOM3, CFG_CONSOLE_BAUD_RATE, USART_PARITY_NONE, USART_RX_3_TX_2_XCK_3, PINMUX_UNUSED, PINMUX_UNUSED, PINMUX_PA24C_SERCOM3_PAD2, PINMUX_PA25C_SERCOM3_PAD3) \
									CFG_UART_CHANNEL(1, SERCOM4, CFG_MODBUS_BAUD_RATE, USART_PARITY_EVEN, USART_RX_3_TX_2_XCK_3, PINMUX_UNUSED, PINMUX_UNUSED, PINMUX_PB14C_SERCOM4_PAD2, PINMUX_PB15C_SERCOM4_PAD3)
#endif /* BOOTLOADER */

#define CFG_CONSOLE_CHANNEL			0
#define CFG_CONSOLE_BAUD_RATE		115200


/* MODBUS configuration */
#define CFG_MODBUS_CHANNEL			1
#define CFG_MODBUS_BAUD_RATE		115200
#define CFG_MODBUS_SLAVE_ADDRESS	11
#define CFG_MODBUS_ADDRESS_1		PIN_PA00
#define CFG_MODBUS_ADDRESS_2		PIN_PA01
#define CFG_MODBUS_ADDRESS_3		PIN_PA02
#define CFG_MODBUS_ADDRESS_4		PIN_PB04
#define CFG_MODBUS_RE_PIN			PIN_PA12
#define CFG_MODBUS_DE_PIN			PIN_PA13
#define CFG_MODBUS_TC_MODULE		TC2
#define CFG_MODBUS_DISCRETE_INPUTS	0xD0 
#define CFG_MODBUS_INPUT_REGS		0x40
#define CFG_MODBUS_HOLDING_REGS		0x90


/* Reset values of MODBUS holding registers */
#define CFG_MODBUS_HLD_UNIT_ON_OFF				1
#define CFG_MODBUS_HLD_PRECONFIG_FAN_REQUEST	60
#define CFG_MODBUS_HLD_GREEN_LED				0
#define CFG_MODBUS_HLD_RED_LED					3
#define CFG_MODBUS_HLD_FAN_REUEST_MIN			30
#define CFG_MODBUS_HLD_FAN_REUEST_MAX			100
#define CFG_MODBUS_HLD_PWM_FREQUENCY			1			
#define CFG_MODBUS_HLD_PWM_DELAY				0
#define CFG_MODBUS_HLD_PULSES_PER_REVOLUTION	2
#define CFG_MODBUS_HLD_MODBUS_DEAD_TIME			30
#define CFG_MODBUS_HLD_SOFTWARE_RESET			0
#define CFG_MODBUS_HLD_UPGRADE_FUNCTION			0

/* SPI Flash configuration */
#define CFG_SPI_FLASH_SS_PIN		PIN_PA05
#define CFG_SPI_FLASH_MUX_SETTING	SPI_SIGNAL_MUX_SETTING_E
#define CFG_SPI_FLASH_BAUDRATE		100000
#define CFG_SPI_FLASH_PINMUX_PAD0	PINMUX_PA04D_SERCOM0_PAD0
#define CFG_SPI_FLASH_PINMUX_PAD1	PINMUX_PA05D_SERCOM0_PAD1
#define CFG_SPI_FLASH_PINMUX_PAD2	PINMUX_PA06D_SERCOM0_PAD2
#define CFG_SPI_FLASH_PINMUX_PAD3	PINMUX_PA07D_SERCOM0_PAD3

/* I2C configuration */
#define CFG_I2C_MODULE					SERCOM1
#define CFG_I2C_SERCOM_PINMUX_PAD0		PINMUX_PA16C_SERCOM1_PAD0
#define CFG_I2C_SERCOM_PINMUX_PAD1		PINMUX_PA17C_SERCOM1_PAD1
#define CFG_I2C_MASTER_TIMEOUT			5
#define CFG_I2C_ADDRESS_INA226			0x40
#define CFG_I2C_ADDRESS_T_H				0x44

/* Fan configuration */
#define CFG_PWM_MODULE					TC1
#define CFG_PWM_FREQUENCY				5000
#define CFG_PWM1_PIN					PIN_PA10E_TC1_WO0
#define CFG_PWM1_MUX					MUX_PA10E_TC1_WO0
#define CFG_PWM_INITIAL_VALUE			0		

#define	CFG_INT0_PIN_FAN1				PIN_PB16A_EIC_EXTINT0
#define	CFG_INT0_MUX_FAN1				MUX_PB16A_EIC_EXTINT0

#define CFG_TACHO_MODULE				TC4
#define CFG_CONVERTER_OFF				PIN_PA28

/* Dip-Switch */
#define CFG_DIP_SWITCH_1				PIN_PB02
#define CFG_DIP_SWITCH_2				PIN_PB03
#define CFG_DIP_SWITCH_3				PIN_PB08
#define CFG_DIP_SWITCH_4				PIN_PB09

/* others */
#define CFG_FIRST_START_DONE			0
#define CFG_HIDE_CLI_COMMANDS			0
#define CFG_DISABLE_UPDATE_ABILITY		0
#define CFG_RESET_SHT31					PIN_PA27

/*
 * Non-volatile (persistent) configuration parameters:
 *
 * CFG_ENV_DESC(name, default_value)
 */

#define CFG_ENV_DESCRIPTORS			CFG_ENV_DESC("modbus_baud_rate", CFG_MODBUS_BAUD_RATE)\
									CFG_ENV_DESC("modbus_slave_addr", CFG_MODBUS_SLAVE_ADDRESS)\
									CFG_ENV_DESC("first_start_done", CFG_FIRST_START_DONE)\
									CFG_ENV_DESC("hide_cli_commands", CFG_HIDE_CLI_COMMANDS)\
									CFG_ENV_DESC("disable_update_ability", CFG_DISABLE_UPDATE_ABILITY)
								
#endif /* __CONFIG_H__ */