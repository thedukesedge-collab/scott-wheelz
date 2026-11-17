# GCC (arm-none-eabi) build of ALCM for the HK32F030M LCM, an alternative to
# the Keil MDK project in Project/MDK5. Re-Float Rink-Mixed adds
# VP_LIGHT_CONTROL (lights set from the VESC package, button = power only).
#
#   make            -> build/ALCM_VP.hex (VP build)
#   make VP=0       -> build/ALCM.hex (stock behavior)

L      := ../Library/HK32F030Mxx_Library_V1.1.6
APP    := $(L)/HK32F030M_Project/src
CC     := arm-none-eabi-gcc
OBJCOPY:= arm-none-eabi-objcopy
SIZE   := arm-none-eabi-size
VP     ?= 1
# BL=1: runs after the LCM bootloader (lcm/bootloader): linked at 0x08000600,
# up to 0x08003F80 (the battery LUT page stays out of reach of updates)
BL     ?= 0
NAME   := $(if $(filter 1,$(VP)),ALCM_VP,ALCM)$(if $(filter 1,$(BL)),_BL_app)

SRC := startup_hk32f030m.c \
  $(addprefix $(APP)/, hk32f030m_it.c main.c event_queue.c timer.c button_driver.c \
    button_events.c board_mode.c power.c power_hw.c buzzer.c tim1.c headlights.c \
    buzzer_hw.c footpads.c footpads_hw.c status_leds.c status_leds_hw.c headlights_hw.c \
    button_driver_hw.c vesc_serial.c vesc_serial_hw.c battery_lut.c battery_lut_hw.c \
    ring_buffer.c crc16_ccitt.c animations.c settings.c eeprom.c command_processor.c \
    function_generator.c hysteresis.c interrupts_hw.c tiny_rt.c) \
  $(L)/HK32F030M/CMSIS/HK32F030M/Source/system_hk32f030m.c \
  $(addprefix $(L)/HK32F030M/HK32F030M_Lib/src/, hk32f030m_rcc.c hk32f030m_gpio.c \
    hk32f030m_exti.c hk32f030m_misc.c hk32f030m_syscfg.c hk32f030m_tim.c hk32f030m_adc.c \
    hk32f030m_usart.c hk32f030m_flash.c)
ASM := ws2812.S

INC := -I$(L)/HK32F030M/HK32F030M_Lib/inc -I$(L)/HK32F030M/CMSIS/CM0/Core \
       -I$(L)/HK32F030M/CMSIS/HK32F030M/Include -I$(L)/HK32F030M_Project/inc
DEF := -DHK32F030M -DHK32F030MF4P6 $(if $(filter 1,$(VP)),-DVP_LIGHT_CONTROL) \
       $(if $(filter 1,$(BL)),-DLCM_BOOTLOADER -DVECT_TAB_OFFSET=0x600)
LDS := $(if $(filter 1,$(BL)),build/app_bl.ld,hk32f030m.ld)

# firmware name sent in every LCM poll (VP builds): "VPBL-<lcm source hash>"
# with the bootloader, "VP-<hash>" without. The hash is git's tree hash of
# lcm/alcm, so it only changes when the LCM code changes; the package build
# labels its embedded LCM image with the same hash (lcm_images.py).
LCM_HASH ?= $(shell git rev-parse --short=7 HEAD:lcm/alcm 2>/dev/null || echo 0000000)
LCM_NAME := $(if $(filter 1,$(BL)),VPBL,VP)-$(LCM_HASH)
TAG_H := build/lcm_tag_$(if $(filter 1,$(BL)),bl,vp).h
CFLAGS_TAG := $(if $(filter 1,$(VP)),-include $(TAG_H))
ARCH := -mcpu=cortex-m0 -mthumb
CFLAGS = $(ARCH) -Oz -std=gnu99 -ffunction-sections -fdata-sections -flto \
          -fno-common -Wall -Wno-unused-parameter $(DEF) $(INC) $(CFLAGS_TAG)
LDFLAGS := $(ARCH) -Oz -flto -nostartfiles -specs=nano.specs -specs=nosys.specs \
           -Wl,--gc-sections -Wl,-Map=build/$(NAME).map -T $(LDS)

all: build/$(NAME).hex

# measure first with an oversized flash region, so the real size and the
# biggest functions are printed even when it doesn't fit
build/app_bl.ld: hk32f030m.ld
	@mkdir -p build
	sed 's/ORIGIN = 0x08000000, LENGTH = 0x3FC0/ORIGIN = 0x08000600, LENGTH = 0x3980/' $< > $@
	grep -q 'ORIGIN = 0x08000600, LENGTH = 0x3980' $@

$(TAG_H): FORCE
	@mkdir -p build
	python3 ../../tools/lcm_tag.py $(LCM_NAME) $@.tmp
	cmp -s $@.tmp $@ || mv $@.tmp $@
	rm -f $@.tmp

FORCE:

build/$(NAME).elf: $(SRC) $(ASM) $(LDS) $(if $(filter 1,$(VP)),$(TAG_H))
	@mkdir -p build
	sed 's/LENGTH = 0x3FC0/LENGTH = 0x10000/; s/LENGTH = 0x3980/LENGTH = 0x10000/' $(LDS) > build/measure.ld
	$(CC) $(CFLAGS) $(SRC) $(ASM) $(subst $(LDS),build/measure.ld,$(LDFLAGS)) -o build/$(NAME)_measure.elf
	$(SIZE) build/$(NAME)_measure.elf
	arm-none-eabi-nm --size-sort -S build/$(NAME)_measure.elf | tail -40
	@echo "limit: text+data must be <= $(if $(filter 1,$(BL)),14720 bytes (0x3980),16320 bytes (0x3FC0))"
	$(CC) $(CFLAGS) $(SRC) $(ASM) $(LDFLAGS) -o $@
	$(SIZE) $@

build/$(NAME).hex: build/$(NAME).elf
	$(OBJCOPY) -O ihex $< $@

# per-function / per-file sizes without LTO (LTO merges everything into
# Reset_Handler), to see where the bytes go
analyze: $(SRC) $(ASM) $(if $(filter 1,$(VP)),$(TAG_H))
	@mkdir -p build/obj
	for f in $(SRC); do $(CC) $(subst -flto,,$(CFLAGS)) -c $$f -o build/obj/$$(basename $$f .c).o; done
	$(CC) $(subst -flto,,$(CFLAGS)) -c $(ASM) -o build/obj/ws2812.o
	$(CC) $(subst -flto,,$(LDFLAGS:hk32f030m.ld=build/measure.ld)) build/obj/*.o -o build/analyze.elf
	( echo "== per file (text data bss)"; arm-none-eabi-size build/obj/*.o | sort -k1 -n -r; \
	  echo "== total (no LTO)"; arm-none-eabi-size build/analyze.elf; \
	  echo "== biggest functions/objects"; arm-none-eabi-nm --size-sort -S -r build/analyze.elf | head -120 ) > build/analyze.txt
	cat build/analyze.txt

clean:
	rm -rf build

.PHONY: all clean analyze FORCE
