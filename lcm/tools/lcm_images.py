#!/usr/bin/env python3
"""Refloat Glow: builds the LCM bootloader images from the two .hex
files (lcm/bootloader + lcm/alcm built with BL=1):

  ALCM_VP_BL_full.hex  bootloader + app, flashed ONCE with the ST-Link
  ALCM_VP_app.bin      the update image the package sends to the bootloader
                       (app area from 0x08000600, padded with 0xFF to whole
                       128-byte pages)
  ALCM_VP_app.json     {"size", "crc", "version", "b64"} for the phone app

Checks the same things the bootloader checks before it starts an app, so a
broken image can't be shipped: stack pointer in RAM, Thumb reset vector
inside the app area, everything inside its own area, CRC16-CCITT (XMODEM).

usage: lcm_images.py bootloader.hex app.hex out_dir version
"""
import base64
import json
import os
import sys

BL_START, APP_START, APP_END = 0x08000000, 0x08000600, 0x08003F80
PAGE = 128


def read_hex(path):
    mem, base = {}, 0
    for line in open(path):
        line = line.strip()
        if not line.startswith(":"):
            continue
        raw = bytes.fromhex(line[1:])
        if sum(raw) & 0xFF:
            sys.exit(f"{path}: bad checksum: {line}")
        n, addr, typ, data = raw[0], (raw[1] << 8) | raw[2], raw[3], raw[4:4 + raw[0]]
        if typ == 0:
            for i, b in enumerate(data):
                mem[base + addr + i] = b
        elif typ == 4:
            base = ((data[0] << 8) | data[1]) << 16
        elif typ == 2:
            base = ((data[0] << 8) | data[1]) << 4
        elif typ == 1:
            break
    return mem


def write_hex(path, mem):
    out, upper = [], None
    addrs = sorted(mem)
    i = 0
    while i < len(addrs):
        a = addrs[i]
        if a >> 16 != upper:
            upper = a >> 16
            rec = bytes([2, 0, 0, 4, upper >> 8, upper & 0xFF])
            out.append(":" + (rec + bytes([(-sum(rec)) & 0xFF])).hex().upper())
        chunk = [mem[a]]
        while (len(chunk) < 16 and i + len(chunk) < len(addrs)
               and addrs[i + len(chunk)] == a + len(chunk) and (a + len(chunk)) >> 16 == upper):
            chunk.append(mem[a + len(chunk)])
        rec = bytes([len(chunk), (a >> 8) & 0xFF, a & 0xFF, 0]) + bytes(chunk)
        out.append(":" + (rec + bytes([(-sum(rec)) & 0xFF])).hex().upper())
        i += len(chunk)
    out.append(":00000001FF")
    open(path, "w").write("\n".join(out) + "\n")


def crc16(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


def main():
    bl_hex, app_hex, out_dir, version = sys.argv[1:5]
    bl, app = read_hex(bl_hex), read_hex(app_hex)

    if not bl or min(bl) != BL_START or max(bl) >= APP_START:
        sys.exit("bootloader must lie in 0x08000000-0x080005FF")
    if not app or min(app) != APP_START or max(app) >= APP_END:
        sys.exit("app must lie in 0x08000600-0x08003F7F (built with BL=1?)")

    end = max(app) + 1
    size = (end - APP_START + PAGE - 1) // PAGE * PAGE
    image = bytes(app.get(APP_START + i, 0xFF) for i in range(size))

    sp = int.from_bytes(image[0:4], "little")
    pc = int.from_bytes(image[4:8], "little")
    if not (0x20000001 <= sp <= 0x20000800) or not pc & 1 or not APP_START <= pc < APP_END:
        sys.exit(f"app vectors look wrong: sp={sp:#x} pc={pc:#x}")

    crc = crc16(image)
    os.makedirs(out_dir, exist_ok=True)
    full = dict(bl)
    full.update(app)
    write_hex(os.path.join(out_dir, "ALCM_VP_BL_full.hex"), full)
    open(os.path.join(out_dir, "ALCM_VP_app.bin"), "wb").write(image)
    json.dump({"size": size, "crc": crc, "version": version,
               "b64": base64.b64encode(image).decode()},
              open(os.path.join(out_dir, "ALCM_VP_app.json"), "w"))
    print(f"app hex: {len(app)} bytes at {min(app):#x}-{max(app):#x}")
    print(f"bootloader {max(bl) + 1 - BL_START} bytes, app image {size} bytes "
          f"(app {end - APP_START} of {APP_END - APP_START} bytes, {APP_END - end} free), crc {crc:#06x}")


if __name__ == "__main__":
    main()
