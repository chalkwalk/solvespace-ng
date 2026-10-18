#!/usr/bin/env python3
"""The keymap comparison of docs/research/keymap-and-layout.md, one table per decision, as
images for an owner review round and as Markdown for the doc.

    python3 tools/review/keymap_tables.py OUT_DIR      # writes keys-<id>.png, prints Markdown

"*" marks a key taken from memory or a third-party list, not re-checked against the app's
official list (see the doc's sources).
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

APPS = ["Classic", "Fusion", "Onshape", "Shapr3D", "Plasticity", "FreeCAD", "Blender",
        "NG now", "Proposed"]

TABLES = {
    "k1-sketch": ("K1. Sketch tools on the industry letters", [
        ("Line",          "S",       "L",    "L",           "L",      "Shift+A", "G,L*", "-",  "L",       "L"),
        ("Rectangle",     "R",       "R",    "G / R",       "R",      "-",       "G,R*", "-",  "B",       "R"),
        ("Circle",        "C",       "C",    "C",           "C",      "-",       "G,C*", "-",  "C",       "C"),
        ("Arc",           "A",       "-",    "A",           "A",      "-",       "G,A*", "-",  "Shift+C", "A"),
        ("Tangent arc",   "Shift+A", "-",    "Shift+A (1)", "-",      "-",       "-",    "-",  "Alt+C",   "Alt+C"),
        ("Point",         "P",       "-",    "Shift+S",     "-",      "-",       "-",    "-",  "P",       "P"),
        ("Spline",        "B",       "-",    "-",           "I",      "-",       "G,B*", "-",  "Shift+B", "S"),
        ("Text",          "T",       "-",    "-",           "-",      "-",       "-",    "-",  "Shift+T", "Shift+T"),
        ("Select all",    "Ctrl+A",  "Ctrl+A*", "-",        "Ctrl+A", "A",       "Ctrl+A*", "A", "A",     "Ctrl+A"),
    ]),
    "k2-hv-hide": ("K2. Horizontal / vertical against hide", [
        ("Horizontal",    "H",  "-",   "H",       "Shift+V (2)", "-",       "K,H*", "-",      "F, 1",    "a: H  / b: F, 1"),
        ("Vertical",      "V",  "-",   "V",       "Shift+V (2)", "-",       "V",    "-",      "F, 2",    "a: V  / b: F, 2"),
        ("Hide",          "-",  "V*",  "Y",       "-",           "H",       "Space*", "H",    "H",       "a: Y  / b: H"),
        ("Show all",      "-",  "-",   "Shift+Y", "-",           "Alt+H",   "-",    "Alt+H",  "Alt+H",   "a: Shift+Y / b: Alt+H"),
        ("Isolate",       "-",  "-",   "Shift+I", "-",           ".",       "-",    "/",      "Shift+H", "a: Shift+I / b: Shift+H"),
    ]),
    "k3-constraints": ("K3. The other constraints on letters", [
        ("Coincident / on",    "O", "-", "I",           "Shift+N",           "-", "K,C*",        "-", "F pie", "I"),
        ("Parallel / tangent", "L", "-", "B / T",       "Shift+A / Shift+T", "-", "K,P* / K,T*", "-", "F pie", "B, T"),
        ("Perpendicular",      "[", "-", "Shift+L",     "Shift+P",           "-", "K,N*",        "-", "F pie", "Shift+L"),
        ("Equal",              "Q", "-", "E",           "Shift+E",           "-", "K,E*",        "-", "F pie", "Q"),
        ("Symmetric",          "Y", "-", "Shift+Q",     "Shift+S",           "-", "K,S*",        "-", "F pie", "Shift+S"),
        ("Midpoint",           "M", "-", "Shift+M",     "Shift+M",           "-", "-",           "-", "F pie", "M"),
        ("Lock where dragged", "]", "-", "Shift+J",     "Shift+L",           "-", "K,L*",        "-", "-",     "Shift+J"),
        ("Dimension / angle",  "D / N", "D", "D",       "-",                 "=", "K,D",         "-", "D",     "D (unchanged)"),
    ]),
    "k4-construction": ("K4. Construction and delete", [
        ("Toggle construction", "G",      "X",      "Q",                  "-",         "-",      "G,N*", "-",          "T",         "X"),
        ("Delete",              "Delete", "Delete", "Delete, Backspace",  "Backspace", "Delete*", "Delete", "X, Delete", "Delete, X", "Delete, Backspace"),
        ("Unselect all",        "Esc",    "Esc*",   "Space",              "Esc",       "Esc",    "Esc*", "Alt+A",      "Alt+A, Esc", "Alt+A, Esc"),
    ]),
    "k5-groups": ("K5. Groups: extrude, revolve, lathe, helix", [
        ("Extrude",            "Shift+X", "E",   "Shift+E", "E", "E*", "-", "E", "E",       "E"),
        ("Revolve",            "Shift+V", "-",   "Shift+W", "V", "-",  "-", "-", "Shift+R", "Shift+R"),
        ("Lathe (full turn)",  "Shift+L", "-",   "-",       "-", "-",  "-", "-", "-",       "Alt+R"),
        ("Helix",              "Shift+H", "-",   "-",       "-", "-",  "-", "-", "-",       "Alt+E"),
        ("Sketch in workplane", "Shift+W", "-",  "Shift+S", "-", "-",  "-", "-", "Shift+W", "Shift+W"),
        ("Patterns",           "Shift+T / Shift+R", "-", "-", "-", "-", "-", "-", "-",    "palette only"),
    ]),
    "k6-views": ("K6. Views without a numpad", [
        ("Front",            "F2 (3)", "-*", "Shift+1", "Ctrl+2", "Num1", "1", "Num1", "Num1", "Num1, Shift+1"),
        ("Back",             "-",      "-*", "Shift+2", "Ctrl+3", "Ctrl+Num1", "4", "Ctrl+Num1", "Ctrl+Num1", "Ctrl+Num1, Shift+2"),
        ("Left / right",     "-",      "-*", "Shift+3 / 4", "Ctrl+7 / 6", "Ctrl+Num3 / Num3", "6 / 3", "Ctrl+Num3 / Num3", "Ctrl+Num3 / Num3", "+ Shift+3 / 4"),
        ("Top / bottom",     "-",      "-*", "Shift+5 / 6", "Ctrl+4 / 5", "Num7 / Ctrl+Num7", "2 / 5", "Num7 / Ctrl+Num7", "Num7 / Ctrl+Num7", "+ Shift+5 / 6"),
        ("Isometric",        "F3 (3)", "-*", "Shift+7", "Ctrl+1", "-", "0", "-", "View pie", "Shift+7"),
        ("Look at sketch",   "W",      "-*", "N",       "-", "-", "V,... *", "Num7 (4)", "Tab (focus)", "W, Tab"),
    ]),
}

NOTES = {
    "k1-sketch": "(1) Onshape: switches line / tangent arc while drawing. A frees Ctrl+A select-all as everywhere but Blender.",
    "k2-hv-hide": "(2) Shapr3D: one Horizontal/Vertical tool. a = Onshape's keys (H/V constraints, Y hides); b = keep Blender's H family.",
    "k3-constraints": "Onshape's E (equal) is our extrude, so equal keeps Classic's Q. B and T both run Parallel / Tangent.",
    "k4-construction": "X toggles construction as in Fusion; X no longer deletes (Blender's habit). Backspace deletes as in Onshape/Shapr3D.",
    "k5-groups": "No app binds lathe or helix by default; Alt+R (revolve, all the way) and Alt+E (extrude, turning) are mnemonic proposals.",
    "k6-views": "(3) Classic's nearest ortho / iso. (4) Blender's top view. Shift+digits are free while typing values (only bare digits go to the fields).",
}


def font(size, mono=False):
    here = os.path.dirname(os.path.abspath(__file__))
    name = "JetBrainsMono-Regular.ttf" if mono else "Inter-Regular.ttf"
    return ImageFont.truetype(os.path.join(here, "..", "..", "res", "fonts", "ng", name), size)


def render(title, rows, note, path):
    f, fm, fb = font(15), font(14, mono=True), font(20)
    head = ["Action"] + APPS
    cols = [max(f.getlength(head[i]), *(fm.getlength(r[i]) for r in rows)) + 24
            for i in range(len(head))]
    rh, top = 30, 56
    w = int(sum(cols)) + 32
    h = top + rh * (len(rows) + 1) + 54
    im = Image.new("RGB", (w, h), (24, 26, 31))
    d = ImageDraw.Draw(im)
    d.text((16, 16), title, font=fb, fill=(235, 235, 240))
    y = top
    for n, row in enumerate([head] + rows):
        x = 16
        if n % 2 == 1:
            d.rectangle((16, y, w - 16, y + rh), fill=(32, 35, 42))
        for i, cell in enumerate(row):
            if i == len(row) - 1:
                d.rectangle((x - 4, y, x + cols[i] - 8, y + rh), fill=(36, 58, 92))
            colour = (150, 155, 165) if n == 0 else (225, 228, 235)
            if n > 0 and i == len(row) - 2:
                colour = (170, 200, 255)
            d.text((x + 4, y + 7), cell, font=f if (n == 0 or i == 0) else fm, fill=colour)
            x += cols[i]
        y += rh
    d.text((16, y + 14), note + "   * not re-checked against the app's own list.",
           font=font(13), fill=(160, 165, 175))
    im.save(path)


def markdown(title, rows, note):
    out = ["### " + title, "", "| Action | " + " | ".join(APPS) + " |",
           "|" + "---|" * (len(APPS) + 1)]
    for r in rows:
        out.append("| " + " | ".join(c.replace("|", "\\|") for c in r) + " |")
    out += ["", note, ""]
    return "\n".join(out)


def main(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for key, (title, rows) in TABLES.items():
        render(title, rows, NOTES[key], os.path.join(out_dir, f"keys-{key}.png"))
        print(markdown(title, rows, NOTES[key]))


if __name__ == "__main__":
    main(sys.argv[1])
