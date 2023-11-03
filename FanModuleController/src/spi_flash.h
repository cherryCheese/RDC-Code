/*
 * spi_flash.h
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#ifndef __SPI_FLASH_H__
#define __SPI_FLASH_H__

void spi_flash_init(void);
int spi_flash_read(uint32_t addr, uint8_t *buf, int len);
int spi_flash_erase(uint32_t addr, int len);
int spi_flash_program(uint32_t addr, uint8_t *buf, int len);
int spi_flash_get_status(uint8_t *pstat);
int spi_flash_get_block_size(void);
void spi_flash_reset(void);

#endif /* __SPI_FLASH_H__ */