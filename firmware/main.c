#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/flash.h>

#include "systick.h"
#include "rgb.h"

static void init_clocktree(void)
{
	// are we on PLL already? nothing to do
	if ((RCC_CFGR & RCC_CFGR_SWS_MASK) == RCC_CFGR_SWS_PLL)
		return;

	// need 2 wait states for SYSCLK > 48 MHz
	FLASH_ACR |= FLASH_ACR_LATENCY_2WS;

	// init HSE
	RCC_CR |= RCC_CR_HSEBYP; // HACK: discovery has no quartz
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

	rcc_periph_clock_enable(RCC_TIM15);
	rcc_periph_reset_pulse(RST_TIM15);

	rcc_periph_clock_enable(RCC_HRTIM);
	rcc_periph_reset_pulse(RST_HRTIM);
}

void main(void)
{
	init_clocktree();
	init_periphs();

	systick_init();

	rgb_init();
	white_init();
}
