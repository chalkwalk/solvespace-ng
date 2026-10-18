#!/usr/bin/env python3
"""Layout mock-ups for an owner review: cut the panels and shelf buttons out of a capture of
the main window (test/ngui/capture: window-1-overview-<theme>.png, Outliner + Tools +
Properties shown) and paste them into other arrangements. Mock-ups only: the app is unchanged.

    python3 tools/review/layout_mockups.py CAPTURE_DIR OUT_DIR
"""
import os
import sys
from PIL import Image, ImageDraw

# Where things are in the 1280x800 overview capture.
OUTLINER   = (12, 40, 272, 560)
PROPERTIES = (928, 40, 1268, 560)
SHELF      = (360, 40, 906, 193)
CUBE       = (60, 600, 175, 700)
PANEL_BOTTOM = 14  # the rounded bottom edge kept when a panel is shortened


def button_boxes(im, bg):
    """The shelf's buttons, row by row: runs of the button colour."""
    def runs(values, lo):
        out, start = [], None
        for i, c in enumerate(values + [None]):
            on = c is not None and sum(abs(a - b) for a, b in zip(c, bg)) < 12
            if on and start is None:
                start = i
            if not on and start is not None:
                out.append((start + lo, i + lo))
                start = None
        return out
    x_probe = SHELF[0] + 16
    rows = runs([im.getpixel((x_probe, y)) for y in range(SHELF[1], SHELF[3])], SHELF[1])
    boxes = []
    for y0, y1 in rows:
        ym = (y0 + y1) // 2 - 12
        for x0, x1 in runs([im.getpixel((x, ym)) for x in range(SHELF[0], SHELF[2])], SHELF[0]):
            boxes.append((x0 - 1, y0 - 2, x1 + 1, y1 + 2))
    return boxes


def shortened(im, box, height):
    """A panel cut to `height`, keeping its bottom edge."""
    x0, y0, x1, y1 = box
    out = Image.new("RGB", (x1 - x0, height))
    out.paste(im.crop((x0, y0, x1, y0 + height - PANEL_BOTTOM)), (0, 0))
    out.paste(im.crop((x0, y1 - PANEL_BOTTOM, x1, y1)), (0, height - PANEL_BOTTOM))
    return out


def narrowed(panel, width):
    """A panel cut to `width`, keeping its right edge (and its close button)."""
    if panel.width <= width:
        return panel
    out = panel.crop((0, 0, width, panel.height))
    out.paste(panel.crop((panel.width - 40, 0, panel.width, panel.height)), (width - 40, 0))
    return out


def widened(panel, width):
    """A panel stretched to `width` by repeating its middle column (an empty area)."""
    if panel.width >= width:
        return narrowed(panel, width)
    out = Image.new("RGB", (width, panel.height))
    out.paste(panel.crop((0, 0, panel.width - 40, panel.height)), (0, 0))
    mid = panel.crop((panel.width - 41, 0, panel.width - 40, panel.height))
    for x in range(panel.width - 40, width - 40):
        out.paste(mid, (x, 0))
    out.paste(panel.crop((panel.width - 40, 0, panel.width, panel.height)), (width - 40, 0))
    return out


def tool_panel(im, boxes, columns, horizontal_rows=None):
    """The shelf rebuilt with `columns` buttons per row."""
    title_h = 30
    bw, bh = boxes[0][2] - boxes[0][0], boxes[0][3] - boxes[0][1]
    gap = 6
    rows = (len(boxes) + columns - 1) // columns
    w = 12 + columns * (bw + gap) - gap + 12
    h = title_h + 8 + rows * (bh + 4) + 8
    frame = im.crop(SHELF)
    out = Image.new("RGB", (w, h), frame.getpixel((20, 60)))
    title = frame.crop((0, 0, frame.width, title_h))
    filler = title.crop((frame.width - 60, 0, frame.width - 59, title_h))
    for x in range(0, w):
        out.paste(filler, (x, 0))
    out.paste(title.crop((0, 0, min(120, w - 30), title_h)), (0, 0))
    out.paste(title.crop((frame.width - 30, 0, frame.width, title_h)), (w - 30, 0))
    d = ImageDraw.Draw(out)
    d.rounded_rectangle((0, 0, w - 1, h - 1), radius=8, outline=frame.getpixel((1, 60)))
    for i, b in enumerate(boxes):
        r, c = divmod(i, columns)
        out.paste(im.crop(b), (12 + c * (bw + gap), title_h + 8 + r * (bh + 4)))
    return out


def label(base, text):
    d = ImageDraw.Draw(base)
    d.rectangle((440, 700, 840, 728), fill=(200, 60, 60))
    d.text((452, 708), text, fill=(255, 255, 255))


def main(cap, out):
    os.makedirs(out, exist_ok=True)
    boxes = None
    for theme in ("dark", "light"):
        path = os.path.join(cap, f"window-1-overview-{theme}.png")
        if not os.path.exists(path):
            continue
        im = Image.open(path).convert("RGB")
        bg = im.getpixel((500, 300))
        if boxes is None:
            boxes = button_boxes(im, im.getpixel((SHELF[0] + 16, SHELF[1] + 40)))
        clean = im.copy()
        d = ImageDraw.Draw(clean)
        for box in (OUTLINER, PROPERTIES, SHELF, CUBE):
            d.rectangle(box, fill=bg)
        outl, props = shortened(im, OUTLINER, 170), shortened(im, PROPERTIES, 520)
        column = tool_panel(im, boxes, 2)
        cube = im.crop(CUBE)

        # B: tools column, then Outliner over Properties, on the left.
        b = clean.copy()
        b.paste(column, (12, 40))
        x = 12 + column.width + 10
        b.paste(outl, (x, 40))
        b.paste(narrowed(props, outl.width), (x, 40 + outl.height + 10))
        b.paste(cube, (x + outl.width + 20, 600))
        label(b, "B: tools + Outliner + Properties on the left (mock-up)")
        b.save(os.path.join(out, f"layout-b-{theme}.png"))

        # C: tools column left; Outliner over Properties on the right.
        c = clean.copy()
        c.paste(column, (12, 40))
        x = im.width - 12 - props.width
        x = im.width - 12 - outl.width
        c.paste(outl, (x, 40))
        c.paste(narrowed(props, outl.width), (x, 40 + outl.height + 10))
        c.paste(cube, (12 + column.width + 20, 600))
        label(c, "C: tools left, Outliner + Properties right (mock-up)")
        c.save(os.path.join(out, f"layout-c-{theme}.png"))

        # D: Outliner over Properties on the left; the shelf along the top.
        dd = clean.copy()
        dd.paste(outl, (12, 40))
        dd.paste(narrowed(props, outl.width), (12, 40 + outl.height + 10))
        row = tool_panel(im, boxes, 17)
        dd.paste(row, ((im.width - row.width) // 2 + outl.width // 2, 40))
        dd.paste(cube, (12 + outl.width + 20, 600))
        label(dd, "D: Outliner + Properties left, tools along the top (mock-up)")
        dd.save(os.path.join(out, f"layout-d-{theme}.png"))

        a = im.copy()
        label(a, "A: now")
        a.save(os.path.join(out, f"layout-a-{theme}.png"))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
