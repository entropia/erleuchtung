#pragma once

#include <stdint.h>
#include <stdbool.h>

#define RGB_PWM_FREQ 8000
#define HRTIM_PWM_FREQ 281250
#define ADC_FREQ 1000

#define CFG_KEY_ADDR 0xff00

bool config_test(uint16_t key);
int config_get_u32(uint16_t key, uint32_t *out);
