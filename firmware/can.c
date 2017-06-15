#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/can.h>

#include "can.h"
#include "config.h"

/*
  Identifier format:

   2    2   2   1   1
   8    4   0   6   2   8   4    0
   00 dddddddddddd sssssssssss ppp directed frame
   01 iiiiiiiiiiii iiiiiiiiiii iii event frame
   1. ............. ............ ... bootloader frame

   d: destination (12 bit)
   s: source (12 bit)
   p: port (3 bit)

   i: event id (27 bit)

   Only extended identfiers are used.
   The all-ones destination is a broadcast.
*/

// avoid collision with libopencm3
void can_if_init(void)
{
	uint32_t id;

	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8 | GPIO9);
	gpio_set_af(GPIOB, GPIO_AF9, GPIO8 | GPIO9);

	// configure CAN
	CAN_MCR(BX_CAN_BASE) &= ~CAN_MCR_SLEEP;
	CAN_MCR(BX_CAN_BASE) |= CAN_MCR_INRQ;

	while (!(CAN_MSR(BX_CAN_BASE) & CAN_MSR_INAK))
		;

	// BPR = 18 -> Tq = 500 ns at 36MHz
	CAN_BTR(BX_CAN_BASE) = CAN_BTR_SJW_2TQ | CAN_BTR_TS1_9TQ | CAN_BTR_TS2_6TQ | 17;

	// set receive filters
	CAN_FMR(BX_CAN_BASE) |= CAN_FMR_FINIT;
	CAN_FS1R(BX_CAN_BASE) |= 0x3; // 32bit filters

	// filter 0: broadcast
	CAN_FiR1(BX_CAN_BASE, 0) = 0x07ff8000 << CAN_RIxR_EXID_SHIFT | CAN_RIxR_IDE;
	CAN_FiR2(BX_CAN_BASE, 0) = 0x1fff8000;
	CAN_FA1R(BX_CAN_BASE) |= 1 << 0; // enable it

	if (config_get_u32(CFG_KEY_ADDR, &id) == 0) {
		uint32_t filter;

		// filter 1: our ID
		filter = id << 15;
		CAN_FiR1(BX_CAN_BASE, 1) = filter << CAN_RIxR_EXID_SHIFT | CAN_RIxR_IDE;
		CAN_FiR2(BX_CAN_BASE, 1) = 0x1fff8000;
		CAN_FA1R(BX_CAN_BASE) |= 1 << 1;

		// set transmit address
		CAN_TI0R(BX_CAN_BASE) = id << CAN_TIxR_EXID_SHIFT | CAN_TIxR_IDE;
	}

	CAN_FMR(BX_CAN_BASE) &= ~CAN_FMR_FINIT;

	CAN_MCR(BX_CAN_BASE) &= ~CAN_MCR_INRQ;
	while ((CAN_MSR(BX_CAN_BASE) & CAN_MSR_INAK))
		;
}

void can_recv(struct can_msg *msg)
{

}
