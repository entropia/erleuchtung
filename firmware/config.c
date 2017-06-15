#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "config.h"

// flash base + 2K bootloader + 28K main firmware + 2K config
#define CONFIG_START (0x8000000 + 2048 + 1024 * 28)
#define CONFIG_SIZE 2048

static void *config_get(uint16_t key, uint16_t *len)
{
	uint8_t *p = (uint8_t*)CONFIG_START;

	// check magic
	if (memcmp(p, "conf", 4))
		return NULL;

	p += 4;

	// limit the iteration so we don't fault because of invalid data
	while (p - (uint8_t*)CONFIG_START < CONFIG_SIZE) {
		uint16_t cur_key, cur_len;

		cur_key = *(uint16_t*)p;
		p += 2;

		cur_len = *(uint16_t*)p;
		p += 2;

		// did we reach the end?
		if (cur_key == 0xFFFF)
			return NULL;

		if (cur_key != key) {
			p += cur_len;
			continue;
		}

		// found it
		if (len)
			*len = cur_len;

		return p;
	}

	return NULL;
}

bool config_test(uint16_t key)
{
	return config_get(key, NULL) != NULL;
}

int config_get_u32(uint16_t key, uint32_t *out)
{
	void *p;

	p = config_get(key, NULL);
	if (!p)
		return -1;

	*out = *(uint32_t*)p;
	return 0;
}
