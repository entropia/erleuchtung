#include <stdint.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

#include "usart.h"

void usart_init(void)
{
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10);
	gpio_set_af(GPIOA, GPIO_AF7, GPIO9 | GPIO10);

	USART1_BRR = 313; // 36 MHz / 313 = ~115200 Bd
	USART1_CR1 |= USART_CR1_TE | USART_CR1_RE;
	USART1_CR1 |= USART_CR1_UE;
}

void usart_putc(char c)
{
	usart_send_blocking(USART1, c);
}

void usart_puts(const char *s)
{
	while (*s) {
		if (*s == '\n')
			usart_putc('\r');

		usart_putc(*s);

		s++;
	}
}

void usart_print_int(uint32_t i)
{
	char buf[16];
	char *p = buf + sizeof(buf) - 1;

	*p-- = 0;

	do {
		*p-- = '0' + (i % 10);
		i /= 10;
	} while(i);

	usart_puts(p + 1);
}
