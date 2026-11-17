# LCM bootloader (1 KB at 0x08000000), arm-none-eabi-gcc.
#   make -> build/lcm_bootloader.hex
L      := ../alcm/Library/HK32F030Mxx_Library_V1.1.6
CC     := arm-none-eabi-gcc
ARCH   := -mcpu=cortex-m0 -mthumb
INC    := -I$(L)/HK32F030M/HK32F030M_Lib/inc -I$(L)/HK32F030M/CMSIS/CM0/Core \
          -I$(L)/HK32F030M/CMSIS/HK32F030M/Include -I$(L)/HK32F030M_Project/inc
CFLAGS := $(ARCH) -Oz -I. -std=gnu99 -ffunction-sections -fdata-sections -flto -Wall \
          -Wno-unused-parameter -DHK32F030M -DHK32F030MF4P6 $(INC)
LDFLAGS:= $(ARCH) -Oz -flto -nostartfiles -nostdlib -Wl,--gc-sections \
          -Wl,-Map=build/lcm_bootloader.map -T bootloader.ld -lgcc
SRC    := bootloader.c

all: build/lcm_bootloader.hex

build/lcm_bootloader.elf: $(SRC) bl_hw.h bootloader.ld
	@mkdir -p build
	sed 's/LENGTH = 0x400/LENGTH = 0x1000/' bootloader.ld > build/measure.ld
	$(CC) $(CFLAGS) $(SRC) $(subst bootloader.ld,build/measure.ld,$(LDFLAGS)) -o build/measure.elf
	arm-none-eabi-size -A build/measure.elf
	arm-none-eabi-nm --size-sort -S build/measure.elf
	$(CC) $(subst -flto,-fno-lto,$(CFLAGS)) -fno-inline-functions -c bootloader.c -o build/nolto.o
	arm-none-eabi-nm --size-sort -S build/nolto.o
	arm-none-eabi-objdump -d build/measure.elf > build/measure.lst
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $@
	arm-none-eabi-size $@
	arm-none-eabi-nm --size-sort -S $@
	@test $$(arm-none-eabi-size -A $@ | awk '/^\.isr_vector|^\.text/ {s+=$$2} END {print s}') -le 1024 \
	  || (echo "bootloader over 1024 bytes"; exit 1)

build/lcm_bootloader.hex: build/lcm_bootloader.elf
	arm-none-eabi-objcopy -O ihex $< $@

test:
	sh tests/run.sh

clean:
	rm -rf build
.PHONY: all clean test
