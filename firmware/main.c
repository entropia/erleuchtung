#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/adc.h>
#include <libopencmsis/core_cm3.h>

#include "systick.h"
#include "rgb.h"
#include "white.h"
#include "usart.h"
#include "can.h"

static void init_clocktree(void)
{
	// are we on PLL already? nothing to do
	if ((RCC_CFGR & RCC_CFGR_SWS_MASK) == RCC_CFGR_SWS_PLL)
		return;

	// need 2 wait states for SYSCLK > 48 MHz
	FLASH_ACR |= FLASH_ACR_LATENCY_2WS;

	// init HSE
	//RCC_CR |= RCC_CR_HSEBYP; // HACK: discovery has no quartz
	RCC_CR |= RCC_CR_HSEON;
	while (!(RCC_CR & RCC_CR_HSERDY))
		;

	// init PLL
	// 8 MHz HSE -> 72 MHz PLLCLK/SYSCLK
	RCC_CFGR |= RCC_CFGR_PLLSRC |
	           (RCC_CFGR_PLLMUL_PLL_IN_CLK_X9 << RCC_CFGR_PLLMUL_SHIFT);

	RCC_CR |= RCC_CR_PLLON;
	while (!(RCC_CR & RCC_CR_PLLRDY))
		;

	// PCLK1 must be <= 36 MHz, divide by 2
	RCC_CFGR |= RCC_CFGR_PPRE1_DIV_2 << RCC_CFGR_PPRE1_SHIFT;

	// switch SYSCLK to PLLCLK
	RCC_CFGR |= RCC_CFGR_SW_PLL << RCC_CFGR_SW_SHIFT;

	while (((RCC_CFGR >> RCC_CFGR_SWS_SHIFT) & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL)
		;
}

static void init_periphs(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_reset_pulse(RST_GPIOA);

	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_reset_pulse(RST_GPIOB);

	rcc_periph_clock_enable(RCC_TIM1);
	rcc_periph_reset_pulse(RST_TIM1);

	rcc_periph_clock_enable(RCC_TIM2);
	rcc_periph_reset_pulse(RST_TIM2);

	rcc_periph_clock_enable(RCC_ADC12);
	rcc_periph_reset_pulse(RST_ADC12);

	rcc_periph_clock_enable(RCC_HRTIM);
	rcc_periph_reset_pulse(RST_HRTIM);

	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_reset_pulse(RST_USART1);

	rcc_periph_clock_enable(RCC_CAN);
	rcc_periph_reset_pulse(RST_CAN);
}

void main(void)
{
	SCB_VTOR = 0x8000800;

	init_clocktree();
	init_periphs();

	systick_init();

	usart_init();
	rgb_init();
	white_init();
	can_if_init();

	usart_puts("Erleuchtung booted\n");

	/*
	while (1) {
		rgb_set_raw(RGB_PWM_MAX, 0, 0);
		set_hrtim_channel(0, 3000);
		set_hrtim_channel(1, 0);

		delay_ms(1000);

		rgb_set_raw(0, RGB_PWM_MAX, 0);
		set_hrtim_channel(0, 3000);
		set_hrtim_channel(1, 3000);

		delay_ms(1000);

		rgb_set_raw(0, 0, RGB_PWM_MAX);
		set_hrtim_channel(0, 0);
		set_hrtim_channel(1, 3000);

		delay_ms(1000);
	}*/

	int i;
	i=250;
	while (1) {
		usart_puts("S1\n");
	ADC1_CR |= ADC_CR_ADSTART;
		for (; i <= 4000; i++) {
			white_set_channel(CHANNEL_WARM, i);
			delay_ms(1);
		}
		usart_puts("S2\n");
	ADC1_CR |= ADC_CR_ADSTART;
		for (;i > 250; i--) {
			white_set_channel(CHANNEL_WARM, i);
			delay_ms(1);
		}
		usart_puts("S3\n");
	ADC1_CR |= ADC_CR_ADSTART;

		for (; i <= 4000; i++) {
			white_set_channel(CHANNEL_COLD, i);
			delay_ms(1);
		}
		usart_puts("S4\n");
	ADC1_CR |= ADC_CR_ADSTART;
		for (;i > 250; i--) {
			white_set_channel(CHANNEL_COLD, i);
			delay_ms(1);
		}

	}
}
