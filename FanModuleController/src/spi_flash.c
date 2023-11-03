/*
 * spi_flash.c: SPI Flash driver
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#include <asf.h>

#include "config.h"
#include "spi_flash.h"
#include "debug.h"
#include "watchdog.h"
#include "uart.h"

/* SPI Flash commands */
#define SPI_FLASH_READ_ID_CMD		0x9F
#define SPI_FLASH_READ_DATA_CMD		0x03
#define SPI_FLASH_ERASE_BLOCK_CMD	0xD8
#define SPI_FLASH_PAGE_PROGRAM_CMD	0x02
#define SPI_FLASH_READ_STATUS_CMD	0x05
#define SPI_FLASH_WREN_CMD			0x06

#define SPI_FLASH_STATUS_BSY		(1 << 0)

/* Supported SPI Flash types */
struct {
	const char *name;
	uint32_t id;
	uint32_t page_size;
	uint32_t block_size;
	uint32_t total_size;
} spi_flash_table[] = {
	{ "gd25q80", 0xC84014, 256, 64*1024, 16*64*1024 }
};

#define SPI_FLASH_TABLE_SIZE	(int)(sizeof(spi_flash_table)/sizeof(*spi_flash_table))

static struct spi_module spi_master_instance;
static struct spi_slave_inst spi_slave_instance;
static int spi_flash_type = -1;

/* Generic SPI write/read function */
static int spi_flash_xfer(uint8_t *write_buf, int write_len, uint8_t *read_buf, int read_len, int select, int deselect)
{
	int ret = -1;
	
	if (select) {
		spi_select_slave(&spi_master_instance, &spi_slave_instance, true);
	}
	if (write_buf && write_len) {
		ret = (spi_write_buffer_wait(&spi_master_instance, write_buf, write_len) == STATUS_OK) ? 0 : -1;
	}
	if (read_buf && read_len && ret >= 0) {
		ret = (spi_read_buffer_wait(&spi_master_instance, read_buf, read_len, 0) == STATUS_OK) ? 0 : -1;
	}
	if (deselect) {
		spi_select_slave(&spi_master_instance, &spi_slave_instance, false);
	}
	
	return ret;
}

int spi_flash_get_status(uint8_t *pstat)
{
	uint8_t cmd = SPI_FLASH_READ_STATUS_CMD;
	
	if (spi_flash_xfer(&cmd, 1, pstat, 1, 1, 1) < 0) {
		return -1;
	}
	
	return 0;
}

static int spi_flash_wait_ready(void)
{
	uint8_t status;
	int retries = 5000;
	
	while (--retries) {
		WDT_RESET;
		if (spi_flash_get_status(&status) < 0) {
			return -1;
		}
		if (!(status & SPI_FLASH_STATUS_BSY)) {
			return 0;
		}
		delay_ms(1);
	}
	PRINTF("SPI: busy timeout\r\n");
	
	return -1;
}

static int spi_flash_write_enable(void)
{
	uint8_t cmd = SPI_FLASH_WREN_CMD;
	
	return spi_flash_xfer(&cmd, 1, NULL, 0, 1, 1);
}

static int spi_flash_read_id(uint8_t *buf, int len)
{
	uint8_t cmd = SPI_FLASH_READ_ID_CMD;
	
	return spi_flash_xfer(&cmd, 1, buf, len, 1, 1);
}

int spi_flash_read(uint32_t addr, uint8_t *buf, int len)
{
	uint8_t cmd[4] = { SPI_FLASH_READ_DATA_CMD };
	
	if (spi_flash_type < 0) {
		return -1;
	}
	cmd[1] = (addr >> 16) & 0xFF;
	cmd[2] = (addr >> 8) & 0xFF;
	cmd[3] = addr & 0xFF;
	
	return spi_flash_xfer(cmd, sizeof(cmd), buf, len, 1, 1);
}

int spi_flash_erase(uint32_t addr, int len)
{
	uint32_t last = addr + len - 1;
	uint8_t cmd[4] = { SPI_FLASH_ERASE_BLOCK_CMD };
	
	if (spi_flash_type < 0) {
		return -1;
	}
	if (len < 0) {
		/* Erase the entire Flash */
		return spi_flash_erase(0, spi_flash_table[spi_flash_type].total_size);
	}
	while (addr <= last) {
		WDT_RESET;
		if (spi_flash_write_enable() < 0) {
			PRINTF("SPI: write enable failed\r\n");
			return -1;
		}
		cmd[1] = (addr >> 16) & 0xFF;
		cmd[2] = (addr >> 8) & 0xFF;
		cmd[3] = addr & 0xFF;
		if (spi_flash_xfer(cmd, sizeof(cmd), NULL, 0, 1, 1) < 0) {
			PRINTF("SPI: block erase failed @ 0x%08lx\r\n", addr);
			return -1;
		}
		if (spi_flash_wait_ready() < 0) {
			PRINTF("SPI: wait for ready failed\r\n");
			return -1;
		}
		addr += spi_flash_table[spi_flash_type].block_size;
	}
	
	return 0;
}

#ifndef BOOTLOADER

int spi_flash_program(uint32_t addr, uint8_t *buf, int len)
{
	int chunk, ps;
	uint32_t end = addr + len;
	uint8_t cmd[4] = { SPI_FLASH_PAGE_PROGRAM_CMD };
	
	if (spi_flash_type < 0) {
		return -1;
	}
	ps = spi_flash_table[spi_flash_type].page_size;
	while (addr < end) {
		WDT_RESET;
		chunk = ps - (addr & (ps - 1));
		if (addr + chunk >= end) {
			chunk = end - addr;
		}
		debug_printf("spi_flash_program: addr = 0x%06x, chunk = %d\r\n", addr, chunk);
		if (spi_flash_write_enable() < 0) {
			PRINTF("SPI: write enable failed\r\n");
			return -1;
		}
		cmd[1] = (addr >> 16) & 0xFF;
		cmd[2] = (addr >> 8) & 0xFF;
		cmd[3] = addr & 0xFF;
		if (spi_flash_xfer(cmd, sizeof(cmd), NULL, 0, 1, 0) < 0
				|| spi_flash_xfer(buf, chunk, NULL, 0, 0, 1) < 0) {
			PRINTF("SPI: page program failed @ 0x%08lx\r\n", addr);
			return -1;
		}
		if (spi_flash_wait_ready() < 0) {
			PRINTF("SPI: wait for ready failed\r\n");
			return -1;
		}
		addr += chunk;
		buf += chunk;
	}
	
	return 0;
}

#endif /* BOOTLOADER */

int spi_flash_get_block_size(void)
{
	if (spi_flash_type < 0) {
		return -1;
	}
	
	return spi_flash_table[spi_flash_type].block_size;
}

void spi_flash_reset(void)
{
	if (spi_flash_type >= 0) {
		spi_reset(&spi_master_instance);
	}
}

void spi_flash_init(void)
{
	struct spi_config spi_cfg;
	struct spi_slave_inst_config slave_cfg;
	uint8_t buf[3];
	uint32_t idcode;
	int i;

	spi_slave_inst_get_config_defaults(&slave_cfg);
	slave_cfg.ss_pin = CFG_SPI_FLASH_SS_PIN;
	spi_attach_slave(&spi_slave_instance, &slave_cfg);
	spi_get_config_defaults(&spi_cfg);
	spi_cfg.mux_setting = CFG_SPI_FLASH_MUX_SETTING;
	spi_cfg.mode_specific.master.baudrate = CFG_SPI_FLASH_BAUDRATE;
	spi_cfg.pinmux_pad0 = CFG_SPI_FLASH_PINMUX_PAD0;
	spi_cfg.pinmux_pad1 = PINMUX_UNUSED;
	spi_cfg.pinmux_pad2 = CFG_SPI_FLASH_PINMUX_PAD2;
	spi_cfg.pinmux_pad3 = CFG_SPI_FLASH_PINMUX_PAD3;
	spi_init(&spi_master_instance, SERCOM0, &spi_cfg);
	spi_enable(&spi_master_instance);
	spi_flash_read_id(buf, sizeof(buf));
	idcode = (buf[0] << 16) | (buf[1] << 8) | buf[2];
	
	for (i = 0; i < SPI_FLASH_TABLE_SIZE; i++) {
		if (idcode == spi_flash_table[i].id) {
			PRINTF("SPI: Flash device detected: %s\r\n", spi_flash_table[i].name);
			spi_flash_type = i;
			break;
		}
	}
	if (i == SPI_FLASH_TABLE_SIZE) {
		PRINTF("SPI: no Flash device detected (ID = 0x%08lx)\r\n", idcode);
	}
}