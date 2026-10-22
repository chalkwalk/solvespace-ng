#!/usr/bin/env python3
"""The keymap pass after M3-M5 (AGENTS.md section 5, "Keymap pass after the features land"):
each binding by how often it is used, a grammar for the qualifiers, and the keymap that
follows. One table per decision, as images for the owner's review round and as Markdown for
docs/research/keymap-pass.md.

    python3 tools/review/keymap_pass.py OUT_DIR      # writes pass-<id>.png, prints Markdown
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

# Each table: title, header, rows, note. A row whose first cell starts with "!" is a change
# (drawn highlighted; the "!" is dropped).
TABLES = {
    "p1-tiers": ("P1. How often each command is used (modelling printed parts)",
        ["Tier", "Commands", "Key rule"], [
        ("1  every few seconds",
         "line, dimension, coincident, horizontal, vertical, construction, delete, undo, "
         "grab, Esc, palette, sketch focus, extrude",
         "bare letter"),
        ("2  every minute or so",
         "circle, rectangle, arc, point, equal, parallel/tangent, perpendicular, midpoint, "
         "symmetric, fillet, views, frame, hide, save",
         "bare letter, or the letter's Shift/Alt"),
        ("3  a few times a session",
         "revolve, chamfer, hole, reference/angle dimension, tangent arc, spline, sketch on "
         "a face, new part, joints, fit joint, design pose, isolate, show all, panels",
         "a family's Shift/Alt, or a pie"),
        ("4  rarely",
         "lathe, helix, loft, sweep, shell, draft, patterns, split part, text, gear, "
         "measure, export, units, trace, mass properties, parts list, fit table",
         "palette and menus only"),
    ], "No telemetry exists; the tiers are judged from the workflow and from which commands "
       "Fusion, Onshape and Shapr3D give bare keys."),
    "p2-grammar": ("P2. The qualifier grammar: bare N is the command, the qualifiers its relatives",
        ["Qualifier", "Dimension", "Fits it now", "Breaks it now"], [
        ("bare", "the most used command of the letter's family",
         "L line, D dimension, E extrude, H, V", "-"),
        ("Shift", "its counterpart: inverse, reverse or reference",
         "Shift+D reference dim, Shift+Y show all", "Shift+L perpendicular, Shift+S symmetric, "
         "Shift+R revolve, Shift+F fillet, Shift+H hole, Shift+I isolate, Shift+W, Shift+T, "
         "Shift+J, Shift+P, Shift+G, Shift+X"),
        ("Alt", "its variant: same family, another shape",
         "Alt+D angle, Alt+R lathe, Alt+E helix", "Alt+C tangent arc (an arc, not a circle), "
         "Alt+A unselect"),
        ("Ctrl", "the document and the app, as everywhere",
         "Ctrl+S, Ctrl+Z, Ctrl+A, Ctrl+I link", "-"),
        ("pie", "a family too big for three keys: Shift+A add, ` views, K constrain, J joints",
         "Shift+A, `, F", "-"),
    ], "Today 12 Shift chords name a second command that merely starts with the letter (Onshape's "
       "habit); under the grammar each moves to its family's slot or to the palette."),
    "p3-moves": ("P3. What moves under the grammar (option a)",
        ["Command", "Now", "Proposed", "Why that slot"], [
        ("Fillet", "Shift+F", "F", "tier 2; F in Fusion and Shapr3D"),
        ("Chamfer", "-", "Alt+F", "fillet's variant"),
        ("Constrain pie", "F", "K", "FreeCAD's constraint letter; frees F"),
        ("Perpendicular", "Shift+L", "Alt+B", "parallel's variant"),
        ("Symmetric", "Shift+S", "Alt+M", "midpoint's variant (about a line)"),
        ("Tangent arc", "Alt+C", "Alt+A", "arc's variant"),
        ("Unselect all", "Alt+A, Esc", "Esc", "Alt+A is the tangent arc"),
        ("Revolve", "Shift+R", "Alt+E", "extrude's variant: the sketch swept round"),
        ("Extrude as a cut", "-", "Shift+E", "extrude's counterpart (a new command)"),
        ("Lathe / helix", "Alt+R / Alt+E", "palette", "tier 4"),
        ("Isolate", "Shift+I", "Alt+Y", "hide's variant (Shift+Y shows all)"),
        ("Sketch in new workplane", "Shift+W", "Alt+W", "W's variant: a new sketch plane"),
        ("Lock where dragged", "Shift+J", "K pie", "tier 4; J becomes joints"),
        ("Problems", "Shift+P", "F8", "VS Code's next problem; it opens itself anyway"),
        ("Grid", "Shift+G", "` pie", "a view setting"),
        ("Text", "Shift+T", "palette", "tier 4"),
    ], "Unchanged: L R C A P S, D Shift+D Alt+D, H V I B/T Q M U, X, G, Y Shift+Y, N O, W, Tab, "
       "Space, Shift+A, `, the digits and every Ctrl chord."),
    "p4-new": ("P4. Keys for what M3-M5 added",
        ["Command", "Now", "Proposed", "Why"], [
        ("!Joint pie: revolute, slider, cylindrical, planar, ball, gear, fit", "-", "J",
         "J for joint; one pie, as Shift+A"),
        ("!Design pose", "-", "Shift+J", "the joints' counterpart: all back to 0"),
        ("!New part", "-", "Ctrl+Shift+N", "Ctrl = the document; next to Ctrl+N"),
        ("Hole", "Shift+H", "Shift+H (exception)", "no family fits; tier 3 for printing"),
        ("Fillet / chamfer", "Shift+F / -", "F / Alt+F", "see P3"),
        ("Shell, draft, loft, sweep, split part", "-", "palette", "tier 4"),
        ("Mass properties, parts list, fit table", "-", "palette", "tier 4"),
    ], "The joint pie replaces six menu-only commands; G on a jointed part still moves it."),
    "p5-exceptions": ("P5. Qualified chords kept against the grammar",
        ["Chord", "Command", "Why keep it", "Else"], [
        ("Shift+A", "add pie", "Blender's, known to every Blender user", "-"),
        ("Shift+1..7", "standard views", "Onshape's; laptops have no numpad", "-"),
        ("Ctrl+Num1/3/7", "opposite views", "Blender's", "-"),
        ("Shift+H", "hole", "tier 3, no family slot", "palette"),
        ("Shift+X", "split curves", "trimming is tier 2-3 in sketches", "palette"),
        ("Ctrl+Shift+Z, Ctrl+Y", "redo", "everywhere", "-"),
    ], "Option a keeps all six; option b sends hole and split curves to the palette."),
}

# The letters after the pass (option a everywhere); "!" marks a changed cell.
LETTERS = [
    ("A", "arc", "add pie", "!tangent arc"),
    ("B", "parallel / tangent", "", "!perpendicular"),
    ("C", "circle", "", "!-"),
    ("D", "dimension", "reference dim", "angle"),
    ("E", "extrude", "!extrude cut", "!revolve"),
    ("F", "!fillet", "!-", "!chamfer"),
    ("G", "grab", "!-", ""),
    ("H", "horizontal", "hole", ""),
    ("I", "coincident / on", "!-", ""),
    ("J", "!joint pie", "!design pose", ""),
    ("K", "!constrain pie", "", ""),
    ("L", "line", "!-", ""),
    ("M", "midpoint", "", "!symmetric"),
    ("N", "Properties", "", ""),
    ("O", "Outliner", "", ""),
    ("P", "point", "!-", ""),
    ("Q", "equal", "", ""),
    ("R", "rectangle", "!-", "!-"),
    ("S", "spline", "!-", ""),
    ("T", "parallel / tangent", "!-", ""),
    ("U", "other angle", "", ""),
    ("V", "vertical", "", ""),
    ("W", "look at sketch", "!-", "!new workplane"),
    ("X", "construction", "split curves", ""),
    ("Y", "hide group", "show all", "!isolate"),
    ("Z", "", "", ""),
]
TABLES["p6-letters"] = ("P6. Every letter after the pass (option a)",
    ["Key", "Bare", "Shift", "Alt"], [], "Highlighted: changed. Free for later: Z, C/R/S/L Shift and "
    "Alt, G/I/P/T Shift. Ctrl chords, digits, Tab, Space, ` and . are as now.")

BG, ROW, HEAD, TEXT, CHANGED, HILITE = ((24, 26, 31), (32, 35, 42), (150, 155, 165),
                                         (225, 228, 235), (170, 200, 255), (36, 58, 92))


def font(size, mono=False):
    here = os.path.dirname(os.path.abspath(__file__))
    name = "JetBrainsMono-Regular.ttf" if mono else "Inter-Regular.ttf"
    return ImageFont.truetype(os.path.join(here, "..", "..", "res", "fonts", "ng", name), size)


def wrap(text, f, width):
    lines, line = [], ""
    for word in text.split(" "):
        trial = (line + " " + word).strip()
        if line and f.getlength(trial) > width:
            lines.append(line)
            line = word
        else:
            line = trial
    return lines + [line] if line else lines or [""]


def render(title, head, rows, note, path, marks=None):
    f, fb = font(15), font(20)
    widths = [min(max(f.getlength(head[i]), *(f.getlength(r[i]) for r in rows)) + 24, 470)
              for i in range(len(head))]
    w = int(sum(widths)) + 32
    cells = [[wrap(c, f, widths[i] - 20) for i, c in enumerate(r)] for r in [head] + rows]
    heights = [max(len(c) for c in r) * 20 + 10 for r in cells]
    notes = wrap(note, font(13), w - 32)
    h = 56 + sum(heights) + 24 + 18 * len(notes)
    im = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(im)
    d.text((16, 16), title, font=fb, fill=(235, 235, 240))
    y = 56
    for n, (row, rh) in enumerate(zip(cells, heights)):
        if n % 2 == 1:
            d.rectangle((16, y, w - 16, y + rh), fill=ROW)
        x = 16
        for i, lines in enumerate(row):
            changed = n > 0 and marks and marks[n - 1][i]
            if changed:
                d.rectangle((x, y, x + widths[i] - 4, y + rh), fill=HILITE)
            colour = HEAD if n == 0 else (CHANGED if changed else TEXT)
            for k, line in enumerate(lines):
                d.text((x + 6, y + 5 + 20 * k), line, font=f, fill=colour)
            x += widths[i]
        y += rh
    for k, line in enumerate(notes):
        d.text((16, y + 12 + 18 * k), line, font=font(13), fill=(160, 165, 175))
    im.save(path)


def split_marks(rows, whole_row):
    """Rows with "!" marks dropped, and which cells were marked."""
    clean, marks = [], []
    for r in rows:
        if whole_row:
            on = r[0].startswith("!")
            clean.append((r[0].lstrip("!"),) + tuple(r[1:]))
            marks.append([on] * len(r))
        else:
            clean.append(tuple(c.lstrip("!") for c in r))
            marks.append([c.startswith("!") for c in r])
    return clean, marks


def markdown(title, head, rows, note):
    out = ["### " + title, "", "| " + " | ".join(head) + " |", "|" + "---|" * len(head)]
    for r in rows:
        out.append("| " + " | ".join(c.replace("|", "\\|") or " " for c in r) + " |")
    return "\n".join(out + ["", note, ""])


def main(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for key, (title, head, rows, note) in TABLES.items():
        if key == "p6-letters":
            rows, marks = split_marks(LETTERS, whole_row=False)
        else:
            rows, marks = split_marks(rows, whole_row=True)
        render(title, head, rows, note, os.path.join(out_dir, f"pass-{key}.png"), marks)
        md_rows = [tuple(("**" + c + "**") if m and c else c for c, m in zip(r, mk))
                   for r, mk in zip(rows, marks)]
        print(markdown(title, head, md_rows, note))


if __name__ == "__main__":
    main(sys.argv[1])
