/*
 * env.c
 *
 * Created: 1/19/2021 6:24:04 PM
 *  Author: E1210640
 */ 

#include <asf.h>
#include <stdio.h>

#include "eeprom_driver.h"
#include "config.h"
#include "modbus.h"
#include "crc.h"
#include "uart.h"
#include "env.h"
#include "watchdog.h"

#ifndef BOOTLOADER

#define ENV_MAX_ENTRIES		128

#define CFG_ENV_DESC(_name, _default) \
	_name,

static const char *env_vars[] = { CFG_ENV_DESCRIPTORS };

#define ENV_SIZE		(sizeof(env_vars)/sizeof(*env_vars))

#undef CFG_ENV_DESC

#define CFG_ENV_DESC(_name, _default) \
	_default,

struct env_cache_s {
	uint32_t magic;
#define ENV_HDR_MAGIC	0x87654321
	uint8_t size;
	uint16_t crc;
	uint32_t data[ENV_MAX_ENTRIES];
} env_cache = { ENV_HDR_MAGIC, ENV_SIZE, 0, { CFG_ENV_DESCRIPTORS }};

static uint8_t env_dirty;

static int env_read(void)
{
	struct env_cache_s eeprom_copy;
	int ret = 0, i;
	
	if (eeprom_read((uint8_t *)&eeprom_copy, CFG_EEPROM_ENV_OFFSET, sizeof(eeprom_copy)) < 0) {
		PRINTF("ERROR: env_read(): failed to read EEPROM\r\n");
		return -1;
	}
	if (eeprom_copy.magic != ENV_HDR_MAGIC) {
		PRINTF("ENV: bad magic number\r\n");
		return -1;
	}
	if (eeprom_copy.size < 1 || eeprom_copy.size >= ENV_MAX_ENTRIES) {
		PRINTF("ENV: invalid size\r\n");
		return -1;
	}
	uint16_t crc = crc16(0, (const uint8_t *)eeprom_copy.data, eeprom_copy.size*sizeof(uint32_t), 0x1021);
	if (crc != eeprom_copy.crc) {
		PRINTF("ENV: bad CRC\r\n");
		return -1;
	}
	PRINTF("ENV: EEPROM copy is valid, restoring\r\n");
	if (eeprom_copy.size < env_cache.size) {
		PRINTF("WARNING: saved environment is shorter than expected, using defaults for the remaining variables\r\n");
		ret = 1;
	} else if (eeprom_copy.size > env_cache.size) {
		PRINTF("WARNING: saved environment is longer than expected, ignoring extra values\r\n");
		ret = 1;
	}
	for (i = 0; i < eeprom_copy.size; i++) {
		env_cache.data[i] = eeprom_copy.data[i];
	}
	
	return ret;
}

static void env_save(void) {
	env_cache.magic = ENV_HDR_MAGIC;
	env_cache.size = ENV_SIZE;
	env_cache.crc = crc16(0, (const uint8_t *)env_cache.data, ENV_SIZE*sizeof(uint32_t), 0x1021);
	if (eeprom_write((uint8_t *)&env_cache, CFG_EEPROM_ENV_OFFSET, sizeof(env_cache)) < 0) {
		PRINTF("ERROR: env_save(): failed to write to EEPROM\r\n");
	}
}

void env_reset(void) {
	PRINTF("ENV: resetting to default environment\r\n");
	env_cache.magic = 0;
	if (eeprom_write((uint8_t *)&env_cache, CFG_EEPROM_ENV_OFFSET, sizeof(env_cache)) < 0) {
		PRINTF("ERROR: env_save(): failed to write to EEPROM\r\n");
	}
	SYSTEM_RESET;
	/* NOTREACHED */
}

void env_init(void)
{
	if (env_read()) {
		PRINTF("ENV: saving default environment\r\n");
		env_save();
	}
}

int env_find(const char *var)
{
	int i;
	
	for (i = 0; i < (int)ENV_SIZE; i++) {
		if (!strcmp(var, env_vars[i])) {
			return i;
		}
	}
	
	return -1;
}

int env_set(const char *var, uint32_t val)
{
	int idx = env_find(var);
	if (idx < 0) {
		PRINTF("ENV: variable %s not found\r\n", var);
		return -1;
	}
	env_cache.data[idx] = val;
	env_dirty = 1;
	
	return 0;
}

uint32_t env_get(const char *var)
{
	int idx = env_find(var);
	if (idx < 0) {
		PRINTF("ENV: variable %s not found\r\n", var);
		return 0;
	}
	
	return env_cache.data[idx];
}

void env_print_all(void)
{
	int i;
	
	for (i = 0; i < (int)ENV_SIZE; i++) {
		PRINTF("%s = %lu\r\n", env_vars[i], env_cache.data[i]);
	}
}

void do_env(void)
{
	if (env_dirty) {
		env_save();
		env_dirty = 0;
	}
}

#endif /* BOOTLOADER */