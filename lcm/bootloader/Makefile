# LCM bootloader (1 KB at 0x08000000), arm-none-eabi-gcc.
#   make -> build/lcm_bootloader.hex
L      := ../alcm/Library/HK32F030Mxx_Library_V1.1.6
CC     := arm-none-eabi-gcc
ARCH   := -mcpu=cortex-m0 -mthumb
INC    := -I$(L)/HK32F030M/HK32F030M_Lib/inc -I$(L)/HK32F030M/CMSIS/CM0/Core \
          -I$(L)/HK32F030M/CMSIS/HK32F030M/Include -I$(L)/HK32F030M_Project/inc
CFLAGS := $(ARCH) -Oz -std=gnu99 -ffunction-sections -fdata-sections -flto -Wall \
          -Wno-unused-parameter -DHK32F030M -DHK32F030MF4P6 $(INC)
LDFLAGS:= $(ARCH) -Oz -flto -nostartfiles -nostdlib -Wl,--gc-sections \
          -Wl,-Map=build/lcm_bootloader.map -T bootloader.ld -lgcc
SRC    := bootloader.c

all: build/lcm_bootloader.hex

build/lcm_bootloader.elf: $(SRC) bootloader.ld
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $@
	arm-none-eabi-size $@

build/lcm_bootloader.hex: build/lcm_bootloader.elf
	arm-none-eabi-objcopy -O ihex $< $@

clean:
	rm -rf build
.PHONY: all clean
