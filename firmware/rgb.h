#pragma once

#include <stdint.h>

#include "constants.h"
#include "config.h"

#define RGB_PWM_MAX (F_TIM / RGB_PWM_FREQ)

void rgb_init(void);
void rgb_set_raw(uint16_t r, uint16_t g, uint16_t b);

