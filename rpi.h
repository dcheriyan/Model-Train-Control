#ifndef _rpi_h_
#define _rpi_h_ 1

#include <stdint.h>
#include <stddef.h>

#define CONSOLE 1
#define MARKLIN 2

void uart_putc(size_t line, unsigned char c);
unsigned char uart_getc(size_t line);
void uart_putl(size_t line, const char *buf, size_t blen);
void uart_puts(size_t line, const char *buf);
void uart_printf(size_t line, char *fmt, ...);
void uart_config_and_enable_console();
void uart_config_and_enable_marklin();
void uart_init();
uint32_t query_gpio_func(uint32_t pin);
uint32_t query_gpio_puppdn(uint32_t pin);

#define UART0_B 0xFE201000
#define UART3_B 0xFE201600

#endif /* rpi.h */
