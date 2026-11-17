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
NAME   := $(if $(filter 1,$(VP)),ALCM_VP,ALCM)

SRC := startup_hk32f030m.c \
  $(addprefix $(APP)/, hk32f030m_it.c main.c event_queue.c timer.c button_driver.c \
    button_events.c board_mode.c power.c power_hw.c buzzer.c tim1.c headlights.c \
    buzzer_hw.c footpads.c footpads_hw.c status_leds.c status_leds_hw.c headlights_hw.c \
    button_driver_hw.c vesc_serial.c vesc_serial_hw.c battery_lut.c battery_lut_hw.c \
    ring_buffer.c crc16_ccitt.c animations.c settings.c eeprom.c command_processor.c \
    function_generator.c hysteresis.c interrupts_hw.c) \
  $(L)/HK32F030M/CMSIS/HK32F030M/Source/system_hk32f030m.c \
  $(addprefix $(L)/HK32F030M/HK32F030M_Lib/src/, hk32f030m_rcc.c hk32f030m_gpio.c \
    hk32f030m_exti.c hk32f030m_misc.c hk32f030m_syscfg.c hk32f030m_tim.c hk32f030m_adc.c \
    hk32f030m_usart.c hk32f030m_flash.c)
ASM := ws2812.S

INC := -I$(L)/HK32F030M/HK32F030M_Lib/inc -I$(L)/HK32F030M/CMSIS/CM0/Core \
       -I$(L)/HK32F030M/CMSIS/HK32F030M/Include -I$(L)/HK32F030M_Project/inc
DEF := -DHK32F030M -DHK32F030MF4P6 $(if $(filter 1,$(VP)),-DVP_LIGHT_CONTROL)
ARCH := -mcpu=cortex-m0 -mthumb
CFLAGS := $(ARCH) -Oz -std=gnu99 -ffunction-sections -fdata-sections -flto \
          -fno-common -Wall -Wno-unused-parameter $(DEF) $(INC)
LDFLAGS := $(ARCH) -Oz -flto -nostartfiles -specs=nano.specs -specs=nosys.specs \
           -Wl,--gc-sections -Wl,-Map=build/$(NAME).map -T hk32f030m.ld

all: build/$(NAME).hex

# measure first with an oversized flash region, so the real size and the
# biggest functions are printed even when it doesn't fit
build/$(NAME).elf: $(SRC) $(ASM) hk32f030m.ld
	@mkdir -p build
	sed 's/LENGTH = 0x3FC0/LENGTH = 0x10000/' hk32f030m.ld > build/measure.ld
	$(CC) $(CFLAGS) $(SRC) $(ASM) $(subst hk32f030m.ld,build/measure.ld,$(LDFLAGS)) -o build/$(NAME)_measure.elf
	$(SIZE) build/$(NAME)_measure.elf
	arm-none-eabi-nm --size-sort -S build/$(NAME)_measure.elf | tail -40
	@echo "limit: text+data must be <= 16320 bytes (0x3FC0)"
	$(CC) $(CFLAGS) $(SRC) $(ASM) $(LDFLAGS) -o $@
	$(SIZE) $@

build/$(NAME).hex: build/$(NAME).elf
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -rf build

.PHONY: all clean
