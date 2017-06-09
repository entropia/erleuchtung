#pragma once

#include <stdint.h>

void usart_init(void);
void usart_putc(char c);
void usart_puts(const char *s);
void usart_print_int(uint32_t i);
