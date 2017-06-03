#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/hrtim.h>

#include "white.h"
#include "constants.h"
#include "config.h"

/*
 * Pin mapping
 * -----------
 *
 * Channel 0:
 *  - SW: PA8 / HRTIM_CHA1
 *  - PWM: PB4 / TIM3_CH1
 *  - Isense: PA7 / ADC2_IN4
 *  - Vsense: PB1 / ADC1_IN12
 *  - Fault: PA12 / HRTIM_FLT1
 *
 * Channel 1:
 *  - SW: PB12 / HRTIM1_CHC1
 *  - PWM: PB5 / TIM3_CH2
 *  - Isense: PB0 / ADC1_IN11
 *  - Vsense: PB2 / ADC2_IN12
 *  - Fault: PA15 / HRTIM_FLT2
 */

#define HRTIM_PERIOD (F_HRTIM_DLL / HRTIM_PWM_FREQ)

static void hrtim_calibrate(void)
{
	HRTIM_ICR |= HRTIM_ICR_DLLRDYC;

	HRTIM_DLLCR |= HRTIM_DLLCR_CAL;

	while (!(HRTIM_ISR & HRTIM_ISR_DLLRDY))
		;

	HRTIM_DLLCR |= HRTIM_DLLCR_CALRTE_1048576 |
	               HRTIM_DLLCR_CALEN;
}

static void set_hrtim_channel(uint8_t ch, uint16_t val)
{
	/*
	 * When we have simultaneous sets and resets, resets win, thus we clamp
	 * the reset point below the set point (= HRTIM_PERIOD) to avoid a
	 * large value resulting in no output
	 */
	if (val >= HRTIM_PERIOD)
		val = HRTIM_PERIOD - 1;

	// values below 0x60 are forbidden for CKPSC == 0
	if (val < 0x60)
		val = 0x60;

	if (ch == 0)
		HRTIM_TIMx_CMP1(HRTIM_TIMA) = val;
	else
		HRTIM_TIMx_CMP1(HRTIM_TIMC) = val;
}

static void setup_timer_common(uint8_t tim)
{
	HRTIM_TIMx_TIMCR(tim) =
		HRTIM_TIMx_CR_PREEN |
		HRTIM_TIMx_CR_CONT |
		HRTIM_TIMx_CR_TxREPU;

	HRTIM_TIMx_OUT(tim) = HRTIM_TIMx_OUT_FAULT1_INACTIVE;

	HRTIM_TIMx_SET1(tim) = HRTIM_TIMx_SETy_UPDATE;
	HRTIM_TIMx_RST1(tim) = HRTIM_TIMx_RSTy_CMP1;

	HRTIM_TIMx_PER(tim) = HRTIM_PERIOD;
}

static void init_hrtim(void)
{
	// set HRTIM clock source to PLL
	RCC_CFGR3 |= 1 << 12;

	hrtim_calibrate();

	setup_timer_common(HRTIM_TIMA);
	setup_timer_common(HRTIM_TIMC);

	HRTIM_TIMx_FLT(HRTIM_TIMA) = HRTIM_TIMx_FLT_FLT1EN;
	HRTIM_TIMx_FLT(HRTIM_TIMC) = HRTIM_TIMx_FLT_FLT2EN;

	set_hrtim_channel(0, 110);
	set_hrtim_channel(1, 100);
	HRTIM_CR2 |= HRTIM_CR2_TASWU | HRTIM_CR2_TCSWU;

	HRTIM_MCR |= HRTIM_MCR_TACEN | HRTIM_MCR_TCCEN;
}

static void init_gpio(void)
{
	// keep dimming outputs as simple GPIOs for the moment
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8 | GPIO12 | GPIO15);
	//gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO4 | GPIO5 | GPIO12);
	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO12);

	gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO7);
	gpio_mode_setup(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO0 | GPIO1 | GPIO2);

	gpio_set_af(GPIOA, GPIO_AF13, GPIO8 | GPIO12 | GPIO15);
	//gpio_set_af(GPIOB, GPIO_AF2, GPIO4 | GPIO5);
	gpio_set_af(GPIOB, GPIO_AF13, GPIO12);

	// statically activate dimming outputs for now
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO4 | GPIO5);
	gpio_set(GPIOB, GPIO4 | GPIO5);
}

static void init_adc(void)
{
	
}

void white_init(void)
{
	init_hrtim();
	init_gpio();

	/*
	 * Setup fault inputs.
	 *
	 * We do this after the other initialization because we want HRTIM to
	 * actually be driving the outputs before handing over GPIO control to
	 * it. However, we also don't want to enable fault inputs before having
	 * set up the GPIOs properly, to prevent bogus faul inputs, so we do it
	 * now.
	 */

	HRTIM_FLTINR1 = HRTIM_FLTINR1_FLTxP(1) | HRTIM_FLTINR1_FLTxP(2);
	HRTIM_FLTINR1 |= HRTIM_FLTINR1_FLTxE(1) | HRTIM_FLTINR1_FLTxE(2);

	init_adc();
}
