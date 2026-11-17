/*
 * Host simulation of the LCM hardware for the bootloader tests: 16 KB
 * flash (128-byte pages, NOR semantics: erase to 0xFF, programming only
 * clears bits), the UART link to a simulated VP package, a 1 ms tick per
 * loop pass, the button and the PA2 power latch. Power cuts, power-off and
 * "app started" leave bl_updater() through longjmp.
 */
#pragma once
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SIM_FLASH_BASE 0x08000000u
#define SIM_FLASH_SIZE 0x4000u

extern uint8_t sim_flash[SIM_FLASH_SIZE];
extern jmp_buf sim_exit;
enum { SIM_RUNNING = 0, SIM_POWER_CUT, SIM_POWER_OFF, SIM_APP_STARTED };

extern int sim_power;  /* PA2 latch */
extern long sim_flash_ops, sim_cut_at; /* power cut when ops reach cut_at (-1 = never) */
extern int sim_bad_write; /* flash written outside the app area / not erased */

uint32_t bl_rd32(uint32_t addr);
uint32_t bl_rd8(uint32_t addr);
const void *bl_ptr(uint32_t addr);
void bl_power_on(void);
void bl_power_off(void) __attribute__((noreturn));
int bl_button_pressed(void);
void bl_hw_init(void);
int bl_tick_ms(void);
void bl_uart_put(uint32_t b);
int bl_uart_get(void);
void bl_flash_erase(uint32_t addr);
void bl_flash_write16(uint32_t addr, uint32_t value);
void bl_start_app(void) __attribute__((noreturn));

int bl_app_valid(void);
void bl_updater(int from_app);
