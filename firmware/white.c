#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/hrtim.h>

#include "white.h"
#include "constants.h"
#include "systick.h"
#include "config.h"
#include "usart.h"

/*
 * Pin mapping
 * -----------
 *
 * Channel 0 (Warm):
 *  - SW: PA8 / HRTIM_CHA1
 *  - PWM: PB4 / TIM3_CH1
 *  - Isense: PA7 / ADC2_IN4
 *  - Vsense: PB1 / ADC1_IN12
 *  - Fault: PA12 / HRTIM_FLT1
 *
 * Channel 1 (Cold):
 *  - SW: PB12 / HRTIM1_CHC1
 *  - PWM: PB5 / TIM3_CH2
 *  - Isense: PB0 / ADC1_IN11
 *  - Vsense: PB2 / ADC2_IN12
 *  - Fault: PA15 / HRTIM_FLT2
 */

#define HRTIM_PERIOD (F_HRTIM_DLL / HRTIM_PWM_FREQ)
#define ADC_TIMER_PERIOD (F_TIM / ADC_FREQ)

static void hrtim_calibrate(void)
{
	HRTIM_ICR |= HRTIM_ICR_DLLRDYC;

	HRTIM_DLLCR |= HRTIM_DLLCR_CAL;

	while (!(HRTIM_ISR & HRTIM_ISR_DLLRDY))
		;

	HRTIM_DLLCR |= HRTIM_DLLCR_CALRTE_1048576 |
	               HRTIM_DLLCR_CALEN;
}

void set_hrtim_channel(uint8_t ch, uint16_t val) {
	uint16_t setval = val;

	/*
	 * When we have simultaneous sets and resets, resets win, thus we clamp
	 * the reset point below the set point (= HRTIM_PERIOD) to avoid a
	 * large value resulting in no output
	 */
	if (val >= HRTIM_PERIOD)
		setval = HRTIM_PERIOD - 1;

	// values below 0x60 are forbidden for CKPSC == 0
	if (val < 0x60)
		setval = 0x60;

	// map zero to HRTIM_PERIOD to achive true disabling
	if (val == 0)
		setval = HRTIM_PERIOD;

	if (ch == 0)
		HRTIM_TIMx_CMP1(HRTIM_TIMA) = setval;
	else
		HRTIM_TIMx_CMP1(HRTIM_TIMC) = setval;
}

static void setup_timer_common(uint8_t tim)
{
	HRTIM_TIMx_TIMCR(tim) =
		HRTIM_TIMx_CR_PREEN |
		HRTIM_TIMx_CR_CONT |
		HRTIM_TIMx_CR_TxREPU;

	//HRTIM_TIMx_OUT(tim) = HRTIM_TIMx_OUT_FAULT1_INACTIVE;

	HRTIM_TIMx_SET1(tim) = HRTIM_TIMx_SETy_PER;
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

	//HRTIM_TIMx_FLT(HRTIM_TIMA) = HRTIM_TIMx_FLT_FLT1EN;
	//HRTIM_TIMx_FLT(HRTIM_TIMC) = HRTIM_TIMx_FLT_FLT2EN;

	set_hrtim_channel(0, 0);
	set_hrtim_channel(1, 0);
	HRTIM_CR2 |= HRTIM_CR2_TASWU | HRTIM_CR2_TCSWU;

	HRTIM_MCR |= HRTIM_MCR_TACEN | HRTIM_MCR_TCCEN;
	HRTIM_OENR |= HRTIM_OENR_TA1OEN | HRTIM_OENR_TC1OEN;
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

static void init_one_adc(uint32_t adc)
{
	adc_enable_regulator(adc);
	delay_ms(1);

	ADC_CR(adc) |= ADC_CR_ADCAL;
	while (ADC_CR(adc) & ADC_CR_ADCAL)
		;

	adc_set_sample_time_on_all_channels(adc, ADC_SMPR_SMP_19DOT5CYC);
	//adc_set_sample_time_on_all_channels(adc, ADC_SMPR_SMP_601DOT5CYC);
}

uint32_t chanval;

void adc1_2_isr(void)
{
	uint32_t val;

	/*
	if (!(ADC1_ISR & ADC_ISR_EOC)) {
		ADC1_ISR = ADC1_ISR;
		return;
	}
	*/

	//usart_puts("ADC1_ISR "); usart_print_hex(ADC1_ISR); usart_puts("\n");
	//usart_puts("ADC2_ISR "); usart_print_hex(ADC2_ISR); usart_puts("\n");

	if (ADC1_ISR & ADC_ISR_EOC) {
		//ADC1_ISR = ADC_ISR_EOC;
		val = *(volatile uint32_t*)(ADC1 + 0x40);
		usart_puts("A");
		usart_print_int(val);
		usart_puts("\n");
	}
	if (ADC2_ISR & ADC_ISR_EOC) {
		//ADC2_ISR = ADC_ISR_EOC;
		val = *(volatile uint32_t*)(ADC2 + 0x40);
		usart_puts("B");
		usart_print_int(val);
		usart_puts("\n");
	}

	/*
	val = *(volatile uint32_t*)(ADC1 + 0x30C);
	usart_puts("C ");
	usart_print_hex(val);
	usart_puts("\n");
	*/
}

static void init_adc(void)
{
	ADC12_CCR |= ADC_CCR_CKMODE_DIV1;

	init_one_adc(ADC1);
	init_one_adc(ADC2);

	ADC1_SQR1 = 11 << ADC_SQR1_SQ1_SHIFT |
	             0 << ADC_SQR1_L_SHIFT;
	ADC2_SQR1 =  4 << ADC_SQR1_SQ1_SHIFT |
	             0 << ADC_SQR1_L_SHIFT;

	// Use simultaneous regular dual conversion mode
//	ADC12_CCR |= 6 << ADC_CCR_DUAL_SHIFT;
	// Triggered by TIM2_TRGO
//	ADC1_CFGR1 |= ADC_CFGR1_EXTSEL_VAL(11) |
//	              ADC_CFGR1_EXTEN_RISING_EDGE;

	nvic_enable_irq(NVIC_ADC1_2_IRQ);

	ADC1_IER |= ADC_IER_EOCIE;
	ADC2_IER |= ADC_IER_EOCIE;

	adc_power_on(ADC1);
	adc_power_on(ADC2);
	delay_ms(1);

//	ADC1_CR |= ADC_CR_ADSTART;

	timer_set_period(TIM2, ADC_TIMER_PERIOD);
	timer_set_master_mode(TIM2, TIM_CR2_MMS_UPDATE);
	timer_enable_counter(TIM2);
}

void white_set_channel(enum white_channel channel, uint16_t val)
{
	// below 250 raw, the gate driver gets bouncy
	if (val <= 250)
		val = 250;

	set_hrtim_channel(channel, val);
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

	//HRTIM_FLTINR1 = HRTIM_FLTINR1_FLTxP(1) | HRTIM_FLTINR1_FLTxP(2);
	//HRTIM_FLTINR1 |= HRTIM_FLTINR1_FLTxE(1) | HRTIM_FLTINR1_FLTxE(2);

	init_adc();
}
