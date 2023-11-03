/*
 * uart.c: UART driver
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#include <asf.h>
#include <string.h>

#include "config.h"
#include "ring_buffer.h"
#include "uart.h"
#include "modbus.h"

#define CFG_UART_CHANNEL(_chan, _sercom, _baud, _parity, _mux_setting, _pinmux_pad0, _pinmux_pad1, _pinmux_pad2, _pinmux_pad3) \
{ \
	_sercom, \
	_baud, \
	_parity, \
	_mux_setting, \
	_pinmux_pad0, \
	_pinmux_pad1, \
	_pinmux_pad2, \
	_pinmux_pad3 \
},

/* Configuration settings for all defined UART channels */
struct {
	Sercom *sercom;
	int baud;
	int parity;
	int mux_setting;
	int pinmux_pad0;
	int pinmux_pad1;
	int pinmux_pad2;
	int pinmux_pad3;
} uart_config[] = { CFG_UART_CHANNELS };

#undef CFG_UART_CHANNEL
#define CFG_UART_CHANNEL(_chan, ...) \
	static RING_BUFFER(uart_ring_##_chan, CFG_UART_RING_SIZE);

/* UART input buffers (one for each defined channel) */
CFG_UART_CHANNELS

#undef CFG_UART_CHANNEL
#define CFG_UART_CHANNEL(_chan, ...) \
	{ \
		(struct ring_buffer *)&uart_ring_##_chan, \
	},

/* Dynamic UART data (input buffer, USART instance, current input character) */
struct {
	struct ring_buffer *ring;
	struct usart_module usart_instance;
	uint16_t current_char;
} uart_data[] = { CFG_UART_CHANNELS };

#define UART_CHANNELS (int)(sizeof(uart_config)/sizeof(*uart_config))

#ifndef BOOTLOADER

int quiet;

static int uart_find_channel(struct usart_module *mod)
{
	int chan;
	
	for (chan = 0; chan < UART_CHANNELS; chan++) {
		if (&uart_data[chan].usart_instance == mod) {
			return chan;
		}
	}
	
	return -1;
}

/* UART callback: called by the ASF driver when a new character is received */
static void uart_callback(struct usart_module *const mod)
{
	int chan = uart_find_channel(mod);
	
	if (chan < 0) {
		return;
	}
#ifdef CFG_MODBUS_CHANNEL
	if (chan == CFG_MODBUS_CHANNEL) {
		modbus_receive((uint8_t)uart_data[chan].current_char);
	} else {
		ring_put(uart_data[chan].ring, uart_data[chan].current_char);
	}
#else
	/* Store the newly-received character in the input buffer */
	ring_put(uart_data[chan].ring, uart_data[chan].current_char);
#endif
	/* Prepare for receiving next character */
	usart_read_job((struct usart_module *const)mod, &uart_data[chan].current_char);
}

int uart_gets(int chan, char *buf, int maxlen)
{
	struct ring_buffer *ring = uart_data[chan].ring;
	int ret;

	system_interrupt_enter_critical_section();
	ret = ring_get_buf(ring, (uint8_t *)buf, maxlen);
	system_interrupt_leave_critical_section();
	
	return ret;
}

#endif /* BOOTLOADER */

void uart_putc(int chan, char data)
{
	struct usart_module *mod = &uart_data[chan].usart_instance;
	
	usart_write_wait(mod, data);
}

void uart_puts(int chan, const char *str)
{
	uart_write(chan, (const uint8_t *)str, strlen(str));
}

void uart_write(int chan, const uint8_t *buf, int len)
{
	struct usart_module *mod = &uart_data[chan].usart_instance;
		
	usart_write_buffer_wait(mod, buf, len);
}

static void uart_init_channel(int chan, int baud)
{
	struct usart_config cfg;
	struct usart_module *mod;
	
	mod = &uart_data[chan].usart_instance;
	usart_get_config_defaults(&cfg);
	cfg.baudrate = baud;
	cfg.parity = uart_config[chan].parity;
	cfg.mux_setting = uart_config[chan].mux_setting;
	cfg.pinmux_pad0	= uart_config[chan].pinmux_pad0;
	cfg.pinmux_pad1	= uart_config[chan].pinmux_pad1;
	cfg.pinmux_pad2	= uart_config[chan].pinmux_pad2;
	cfg.pinmux_pad3	= uart_config[chan].pinmux_pad3;
	while (usart_init((struct usart_module *const)mod, (Sercom *const)uart_config[chan].sercom, &cfg) != STATUS_OK)
		;
	usart_enable(mod);
	if (chan == CFG_CONSOLE_CHANNEL) {
		stdio_serial_init(mod, uart_config[chan].sercom, &cfg);
	}
#ifndef BOOTLOADER
	usart_register_callback(mod, uart_callback, USART_CALLBACK_BUFFER_RECEIVED);
	usart_enable_callback(mod, USART_CALLBACK_BUFFER_RECEIVED);
	usart_read_job((struct usart_module *const)mod, &uart_data[chan].current_char);
#endif
}

int uart_init(void)
{
	int chan;
	
	for (chan = 0; chan < UART_CHANNELS; chan++) {
		uart_init_channel(chan, uart_config[chan].baud);
	}
	
	return 0;
}

void uart_set_baud_rate(int chan, int baud)
{
	uart_reset(chan);
	uart_init_channel(chan, baud);
}

void uart_reset(int chan)
{
	usart_reset(&uart_data[chan].usart_instance);
}