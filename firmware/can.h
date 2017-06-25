#pragma once

#include <stdint.h>

struct can_msg {
	uint32_t id;
	uint8_t len;
	uint8_t data[8];
};

void can_if_init(void);
void can_recv(struct can_msg *msg);
int can_send(struct can_msg *msg);
