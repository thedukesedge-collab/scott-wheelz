# LCM bootloader: firmware updates over Bluetooth

Update the LCM (light control module) firmware from the phone app instead of
with an ST-Link. Only for **ADV200 controllers (Floatwheel XRV, GTV, PintV)**
running the ALCM VP light firmware. Started on the PC, finished in the
`lcm-bootloader` branch.

> **Status: built and host-tested, not yet tried on a board.** The first
> install needs the ST-Link anyway; keep it connected for the first test.

## Why not from the stock LCM firmware?

Checked (Nov 2026): the stock Floatwheel LCM firmware can't be flashed or
changed from the package. It has no code that writes its own flash, it only
polls the VESC for `COMM_GET_VALUES` (answered by the VESC firmware, not the
package), and its UART receive can't overflow (8-bit index into a 256-byte
buffer) - nothing to hook into. The chip's own boot modes need hardware pins
that aren't wired to the VESC. So every board needs this bootloader flashed
once with an ST-Link; after that, everything goes through the app.

## How it works

There is no direct link between the LCM and Bluetooth. The LCM already talks
to the main VESC over UART (USART1, PD1 TX / PD6 RX, 115200), polling the
package. The update reuses that link:

```
phone app --BLE--> VESC Express --CAN--> main VESC (VP package) --UART--> LCM bootloader
```

1. The phone app (LEDS tab › LCM FIRMWARE › UPDATE LCM) sends START (size,
   CRC) to the package. The image is built with the package and embedded in
   its app UI.
2. The package sets bit 4 (`0x10`) in the flags byte of the VP block of its
   LCM poll replies - only while the board is not running.
3. ALCM (bootloader build), if idle, switches the headlights and buzzer off,
   masks interrupts and calls the bootloader's update entry (address in
   vector slot 7, `0x0800001C`). No chip reset: see "Power" below.
4. The bootloader pulls the image from the package page by page, checks it,
   and starts the new app. The phone feeds the package two pages at a time.

## Power (why there is no reset)

The LCM is only powered while it drives **PA2 (PWR_EN)** high: the button
press powers it up, PA2 latches it. A chip reset would release PA2 and could
cut the power to the whole board. So:

- nothing ever resets the chip; app → bootloader → app are plain jumps;
- in update mode the bootloader drives PA2 high before anything else (output
  level set before the pin mode, so it never goes low);
- the bootloader build of ALCM sets PA2 **on** at init instead of off
  (stock ALCM switches it off and on again a moment later at boot - harmless
  with a finger on the button, not after an update when nobody is);
- the button still works: **hold 2 s** (after the power-up press is
  released) to power off. There is **no timed power-off**: the VESC balances
  without the LCM, so the bootloader can't know nobody is riding, and power
  is only ever released by the button. The VP package refuses to engage
  while the bootloader is asking for pages (lights dark + board won't start =
  finish the update from the app, or hold the button to power off).

## Fail-safe

- **The bootloader is never written by itself.** Erase/write only reach the
  app area (address guard), so the bootloader and the battery LUT page survive
  any update.
- **Page 0 last, back to front.** Page 0 holds the app's vector table, which
  is what decides "is there an app". It is erased before the first page is
  written, written last, and back to front so its first two words (stack
  pointer, reset vector) are the very last bytes written.
- **Every page is read back**, and the whole image CRC is checked before the
  update counts; on a mismatch page 0 is erased again. After 3 bad images
  nothing more is written until the next power-up (no flash wear from a bad
  source).
- **Interrupted at any point** (power loss, phone gone, app closed): there is
  no valid app, so the next power-up goes straight into the bootloader, holds
  the power on and waits for the image - press UPDATE LCM again. No ST-Link
  needed.
- **Not while riding.** The package refuses START and never raises the flag
  while the board is running, and ALCM only hands over when idle.
- Nothing staged: coming from the app, the bootloader goes back to the
  unchanged app after 20 s.

`tests/` checks all of this on the PC with simulated flash, UART, button
and power latch: a power cut at **every** flash operation of an update
(small and full-size images) never leaves a partial app; corrupted pages,
a wrong-address image, the button and idle power-off, and an end-to-end run
phone → package (the real `src/lcm_fw.c`) → bootloader, including a phone
that disappears mid-update.

## Flash map (HK32F030MF4P6: 16 KB, 128-byte pages, 2 KB RAM)

| Range | Contents |
| --- | --- |
| `0x08000000-0x080005FF` | bootloader (1.5 KB; currently ~1500 bytes) |
| `0x08000600-0x08003F7F` | ALCM app (14720 bytes max); vector table at `0x08000600`, selected by `FLASH->INT_VEC_OFFSET = 0x600` (vendor `SystemInit`, `VECT_TAB_OFFSET`; a multiple of 0x200) |
| `0x08003F80-0x08003FFF` | last page, holds the battery LUT block at `0x08003FC0`; never written by updates |

To fit, the bootloader build of ALCM (`make BL=1`) uses small runtime
routines (`tiny_rt.c`), direct register setup for the USART/EXTI/NVIC, a
32-bit ratio in the function generator - all without changing behavior -
and leaves out the battery LUT patch (the battery % comes from the VESC, as
it does anyway without a patched LUT). The standalone `ALCM_VP.hex` keeps it.

## Protocol

UART, VESC packets: `0x02, len, payload, crc16 BE, 0x03`. Command
`COMMAND_LCM_FW = 128` (package id 101).

| Direction | Payload |
| --- | --- |
| LCM → VESC | `36, 101, 128, 0x10 (REQ), offset u32` |
| VESC → LCM | `36, 101, 128, 0x10, offset u32, image size u32, image CRC16 u16, 128 data bytes`; size 0 = not there yet (asked again 20 ms later) |
| LCM → VESC | `36, 101, 128, 0x11 (DONE), status` (0 ok, 1 CRC mismatch / starting over, 2 nothing came, back to the old app) |
| phone → package | `101, 128, 0x01 START, size u32, crc u16` / `0x02 DATA, offset u32, 128 bytes` / `0x03 STATUS` / `0x04 CANCEL` |
| package → phone | `101, 128, 0x03, state, error, wanted offset u32, size u32, flags` (flags: bit0 a bootloader asked in the last 5 s, bit1 the LCM named itself `VPBL-...`) |

**Fail-safe:** the package refuses START (error 3, no bootloader) unless the
LCM's poll name starts with `VPBL` (ALCM VP built with `BL=1` names itself
`VPBL-<lcm/alcm tree hash>`, without the bootloader `VP-<hash>`) or a
bootloader is asking for pages right now (recovery after an interrupted
update). The app hides FLASH LCM in the same cases.

The image is the app area from `0x08000600`, padded with `0xFF` to whole
128-byte pages; CRC16-CCITT (XMODEM), the same as the VESC packet CRC.

## Files

- `bootloader.c` - the logic; `bl_hw.h` - register-level hardware access;
  `bootloader.ld`, `Makefile` (`make` → `build/lcm_bootloader.hex`,
  `make test` → host tests).
- `../alcm/gcc`: `make BL=1` → `ALCM_VP_BL_app.hex` (the app for after the
  bootloader).
- `../tools/lcm_images.py` → `ALCM_VP_BL_full.hex`, published as `SLCM.hex` (bootloader + app, for the
  ST-Link, once) and `ALCM_VP_app.bin` / `.json` (the update image); the
  package build embeds the `.json` (`make LCM_FW=0` to leave it out).
- `../flasher/LCM_Flasher.pyw` - Windows GUI: choose a .hex, flash it with
  pyocd over the ST-Link.

## First install (once, with the ST-Link)

1. Keep a copy of the firmware you have now (`ALCM_v0.1.1.hex` or
   `ALCM_VP.hex`) to flash back if needed.
2. Flash **`SLCM.hex`** with the ST-Link (LCM Flasher or
   `flash.bat`), exactly like before.
3. Board on a stand: power on with the button, check the lights, hold the
   button to power off. Then in the app: LEDS › LCM FIRMWARE › UPDATE LCM,
   and watch it go to 100% and the lights come back.
4. Test an interruption: start an update and cut it (close the app). The
   board should stay on; power-cycle it, press UPDATE again, it finishes.

From then on, new LCM firmware comes with the package: install the package,
then UPDATE LCM.
