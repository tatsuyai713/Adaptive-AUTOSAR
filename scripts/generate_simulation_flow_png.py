#!/usr/bin/env python3
"""Generate docs/simulation_flow_diagram.png without external libraries.

This keeps the PNG preview in sync with the drawio source using only
Python standard library.
"""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

WIDTH = 1600
HEIGHT = 900
BG = (255, 255, 255)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "simulation_flow_diagram.png"


FONT = {
    "A": [
        "01110",
        "10001",
        "10001",
        "11111",
        "10001",
        "10001",
        "10001",
    ],
    "B": [
        "11110",
        "10001",
        "10001",
        "11110",
        "10001",
        "10001",
        "11110",
    ],
    "C": [
        "01111",
        "10000",
        "10000",
        "10000",
        "10000",
        "10000",
        "01111",
    ],
    "D": [
        "11110",
        "10001",
        "10001",
        "10001",
        "10001",
        "10001",
        "11110",
    ],
    "E": [
        "11111",
        "10000",
        "10000",
        "11110",
        "10000",
        "10000",
        "11111",
    ],
    "F": [
        "11111",
        "10000",
        "10000",
        "11110",
        "10000",
        "10000",
        "10000",
    ],
    "G": [
        "01111",
        "10000",
        "10000",
        "10111",
        "10001",
        "10001",
        "01110",
    ],
    "H": [
        "10001",
        "10001",
        "10001",
        "11111",
        "10001",
        "10001",
        "10001",
    ],
    "I": [
        "11111",
        "00100",
        "00100",
        "00100",
        "00100",
        "00100",
        "11111",
    ],
    "J": [
        "00111",
        "00010",
        "00010",
        "00010",
        "10010",
        "10010",
        "01100",
    ],
    "K": [
        "10001",
        "10010",
        "10100",
        "11000",
        "10100",
        "10010",
        "10001",
    ],
    "L": [
        "10000",
        "10000",
        "10000",
        "10000",
        "10000",
        "10000",
        "11111",
    ],
    "M": [
        "10001",
        "11011",
        "10101",
        "10101",
        "10001",
        "10001",
        "10001",
    ],
    "N": [
        "10001",
        "11001",
        "10101",
        "10011",
        "10001",
        "10001",
        "10001",
    ],
    "O": [
        "01110",
        "10001",
        "10001",
        "10001",
        "10001",
        "10001",
        "01110",
    ],
    "P": [
        "11110",
        "10001",
        "10001",
        "11110",
        "10000",
        "10000",
        "10000",
    ],
    "Q": [
        "01110",
        "10001",
        "10001",
        "10001",
        "10101",
        "10010",
        "01101",
    ],
    "R": [
        "11110",
        "10001",
        "10001",
        "11110",
        "10100",
        "10010",
        "10001",
    ],
    "S": [
        "01111",
        "10000",
        "10000",
        "01110",
        "00001",
        "00001",
        "11110",
    ],
    "T": [
        "11111",
        "00100",
        "00100",
        "00100",
        "00100",
        "00100",
        "00100",
    ],
    "U": [
        "10001",
        "10001",
        "10001",
        "10001",
        "10001",
        "10001",
        "01110",
    ],
    "V": [
        "10001",
        "10001",
        "10001",
        "10001",
        "10001",
        "01010",
        "00100",
    ],
    "W": [
        "10001",
        "10001",
        "10001",
        "10101",
        "10101",
        "10101",
        "01010",
    ],
    "X": [
        "10001",
        "10001",
        "01010",
        "00100",
        "01010",
        "10001",
        "10001",
    ],
    "Y": [
        "10001",
        "10001",
        "01010",
        "00100",
        "00100",
        "00100",
        "00100",
    ],
    "Z": [
        "11111",
        "00001",
        "00010",
        "00100",
        "01000",
        "10000",
        "11111",
    ],
    "0": [
        "01110",
        "10001",
        "10011",
        "10101",
        "11001",
        "10001",
        "01110",
    ],
    "1": [
        "00100",
        "01100",
        "00100",
        "00100",
        "00100",
        "00100",
        "01110",
    ],
    "2": [
        "01110",
        "10001",
        "00001",
        "00010",
        "00100",
        "01000",
        "11111",
    ],
    "3": [
        "11110",
        "00001",
        "00001",
        "01110",
        "00001",
        "00001",
        "11110",
    ],
    "4": [
        "00010",
        "00110",
        "01010",
        "10010",
        "11111",
        "00010",
        "00010",
    ],
    "5": [
        "11111",
        "10000",
        "10000",
        "11110",
        "00001",
        "00001",
        "11110",
    ],
    "6": [
        "01110",
        "10000",
        "10000",
        "11110",
        "10001",
        "10001",
        "01110",
    ],
    "7": [
        "11111",
        "00001",
        "00010",
        "00100",
        "01000",
        "01000",
        "01000",
    ],
    "8": [
        "01110",
        "10001",
        "10001",
        "01110",
        "10001",
        "10001",
        "01110",
    ],
    "9": [
        "01110",
        "10001",
        "10001",
        "01111",
        "00001",
        "00001",
        "01110",
    ],
    "-": [
        "00000",
        "00000",
        "00000",
        "11111",
        "00000",
        "00000",
        "00000",
    ],
    "/": [
        "00001",
        "00010",
        "00100",
        "01000",
        "10000",
        "00000",
        "00000",
    ],
    "(": [
        "00010",
        "00100",
        "01000",
        "01000",
        "01000",
        "00100",
        "00010",
    ],
    ")": [
        "01000",
        "00100",
        "00010",
        "00010",
        "00010",
        "00100",
        "01000",
    ],
    ":": [
        "00000",
        "00100",
        "00100",
        "00000",
        "00100",
        "00100",
        "00000",
    ],
    ".": [
        "00000",
        "00000",
        "00000",
        "00000",
        "00000",
        "00110",
        "00110",
    ],
    " ": [
        "00000",
        "00000",
        "00000",
        "00000",
        "00000",
        "00000",
        "00000",
    ],
}


def new_canvas(width: int, height: int, color: tuple[int, int, int]):
    r, g, b = color
    return [bytearray([r, g, b] * width) for _ in range(height)]


def put_px(img, x: int, y: int, color: tuple[int, int, int]):
    if x < 0 or y < 0 or y >= len(img) or x >= len(img[0]) // 3:
        return
    i = x * 3
    img[y][i : i + 3] = bytes(color)


def fill_rect(img, x: int, y: int, w: int, h: int, color: tuple[int, int, int]):
    for yy in range(y, y + h):
        if yy < 0 or yy >= len(img):
            continue
        row = img[yy]
        x0 = max(0, x)
        x1 = min(len(row) // 3, x + w)
        if x0 >= x1:
            continue
        pix = bytes(color) * (x1 - x0)
        row[x0 * 3 : x1 * 3] = pix


def stroke_rect(img, x: int, y: int, w: int, h: int, color: tuple[int, int, int], t: int = 2):
    fill_rect(img, x, y, w, t, color)
    fill_rect(img, x, y + h - t, w, t, color)
    fill_rect(img, x, y, t, h, color)
    fill_rect(img, x + w - t, y, t, h, color)


def dashed_rect(img, x: int, y: int, w: int, h: int, color: tuple[int, int, int], dash: int = 10):
    for xx in range(x, x + w, dash * 2):
        fill_rect(img, xx, y, min(dash, x + w - xx), 2, color)
        fill_rect(img, xx, y + h - 2, min(dash, x + w - xx), 2, color)
    for yy in range(y, y + h, dash * 2):
        fill_rect(img, x, yy, 2, min(dash, y + h - yy), color)
        fill_rect(img, x + w - 2, yy, 2, min(dash, y + h - yy), color)


def line(img, x0: int, y0: int, x1: int, y1: int, color: tuple[int, int, int]):
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        put_px(img, x0, y0, color)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def arrow(img, x0: int, y0: int, x1: int, y1: int, color: tuple[int, int, int]):
    line(img, x0, y0, x1, y1, color)
    line(img, x1, y1, x1 - 10, y1 - 6, color)
    line(img, x1, y1, x1 - 10, y1 + 6, color)


def draw_char(img, x: int, y: int, ch: str, scale: int, color: tuple[int, int, int]):
    pat = FONT.get(ch, FONT[" "])
    for ry, row in enumerate(pat):
        for rx, c in enumerate(row):
            if c == "1":
                fill_rect(img, x + rx * scale, y + ry * scale, scale, scale, color)


def text(img, x: int, y: int, s: str, scale: int = 2, color: tuple[int, int, int] = (0, 0, 0)):
    cx = x
    for ch in s:
        draw_char(img, cx, y, ch, scale, color)
        cx += 6 * scale


def center_text(img, x: int, y: int, w: int, h: int, lines: list[str], scale: int = 2, color=(0, 0, 0)):
    line_h = 8 * scale
    block_h = len(lines) * line_h
    cy = y + (h - block_h) // 2
    for ln in lines:
        tw = len(ln) * 6 * scale
        cx = x + (w - tw) // 2
        text(img, cx, cy, ln, scale=scale, color=color)
        cy += line_h


def chunk(tag: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    )


def write_png(path: Path, img) -> None:
    h = len(img)
    w = len(img[0]) // 3
    raw = b"".join(bytes([0]) + bytes(row) for row in img)
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    data = zlib.compress(raw, 9)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", data) + chunk(b"IEND", b"")
    path.write_bytes(png)


def main() -> None:
    img = new_canvas(WIDTH, HEIGHT, BG)

    black = (30, 30, 30)
    blue_fill = (218, 232, 252)
    blue_stroke = (108, 142, 191)
    pink_fill = (248, 206, 204)
    pink_stroke = (184, 84, 80)
    green_fill = (213, 232, 212)
    green_stroke = (130, 179, 102)
    yellow_fill = (255, 242, 204)
    yellow_stroke = (214, 182, 86)
    purple_fill = (225, 213, 231)
    purple_stroke = (150, 115, 166)
    gray_fill = (245, 245, 245)
    gray_stroke = (110, 110, 110)

    # Boundary
    dashed_rect(img, 280, 120, 1020, 660, black, dash=12)
    text(img, 560, 95, "RASPBERRY PI LINUX ECU", scale=3, color=black)

    # External blocks
    fill_rect(img, 40, 370, 220, 110, blue_fill)
    stroke_rect(img, 40, 370, 220, 110, blue_stroke, 3)
    center_text(img, 40, 370, 220, 110, ["CAN BUS", "SOCKETCAN", "CAN0 VCAN0"], scale=2)

    fill_rect(img, 1320, 260, 220, 90, pink_fill)
    stroke_rect(img, 1320, 260, 220, 90, pink_stroke, 3)
    center_text(img, 1320, 260, 220, 90, ["SOMEIP PEER ECU"], scale=2)

    fill_rect(img, 1320, 500, 220, 90, green_fill)
    stroke_rect(img, 1320, 500, 220, 90, green_stroke, 3)
    center_text(img, 1320, 500, 220, 90, ["DDS PEER ECU"], scale=2)

    # Internal blocks
    fill_rect(img, 340, 160, 180, 70, yellow_fill)
    stroke_rect(img, 340, 160, 180, 70, yellow_stroke, 3)
    center_text(img, 340, 160, 180, 70, ["SYSTEMD"], scale=2)

    fill_rect(img, 580, 150, 300, 80, gray_fill)
    stroke_rect(img, 580, 150, 300, 80, gray_stroke, 2)
    center_text(img, 580, 150, 300, 80, ["AUTOSAR IOX ROUDI", "SERVICE"], scale=2)

    fill_rect(img, 910, 150, 340, 80, gray_fill)
    stroke_rect(img, 910, 150, 340, 80, gray_stroke, 2)
    center_text(img, 910, 150, 340, 80, ["AUTOSAR EXEC MANAGER", "SERVICE"], scale=2)

    fill_rect(img, 860, 280, 290, 80, purple_fill)
    stroke_rect(img, 860, 280, 290, 80, purple_stroke, 3)
    center_text(img, 860, 280, 290, 80, ["ETC AUTOSAR", "BRINGUP SH"], scale=2)

    fill_rect(img, 650, 390, 320, 120, blue_fill)
    stroke_rect(img, 650, 390, 320, 120, (74, 134, 232), 3)
    center_text(img, 650, 390, 320, 120, ["USER APPS", "ARA COM ONLY API"], scale=3)

    fill_rect(img, 430, 560, 760, 90, gray_fill)
    stroke_rect(img, 430, 560, 760, 90, (108, 117, 125), 2)
    center_text(img, 430, 560, 760, 90, ["AUTOSAR AP RUNTIME LIBRARIES", "ARA CORE COM EXEC DIAG LOG PHM PER UCM IAM TSYNC"], scale=2)

    fill_rect(img, 510, 670, 600, 70, green_fill)
    stroke_rect(img, 510, 670, 600, 70, green_stroke, 3)
    center_text(img, 510, 670, 600, 70, ["TRANSPORT BACKENDS VSOMEIP CYCLONEDDS ICEORYX"], scale=2)

    # Arrows
    arrow(img, 260, 425, 650, 445, black)
    arrow(img, 1320, 305, 970, 430, black)
    arrow(img, 970, 455, 1320, 545, black)
    arrow(img, 520, 195, 580, 190, black)
    arrow(img, 520, 200, 910, 190, black)
    arrow(img, 1080, 230, 1030, 280, black)
    arrow(img, 1000, 360, 900, 390, black)
    arrow(img, 810, 560, 810, 510, black)
    arrow(img, 810, 670, 810, 650, black)

    text(img, 1010, 385, "DDS PUBLISH", scale=2)
    text(img, 1000, 300, "SOMEIP INPUT", scale=2)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    write_png(OUT, img)
    print(f"[OK] Generated {OUT}")


if __name__ == "__main__":
    main()
