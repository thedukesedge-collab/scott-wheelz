/*
 * WS2812 bit-banging, GNU assembler port of src/ws2812.s (Keil armasm).
 * Same instructions in the same order, so the cycle timing is unchanged.
 *
 * Copyright (c) 2024-2025, Mitchell White - part of ALCM (GPL-3.0-or-later)
 */
    .syntax unified
    .cpu cortex-m0
    .thumb

    .equ T0H, 1
    .equ T1H, 3
    .equ TRESET, 400
    .equ PIN_SET, (1 << 4)
    .equ PIN_RESET, (1 << (4 + 16))
    .equ GPIOD_BSRR, 0x48000C18

    .section .text.ws2812_send_buffer, "ax", %progbits
    .global ws2812_send_buffer
    .type ws2812_send_buffer, %function
    .thumb_func
/* R0 = uint8_t* buffer, R1 = uint32_t length */
ws2812_send_buffer:
    push {r4-r7, lr}

    cmp r0, #0
    beq end_function

    cmp r1, #0
    beq end_function

    ldr r6, =GPIOD_BSRR

next_byte:
    ldrb r2, [r0]
    adds r0, r0, #1
    lsls r2, r2, #24
    movs r3, #8

next_bit:
    lsls r2, r2, #1
    bcc bit_0
    b bit_1

bit_0:
    movs r4, #T0H
    b send_bit

bit_1:
    movs r4, #T1H
    b send_bit

send_bit:
    movs r7, #PIN_SET
    str r7, [r6]

high_delay:
    subs r4, r4, #1
    bne high_delay

    ldr r7, =PIN_RESET
    str r7, [r6]

    subs r3, r3, #1
    bne next_bit

    subs r1, r1, #1
    bne next_byte

    ldr r4, =TRESET
t_reset:
    subs r4, r4, #1
    bne t_reset

end_function:
    nop
    pop {r4-r7, pc}
    .size ws2812_send_buffer, . - ws2812_send_buffer
    .ltorg
