/*
 * upgrade.h
 *
 * Created: 8/14/2020 3:50:55 PM
 *  Author: E1210640
 */ 


#ifndef __UPGRADE_H__
#define __UPGRADE_H__

int upgrade_start(void);
int upgrade_activate(void);
int upgrade_parse_ihex(char *buf);
int upgrade_write_data(uint32_t addr, uint8_t *buf, int len);
int upgrade_verify(void);
int upgrade_copy_to_nvm(void);

#endif /* __UPGRADE_H__ */