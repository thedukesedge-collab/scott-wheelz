# LCM bootloader: firmware updates over BLE (work in progress)

Goal: update the LCM (light control module) firmware from the phone over
BLE instead of with an ST-Link. Branch: `lcm-bootloader`.

> **Status (2026-11-17): not ready to flash.** The bootloader is written
> but is 1188 bytes and must fit in 1024. The app (`lcm/alcm`) hasn't been
> relocated or trimmed yet, and the package side isn't written yet. See
> "Next steps".

## How it works

There is no direct link between the LCM and BLE. The LCM already talks to
the main VESC over UART (USART1, PD1 TX / PD6 RX, 115200), polling the
package with `COMM_CUSTOM_APP_DATA` (package id 101). The update reuses
that link:

```
phone app --BLE--> VESC Express --CAN--> main VESC (VP package) --UART--> LCM bootloader
```

1. The phone uploads the new `.bin` to the package (to be designed: new
   VP commands, image held in package RAM).
2. The package sets flag bit 3 (`LCM_VP_FLAG_UPDATE`, 0x08) in the VP
   block of its `COMMAND_LCM_POLL` reply (byte 17).
3. ALCM sees the flag, disables interrupts and calls the bootloader's
   update entry (address stored in vector slot 7, `0x0800001C`).
4. The bootloader pulls the image from the package, page by page, then
   checks the CRC and starts the new app.

## Flash map (HK32F030MF4P6: 16 KB, 128-byte pages, 2 KB RAM)

| Range | Contents |
| --- | --- |
| `0x08000000-0x080003FF` | bootloader (1 KB) |
| `0x08000400-0x08003F7F` | ALCM app; vector table at `0x08000400`, selected by `FLASH->INT_VEC_OFFSET = 0x400` (vendor `SystemInit`, `VECT_TAB_OFFSET`; must be a multiple of 0x200) |
| `0x08003F80-0x08003FFF` | last page, holds the battery LUT block at `0x08003FC0` (`tools/battery_lut_patch`). Never written by updates, so a patched LUT survives. |

App size limit with the bootloader: `0x3F80 - 0x400` = **15232 bytes**.
Current VP build (GCC, `make VP=1`): **15652 bytes** → about **420 bytes**
to trim, plus a few for the update entry. Candidates: the button / footpad
settings menus in `command_processor.c` (VP sets everything from the
package now).

## Protocol (UART, VESC packets: `0x02, len, payload, crc16 BE, 0x03`)

New package command `COMMAND_LCM_FW = 127` (free in `src/main.c`).

| Direction | Payload |
| --- | --- |
| LCM → VESC | `36, 101, 127, 0x10 (REQ), offset u32` |
| VESC → LCM | `36, 101, 127, 0x10, offset u32, image size u32, image CRC16 u16, 128 data bytes` (size 0 = nothing staged yet) |
| LCM → VESC | `36, 101, 127, 0x11 (DONE), status` (0 ok, 1 CRC mismatch / starting over, 2 nothing came, back to the old app) |

- The image is the app region from `0x08000400`, padded with `0xFF` to whole
  128-byte pages. CRC is CRC16-CCITT/XMODEM (same as VESC packets) over the
  padded image.
- Pages are requested 128, 256, ... and page 0 (vector table) **last**.
  Page 0 is erased when the first chunk arrives, so an interrupted update
  leaves no valid app and the bootloader waits for the image again on the
  next power-up (no ST-Link needed to recover).
- Request timeout 300 ms. From the app: gives up after ~20 s with nothing
  staged (app untouched). Cold boot with no valid app: holds the power
  latch (PA2) and waits; after ~5 min with no image releases it (board off).
- The package must reply within the same `COMM_CUSTOM_APP_DATA` handler
  (VESC sends the reply back on the interface the request came in on).

## Files

- `bootloader.c`, `bootloader.ld`, `Makefile` — `make` → `build/lcm_bootloader.hex`
  (needs arm-none-eabi-gcc; currently fails: 164 bytes over 1 KB).
- `../flasher/LCM_Flasher.pyw` — Windows GUI: choose a .hex, flash it with
  pyocd over ST-Link (same commands as ALCM's `flash.bat`). Put it next to
  `pyocd.yaml` and the HK32 `.pack`.

## Next steps

1. Shrink the bootloader to ≤ 1024 bytes (or move to 0x600 and trim the
   app more).
2. ALCM: link at `0x08000400` (limit `0x3B80`), set `VECT_TAB_OFFSET` 0x400,
   handle `LCM_VP_FLAG_UPDATE` → jump to `*(uint32_t *)0x0800001C`, trim
   ~420 bytes.
3. Build outputs: `ALCM_VP_full.hex` (bootloader + app, flashed once with
   the ST-Link) and `ALCM_VP_app.bin` (update image).
4. Package: `COMMAND_LCM_FW` handling (upload from phone, serve pages to
   the LCM, set/clear the update flag, report progress).
5. Phone app: "Update LCM" button.
6. Test on the board with the ST-Link connected as a safety net.
