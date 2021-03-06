#include <string.h>

#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/can.h>

#include "can.h"
#include "config.h"
#include "systick.h"

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

#define CAN_TX_TIMEOUT 1000

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

	// enable reception interrupt
	CAN_IER(BX_CAN_BASE) |= CAN_IER_FMPIE0;
	nvic_enable_irq(NVIC_USB_LP_CAN1_RX0_IRQ);

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
	uint32_t h, l;
	while (!(CAN_RF0R(BX_CAN_BASE) & CAN_RF0R_FMP0_MASK))
		;

	msg->len = CAN_RDT0R(BX_CAN_BASE) & CAN_RDTxR_DLC_MASK;
	msg->id = CAN_RI0R(BX_CAN_BASE) >> 3;

	h = CAN_RDH0R(BX_CAN_BASE);
	l = CAN_RDL0R(BX_CAN_BASE);
	memcpy(&msg->data[0], &l, 4);
	memcpy(&msg->data[4], &h, 4);

	CAN_RF0R(BX_CAN_BASE) |= CAN_RF0R_RFOM0;
	while (CAN_RF0R(BX_CAN_BASE) & CAN_RF0R_RFOM0)
		;
}

int can_send(struct can_msg *msg)
{
	uint32_t timeout = ticks + CAN_TX_TIMEOUT * HZ;
	uint32_t h, l;

	memcpy(&l, &msg->data[0], 4);
	memcpy(&h, &msg->data[4], 4);

	CAN_TDT0R(BX_CAN_BASE) &= ~CAN_TDTxR_DLC_MASK;
	CAN_TDT0R(BX_CAN_BASE) |= msg->len;
	CAN_TDH0R(BX_CAN_BASE) = h;
	CAN_TDL0R(BX_CAN_BASE) = l;
	CAN_TI0R(BX_CAN_BASE) = (msg->id << 3) | CAN_TIxR_IDE;

	CAN_TI0R(BX_CAN_BASE) |= CAN_TIxR_TXRQ;

	while (CAN_TI0R(BX_CAN_BASE) & CAN_TIxR_TXRQ) {
		if (time_after(ticks, timeout)) {
			CAN_TSR(BX_CAN_BASE) |= CAN_TSR_ABRQ0;
			while (CAN_TSR(BX_CAN_BASE) & CAN_TSR_ABRQ0)
				;

			return -1;
		}
	}

	return 0;
}

void usb_lp_can1_rx0_isr(void)
{
	struct can_msg msg;

	can_recv(&msg);

	if (msg.len > 0 && msg.data[0] == 0x42)
		scb_reset_system();
}
