#!/usr/bin/env python3
"""Crop the capture suite's screenshots (test/ngui/capture) and put each item's dark and
light versions side by side: combine.py CAPTURE_DIR OUT_DIR"""
import os
import sys

from PIL import Image


def main(src, out):
    os.makedirs(out, exist_ok=True)
    crops = {}
    for line in open(os.path.join(src, "crops.txt")):
        name, theme, *box = line.split()
        crops[(name, theme)] = tuple(int(float(v)) for v in box)
    for name in sorted({n for n, _ in crops}):
        parts = []
        for theme in ("dark", "light"):
            path = os.path.join(src, f"{name}-{theme}.png")
            if (name, theme) not in crops or not os.path.exists(path):
                continue
            im = Image.open(path).convert("RGB")
            x0, y0, x1, y1 = crops[(name, theme)]
            parts.append(im.crop((x0, y0, min(x1, im.width), min(y1, im.height))))
        if not parts:
            continue
        gap = 16
        w = sum(p.width for p in parts) + gap * (len(parts) - 1)
        h = max(p.height for p in parts)
        sheet = Image.new("RGB", (w, h), (128, 128, 128))
        x = 0
        for p in parts:
            sheet.paste(p, (x, 0))
            x += p.width + gap
        sheet.save(os.path.join(out, name + ".png"))
        print(name)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
