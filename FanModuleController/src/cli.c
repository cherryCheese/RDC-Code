/*
 * cli.c: command line interface
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#ifndef BOOTLOADER

#include <asf.h>
#include <string.h>

#include "config.h"
#include "uart.h"
#include "cli.h"
#include "spi_flash.h"
#include "upgrade.h"
#include "sys_timer.h"
#include "fan.h"
#include "i2c_local.h"
#include "eeprom_driver.h"
#include "modbus.h"
#include "watchdog.h"
#include "env.h"

#define CLI_INBUF_SIZE	256
#define CLI_MAX_ARGS	256
#define CLI_PROMPT		"FMC> "

static uint8_t prompt = 1;
static uint8_t inbuf[CLI_INBUF_SIZE];
static uint32_t inbuf_ptr;
static uint8_t upgrade_mode;


#define CLI_COMMANDS (int)(sizeof(cli_cmd_switch)/sizeof(*cli_cmd_switch))

struct cli_cmd_entry {
	const char *cmd;
	const char *args;
	const char *desc;
	int (*func)(int argc, char **argv);
};

/***************************************************************
 *                 CLI command handlers                        *
 ***************************************************************/

static int cli_cmd_modbus_write_holding_regs(int argc, char **argv)
{
	uint8_t offset;
	char *end;
	int i;
	
	if (argc < 2) {
		PRINTF("Invalid arguments\r\n");
		return -1;
	}
	
	offset = strtoul(argv[0], &end, 0);
	
	if ((argc-1) > (CFG_MODBUS_HOLDING_REGS - offset)) {
		PRINTF("To many bytes, beginning from offset\r\n");
		return -1;
	}
	
	PRINTF("Writing %d bytes to holding regs @ offset %d\r\n", argc - 1, offset);
	for (i = 0; i < argc - 1; i++) {
		modbus_set_holding_reg(i+offset, strtoul(argv[i + 1], &end, 0));
	}
	
	PRINTF("Success\r\n");
	
	return 0;
}

static int cli_cmd_modbus_read_holding_regs(int argc, char **argv)
{
	uint8_t offset, len;
	char *end;
	int i;
	
	if (argc != 2) {
		PRINTF("Invalid arguments\r\n");
		return -1;
	}
	offset = strtoul(argv[0], &end, 0);
	len = strtoul(argv[1], &end, 0);
	if (len < 1 || (len > (CFG_MODBUS_HOLDING_REGS-offset))) {
		PRINTF("Invalid length: %d\r\n", len);
		return -1;
	}

	for (i = 0; i < (int)len; i++) {
		PRINTF("%04x ", modbus_get_holding_reg(i+offset));
	}
	PRINTF("\r\n");
	
	return 0;
}


static int cli_cmd_modbus_read_input_regs(int argc, char **argv)
{
	uint8_t offset, len;
	char *end;
	int i;
	
	if (argc != 2) {
		PRINTF("Invalid arguments\r\n");
		return -1;
	}
	offset = strtoul(argv[0], &end, 0);
	len = strtoul(argv[1], &end, 0);
	if (len < 1 || (len > (CFG_MODBUS_INPUT_REGS-offset))) {
		PRINTF("Invalid length: %d\r\n", len);
		return -1;
	}

	for (i = 0; i < (int)len; i++) {
		PRINTF("%04x ", modbus_get_input_reg(i+offset));
	}
	PRINTF("\r\n");
	
	return 0;
}


static int cli_cmd_modbus_read_discrete_inputs(int argc, char **argv)
{
	uint8_t offset, len;
	char *end;
	int i;
	
	if (argc != 2) {
		PRINTF("Invalid arguments\r\n");
		return -1;
	}
	offset = strtoul(argv[0], &end, 0);
	len = strtoul(argv[1], &end, 0);
	if (len < 1 || (len > (CFG_MODBUS_DISCRETE_INPUTS-offset))) {
		PRINTF("Invalid length: %d\r\n", len);
		return -1;
	}

	for (i = 0; i < (int)len; i++) {
		PRINTF("%02x ", modbus_get_discrete_input(i+offset));
	}
	PRINTF("\r\n");
	
	return 0;
}


static int cli_cmd_reset(int argc, char **argv)
{
	SYSTEM_RESET;
	/* NOTREACHED */
	
	return 0;
}

static int cli_cmd_upgrade(int argc, char **argv)
{
	if (upgrade_start() < 0) {
		return -1;
	}
	PRINTF("Entering firmware upgrade mode: send IHEX data, or press ^C to terminate\r\n");
	upgrade_mode = 1;
	quiet = 1;
	prompt = 0;
	
	return 0;
}

static int cli_cmd_setenv(int argc, char **argv)
{
	char *var, *end;
	uint32_t val;
	
	if (argc != 2) {
		PRINTF("Invalid arguments\r\n");
		return -1;
	}
	var = argv[0];
	val = strtoul(argv[1], &end, 0);
	if (env_find(var) < 0) {
		PRINTF("Variable %s not found\r\n", var);
	}
	if (env_set(var, val) < 0) {
		PRINTF("ERROR: env_set() failed\r\n");
		return -1;
	}
	
	return 0;
}

static int cli_cmd_printenv(int argc, char **argv)
{
	char *var;
	uint32_t val;
	
	if (argc > 1) {
		PRINTF("Invalid arguments\r\n");
		return -1;
	}
	if (argc == 0) {
		env_print_all();
	} else {
		var = argv[0];
		if (env_find(var) < 0) {
			PRINTF("Variable %s not found\r\n", var);
			return -1;
		}
		val = env_get(var);
		PRINTF("%s = %lu\r\n", var, val);
	}
	
	return 0;
}

static int cli_cmd_env_reset(int argc, char **argv) {
	env_reset();
	/* NOTREACHED */
	return 0;
}

#ifdef CFG_DEVEL_COMMANDS_ENABLE

static int cli_cmd_eeprom_read(int argc, char **argv)
{
	uint8_t offset, len;
	uint8_t buf[256];
	char *end;
	int i;
	
	if (argc != 2) {
		PRINTF("Invalid arguments\r\n");
		return -1;
	}
	offset = strtoul(argv[0], &end, 0);
	len = strtoul(argv[1], &end, 0);
	if (len < 1 || len > 256) {
		PRINTF("Invalid length: %d\r\n", len);
		return -1;
	}
	if (eeprom_read(buf, offset, len) < 0) {
		PRINTF("ERROR: eeprom_read failed\r\n");
		return -1;
	}
	for (i = 0; i < (int)len; i++) {
		PRINTF("%02x ", buf[i]);
	}
	PRINTF("\r\n");
	
	return 0;
}

static int cli_cmd_eeprom_write(int argc, char **argv)
{
	uint8_t offset;
	uint8_t buf[256];
	char *end;
	int i;
	
	if (argc < 2) {
		PRINTF("Invalid arguments\r\n");
		return -1;
	}
	offset = strtoul(argv[0], &end, 0);
	PRINTF("Writing %d bytes to emulated EEPROM @ offset %d\r\n", argc - 1, offset);
	for (i = 0; i < argc - 1; i++) {
		buf[i] = strtoul(argv[i + 1], &end, 0);
	}
	if (eeprom_write((const uint8_t *)buf, offset, argc - 1) < 0) {
		PRINTF("ERROR: eeprom_write failed\r\n");
		return -1;
	}
	PRINTF("Success\r\n");
	
	return 0;
}

static int cli_cmd_eeprom_commit(int argc, char **argv)
{
	eeprom_emulator_commit_page_buffer();
	
	return 0;
}

static int cli_cmd_flash_read(int argc, char **argv)
{
	uint32_t addr, len;
	uint8_t buf[256];
	char *end;
	int i;
	
	if (argc != 2) {
		PRINTF("Invalid arguments\r\n");
		return -1;
	}
	addr = strtoul(argv[0], &end, 0);
	len = strtoul(argv[1], &end, 0);
	if (len < 1 || len > 256) {
		PRINTF("Invalid length: %lu\r\n", len);
		return -1;
	}
	if (spi_flash_read(addr, buf, len) < 0) {
		PRINTF("ERROR: spi_flash_read failed\r\n");
		return -1;
	}
	for (i = 0; i < (int)len; i++) {
		PRINTF("%02x ", buf[i]);
	}
	PRINTF("\r\n");
	
	return 0;
}

static int cli_cmd_flash_erase(int argc, char **argv)
{
	uint32_t addr, len;
	char *end;
	
	if (!argc) {
		addr = 0;
		len = -1;
		PRINTF("Erasing the entire Flash\r\n");
	} else if (argc != 2) {
		PRINTF("Invalid arguments\r\n");
		return -1;
	} else {
		addr = strtoul(argv[0], &end, 0);
		len = strtoul(argv[1], &end, 0);
		PRINTF("Erasing Flash @ 0x%06lx (%lu bytes)\r\n", addr, len);
	}
	if (spi_flash_erase(addr, len) < 0) {
		PRINTF("ERROR: spi_flash_erase failed\r\n");
		return -1;
	}
	PRINTF("Success\r\n");
	
	return 0;
}

static int cli_cmd_flash_write(int argc, char **argv)
{
	uint32_t addr;
	uint8_t buf[256];
	char *end;
	int i;
	
	if (argc < 2) {
		PRINTF("Invalid arguments\r\n");
		return -1;
	}
	addr = strtoul(argv[0], &end, 0);
	PRINTF("Writing %d bytes to Flash @ 0x%06lx\r\n", argc - 1, addr);
	for (i = 0; i < argc - 1; i++) {
		buf[i] = strtoul(argv[i + 1], &end, 0);
	}
	if (spi_flash_program(addr, buf, argc - 1) < 0) {
		PRINTF("ERROR: spi_flash_program failed\r\n");
		return -1;
	}
	PRINTF("Success\r\n");
	
	return 0;
}

static int cli_cmd_flash_copy(int argc, char **argv)
{
	uint8_t buf[256];
	uint32_t src, dst, len, chunk;
	char *end;
	
	dst = strtoul(argv[0], &end, 0);
	src = strtoul(argv[1], &end, 0);
	len = strtoul(argv[2], &end, 0);

	while (len > 0) {
		chunk = sizeof(buf);
		if (chunk > len) {
			chunk = len;
		}
		if (spi_flash_read(src, buf, chunk) < 0) {
			PRINTF("ERROR: spi_flash_read failed\r\n");
			return -1;
		}
		if (spi_flash_program(dst, buf, chunk) < 0) {
			PRINTF("ERROR: spi_flash_program failed\r\n");
			return -1;
		}
		src += chunk;
		dst += chunk;
		len -= chunk;
	}
	PRINTF("Success\r\n");
	
	return 0;
}

static int cli_cmd_flash_compare(int argc, char **argv)
{
	uint8_t buf1[256], buf2[256];
	uint32_t addr1, addr2, len, i, chunk;
	char *end;
	
	addr1 = strtoul(argv[0], &end, 0);
	addr2 = strtoul(argv[1], &end, 0);
	len = strtoul(argv[2], &end, 0);
	
	while (len > 0) {
		chunk = sizeof(buf1);
		if (chunk > len) {
			chunk = len;
		}
		if (spi_flash_read(addr1, buf1, chunk) < 0) {
			PRINTF("ERROR: spi_flash_read failed\r\n");
			return -1;
		}
		if (spi_flash_read(addr2, buf2, chunk) < 0) {
			PRINTF("ERROR: spi_flash_read failed\r\n");
			return -1;
		}
		for (i = 0; i < chunk; i++) {
			if (buf1[i] != buf2[i]) {
				PRINTF("%02x@%08lx != %02x@%08lx\r\n", buf1[i], addr1 + i, buf2[i], addr2 + i);
			}
		}
		addr1 += chunk;
		addr2 += chunk;
		len -= chunk;
		WDT_RESET;
	}
	
	return 0;
}

static int cli_cmd_flash_status(int argc, char **argv)
{
	uint8_t status;
	
	if (spi_flash_get_status(&status) < 0) {
		return -1;
	}
	PRINTF("0x%02x\r\n", status);
	
	return 0;
}

static int cli_cmd_hang(int argc, char **argv)
{
	PRINTF("Hanging the CPU...\r\n");
	while (1)
		;
	/* NOTREACHED */
	
	return 0;
}

static int cli_cmd_systick(int argc, char **argv)
{
	PRINTF("%ld\r\n", get_jiffies());
	
	return 0;
}


#endif /* CFG_DEVEL_COMMANDS_ENABLE */


/***************************************************************
 *                   CLI command switch                        *
 ***************************************************************/

static int cli_cmd_help(int argc, char **argv);

static struct cli_cmd_entry cli_cmd_switch[] = {
	{
		"holding_regs_write",
		"offset, byte1, byte2, ...",
		"Write to Modbus holding registers",
		cli_cmd_modbus_write_holding_regs
	},
	{
		"holding_regs_read",
		"offset, len",
		"Read Modbus holding registers ",
		cli_cmd_modbus_read_holding_regs
	},
	{
		"input_regs_read",
		"offset, len",
		"Read Modbus input registers ",
		cli_cmd_modbus_read_input_regs
	},
	{
		"discrete_inputs_read",
		"offset, len",
		"Read Modbus discrete inputs ",
		cli_cmd_modbus_read_discrete_inputs
	},
	{
		"help",
		"",
		"Print help information",
		cli_cmd_help
	},
	{
		"reset",
		"",
		"Reset the CPU",
		cli_cmd_reset
	},
	{
		"upgrade",
		"",
		"Enter upgrade mode",
		cli_cmd_upgrade
	},
	{
		"setenv",
		"var, value",
		"Set an environment variable",
		cli_cmd_setenv
	},
	{
		"printenv",
		"[var]",
		"Print an environment variable (or all variables, if var is omitted)",
		cli_cmd_printenv
	},
	{
		"env_reset",
		"",
		"Reset to default environment",
		cli_cmd_env_reset
	},
	
	
#ifdef CFG_DEVEL_COMMANDS_ENABLE
	{
		"eeprom_read",
		"offset, len",
		"Read from emulated EEPROM",
		cli_cmd_eeprom_read
	},
	{
		"eeprom_write",
		"offset, len, byte1, byte2, ...",
		"Write to emulated EEPROM",
		cli_cmd_eeprom_write
	},
	{
		"eeprom_commit",
		"",
		"Commit EEPROM page buffer to NVM",
		cli_cmd_eeprom_commit
	},
	{
		"systick",
		"",
		"Get current system timer counter",
		cli_cmd_systick
	},
	{
		"flash_read",
		"addr, len",
		"Read from SPI Flash",
		cli_cmd_flash_read
	},
	{
		"flash_erase",
		"addr, len",
		"Erase SPI Flash (erase the entire Flash if no arguments are specified)",
		cli_cmd_flash_erase
	},
	{
		"flash_write",
		"addr, len, byte1, byte2, ...",
		"Write to SPI Flash",
		cli_cmd_flash_write
	},
	{
		"flash_copy",
		"dst, src, len",
		"Copy SPI Flash data",
		cli_cmd_flash_copy
	},
	{
		"flash_compare",
		"addr1, addr2, len",
		"Compare SPI Flash data",
		cli_cmd_flash_compare
	},
	{
		"flash_status",
		"",
		"Get SPI Flash status",
		cli_cmd_flash_status
	},
	{
		"hang",
		"",
		"Hang the CPU (for WDT testing)",
		cli_cmd_hang
	}
#endif /* CFG_DEVEL_COMMANDS_ENABLE */
};

/***************************************************************
 *                   Auxiliary Functions                       *
 ***************************************************************/

static int cli_cmd_lookup(char *cmd)
{
	int i;
	
	for (i = 0; i < CLI_COMMANDS; i++) {
		if (!strcasecmp(cmd, cli_cmd_switch[i].cmd)) {
			return i;
		}
	}
	
	return -1;
}

static int cli_cmd_help(int argc, char **argv)
{
	int cmd;
	
	if (argc > 0) {
		cmd = cli_cmd_lookup(argv[0]);
		if (cmd >= 0) {
			PRINTF("%s %s\r\n  %s\r\n", cli_cmd_switch[cmd].cmd, cli_cmd_switch[cmd].args, cli_cmd_switch[cmd].desc);
		} else {
			PRINTF("Command not supported: %s\r\n", argv[0]);
		}
	} else {
		if(env_get("hide_cli_commands") == 0)
		{
			PRINTF("Supported commands:\r\n");
			for (cmd = 0; cmd < CLI_COMMANDS; cmd++) {
				PRINTF("  %s\r\n", cli_cmd_switch[cmd].cmd);
			}
		}
	}
	
	return 0;
}

static int iswhitespace(char c)
{
	return c == ' ' || c == '\t';
}

static void cli_parse(char *buf)
{
	char *argv[CLI_MAX_ARGS];
	int argc, cmd, ret;
	
	for (argc = 0; argc < CLI_MAX_ARGS && *buf; argc++) {
		while (*buf && iswhitespace(*buf)) {
			buf++;
		}
		if (!*buf) {
			break;
		}
		argv[argc] = buf;
		while (*buf && !iswhitespace(*buf)) {
			buf++;
		}
		if (*buf) {
			*buf++ = 0;
		}
	}
	if (!argc) {
		return;
	}
	
	cmd = cli_cmd_lookup(argv[0]);
	if (cmd >= 0) {
		ret = cli_cmd_switch[cmd].func(argc - 1, argv + 1);
		if (ret < 0) {
			PRINTF("ERROR: %d\r\n", ret);
		}
	} else {
		PRINTF("Invalid command: %s\r\n", argv[0]);
	}
}

static void cli_upgrade_parse(char *buf)
{
	int ret;
	
	if (!*buf) {
		return;
	}
	ret = upgrade_parse_ihex(buf);
	if (ret < 0) {
		upgrade_mode = 0;
		quiet = 0;
	} else {
		/* Acknowledge */
		uart_puts(CFG_CONSOLE_CHANNEL, "OK\r\n");
	}
	if (ret == 1) {
		uart_puts(CFG_CONSOLE_CHANNEL, "Data upload successful, verifying firmware...\r\n");
		if (upgrade_verify() < 0) {
			uart_puts(CFG_CONSOLE_CHANNEL, "ERROR: verification failed, skipping activation\r\n");
			return;
		}
		uart_puts(CFG_CONSOLE_CHANNEL, "Verified OK, activating firmware...\r\n");
		if (upgrade_activate() < 0) {
			uart_puts(CFG_CONSOLE_CHANNEL, "ERROR: activation failed\r\n");
			return;
		}
		/* NOTREACHED */		
	}
}

void do_cli(void)
{
	char c;
	
	if (prompt && !upgrade_mode) {
		uart_puts(CFG_CONSOLE_CHANNEL, CLI_PROMPT);
		prompt = 0;
	}
	if (!uart_gets(CFG_CONSOLE_CHANNEL, &c, 1)) {
		return;
	}
	if (c == '\n') {
		/* Do nothing: see '\r' below */
	} else if (c == '\r') {
		inbuf[inbuf_ptr] = 0;
		if (upgrade_mode) {
			cli_upgrade_parse((char *)inbuf);
		} else {
			uart_puts(CFG_CONSOLE_CHANNEL, "\r\n");
			cli_parse((char *)inbuf);
		}
		inbuf_ptr = 0;
		prompt = 1;
	} else if ((c == 127 || c == 8) && !upgrade_mode) {
		/* Backspace/Delete */
		if (inbuf_ptr > 0) {
			inbuf[--inbuf_ptr] = 0;
			uart_putc(CFG_CONSOLE_CHANNEL, c);
		}
	} else if (c == 3) {
		/* CTRL+C */
		upgrade_mode = 0;
		quiet = 0;
		prompt = 1;
		inbuf_ptr = 0;
		uart_puts(CFG_CONSOLE_CHANNEL, "\r\n");
	} else if (inbuf_ptr < CLI_INBUF_SIZE - 1) {
		inbuf[inbuf_ptr++] = c;
		if (!upgrade_mode) {
			uart_putc(CFG_CONSOLE_CHANNEL, c);
		}
	}
}

#endif /* BOOTLOADER */