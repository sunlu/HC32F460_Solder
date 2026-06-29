#!/usr/bin/env python3
"""Generate multi-size ASCII fonts for LCD - no software scaling needed."""

from PIL import Image, ImageDraw, ImageFont
import os

FONT_FILE = "C:/Windows/Fonts/consola.ttf"  # Consolas monospace
if not os.path.exists(FONT_FILE):
    FONT_FILE = "C:/Windows/Fonts/cour.ttf"  # Courier New fallback

# Font definitions: (name, point_size, target_w, target_h)
FONTS = [
    ("font_10x16", 13, 10, 16),   # small labels
    ("font_22x36", 28, 22, 36),   # large temperature
]

for name, pt, tw, th in FONTS:
    try:
        font = ImageFont.truetype(FONT_FILE, pt)
    except:
        font = ImageFont.load_default()

    bpr = (tw + 7) // 8  # bytes per row
    total_bytes = bpr * th

    lines = []
    lines.append(f"/* {name}: {tw}x{th} ASCII font, {total_bytes} bytes/char ({bpr} bytes/row * {th} rows) */")
    lines.append(f"static const uint8_t {name}[96][{total_bytes}] = {{")

    for ch in range(32, 128):
        img = Image.new('1', (tw, th), 0)
        draw = ImageDraw.Draw(img)
        c = chr(ch)
        bbox = draw.textbbox((0, 0), c, font=font)
        bw = bbox[2] - bbox[0]
        bh = bbox[3] - bbox[1]
        x = (tw - bw) // 2 - bbox[0]
        y = (th - bh) // 2 - bbox[1]
        draw.text((x, y), c, font=font, fill=1)

        glyph = []
        for row in range(th):
            for byte_idx in range(bpr):
                byte_val = 0
                for bit in range(8):
                    col = byte_idx * 8 + bit
                    if col < tw and img.getpixel((col, row)):
                        byte_val |= 0x80 >> bit
                glyph.append(byte_val)

        h = ", ".join(f"0x{b:02X}" for b in glyph)
        label = c if 32 <= ch < 127 else '?'
        lines.append(f"    {{ {h} }},  /* {ch:3d} '{label}' */")

    lines.append("};")
    lines.append("")

    fname = f"E:/esp/JBC245_Code/JBC245/MDK/hal/{name}.h"
    with open(fname, "w") as f:
        f.write("\n".join(lines))

    print(f"Generated {name}: {tw}x{th}, {len(lines)-3} chars")

print("Done!")
