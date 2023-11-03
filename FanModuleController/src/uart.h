/*
 * uart.h
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#ifndef __UART_H__
#define __UART_H__

extern int quiet;

#ifndef BOOTLOADER
#define PRINTF(args...) \
	if (!quiet) { \
		printf(args); \
	}
#else
#define PRINTF(args...) printf(args)
#endif

int uart_init(void);
void uart_putc(int chan, char data);
void uart_puts(int chan, const char *str);
void uart_write(int chan, const uint8_t *buf, int len);
int uart_gets(int chan, char *buf, int maxlen);
void uart_reset(int chan);
void uart_set_baud_rate(int chan, int baud);

#endif /* __UART_H__ */