TOPDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

OBJ = main.o systick.o

CROSS_PREFIX ?= arm-none-eabi-
CC=$(CROSS_PREFIX)gcc
OBJCOPY=$(CROSS_PREFIX)objcopy
SIZE=$(CROSS_PREFIX)size

CFLAGS = -Wall -Wextra -std=gnu99 -I../libopencm3/include -DSTM32F3 -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mthumb -fno-common -ffreestanding -Og -g
LDFLAGS = -lopencm3_stm32f3 -L../libopencm3/lib -mthumb -mcpu=cortex-m4 -nostartfiles -Wl,--gc-sections -flto -T linker.ld -g

all: firmware.bin

firmware.elf: $(OBJ)
	$(CC) -o firmware.elf $^ $(LDFLAGS)
	$(SIZE) -B firmware.elf

%.bin: %.elf
	$(OBJCOPY) -O binary $^ $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f *.o firmware.elf firmware.bin

.PHONY: all clean
