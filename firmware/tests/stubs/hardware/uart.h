#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define UART_PARITY_NONE 0
typedef struct { uint8_t rx[128], tx[512]; size_t rx_head, rx_count, tx_count; bool writable; } uart_inst_t;
extern uart_inst_t fake_uart[2];
#define uart0 (&fake_uart[0])
#define uart1 (&fake_uart[1])
void uart_set_format(uart_inst_t *port, unsigned bits, unsigned stop, unsigned parity);
void uart_set_hw_flow(uart_inst_t *port, bool cts, bool rts);
bool uart_is_writable(uart_inst_t *port);
bool uart_is_readable(uart_inst_t *port);
void uart_putc_raw(uart_inst_t *port, uint8_t byte);
uint8_t uart_getc(uart_inst_t *port);
