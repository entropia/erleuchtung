#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

#include "constants.h"
#include "config.h"
#include "rgb.h"

/*
 * Channel mapping:
 *
 *  Red -> PA11 / TIM1_CH4
 *  Green -> PB14 / TIM1_CH2N
 *  Blue -> PB15 / TIM1_CH3N
 */

void rgb_set_raw(uint16_t r, uint16_t g, uint16_t b)
{
	timer_set_oc_value(TIM1, TIM_OC4, r);
	timer_set_oc_value(TIM1, TIM_OC2N, g);
	timer_set_oc_value(TIM1, TIM_OC3N, b);
}

void rgb_init(void)
{
	timer_set_period(TIM1, RGB_PWM_MAX);

	timer_set_oc_mode(TIM1, TIM_OC2N, TIM_OCM_PWM1);
	timer_enable_oc_output(TIM1, TIM_OC2N);

	timer_set_oc_mode(TIM1, TIM_OC3N, TIM_OCM_PWM1);
	timer_enable_oc_output(TIM1, TIM_OC3N);

	timer_set_oc_mode(TIM1, TIM_OC4, TIM_OCM_PWM1);
	timer_enable_oc_output(TIM1, TIM_OC4);

	rgb_set_raw(0, 0, 0);

	timer_enable_counter(TIM1);

	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11);
	gpio_set_af(GPIOA, GPIO_AF6, GPIO11);

	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO14 | GPIO15);
	gpio_set_af(GPIOB, GPIO_AF6, GPIO14);
	gpio_set_af(GPIOB, GPIO_AF4, GPIO15);
}
