#!/usr/bin/env python3
# Black-box tests of the real solvespace-ng binary through a real X server: each scenario starts
# the app on Xvfb, drives it with xdotool (the same X11 input path a keyboard and mouse take),
# saves with Ctrl+S, quits with Ctrl+Q, and checks the saved .slvs file. Nothing reaches inside
# the app, so this also covers SDL's X11 backend, window focus and the real event timing.
#
# Usage: run.py path/to/solvespace-ng [scenario-substring]
# Needs Xvfb and xdotool; ImageMagick (`magick` or `convert`) for the screenshot checks.
# Exits 0 when every scenario passes, 1 on a failure, 77 when a tool is missing (skipped).
# NGX11_DELAY scales the pause after each input (default 0.15 s); NGX11_SHOW=:0 runs on an
# existing display instead of Xvfb, to watch.

import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DELAY = float(os.environ.get("NGX11_DELAY", "0.15"))

# .slvs ids (src/sketch.h)
LINE, CIRCLE = 200, 400
COINCIDENT, DISTANCE, HORIZONTAL, VERTICAL, DIAMETER, ANGLE = 20, 30, 80, 81, 90, 120


def read_slvs(path):
    """Groups, requests and constraints as lists of {field: value} dicts."""
    out = {"Group": [], "Request": [], "Constraint": []}
    fields = {}
    with open(path, "rb") as f:
        for raw in f.read().decode("latin-1").splitlines():
            if raw.startswith("Add") and raw[3:] in out:
                out[raw[3:]].append(fields)
                fields = {}
            elif "=" in raw:
                key, value = raw.split("=", 1)
                fields[key.split(".", 1)[-1]] = value
    return out


def count(items, type_id):
    return sum(1 for i in items if int(i.get("type", -1)) == type_id)


def values(items, type_id):
    return sorted(round(float(i.get("valA", "nan")), 6) for i in items
                  if int(i.get("type", -1)) == type_id)


class Failure(Exception):
    pass


def check(cond, what):
    if not cond:
        raise Failure(what)


class App:
    """One run of the app on `display`, editing a copy of a fixture."""

    def __init__(self, binary, display, workdir, fixture="line.slvs"):
        self.display = display
        self.file = os.path.join(workdir, "model.slvs")
        shutil.copy(os.path.join(HERE, fixture), self.file)
        env = dict(os.environ, DISPLAY=display, SDL_VIDEO_DRIVER="x11",
                   XDG_DATA_HOME=os.path.join(workdir, "xdg"))
        self.proc = subprocess.Popen([binary, self.file], env=env, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.DEVNULL)
        self.window = self.xdo("search", "--sync", "--onlyvisible", "--name", "model.slvs")
        self.window = self.window.split()[0]
        self.xdo("windowfocus", "--sync", self.window)
        time.sleep(4 * DELAY)

    def xdo(self, *args):
        env = dict(os.environ, DISPLAY=self.display)
        return subprocess.run(["xdotool", *args], env=env, check=True, timeout=20,
                              capture_output=True, text=True).stdout

    def pause(self, n=1):
        time.sleep(n * DELAY)

    # Input, in window coordinates.
    def move(self, x, y):
        self.xdo("mousemove", "--window", self.window, str(x), str(y))
        self.pause()

    def click(self, x, y, button=1):
        self.move(x, y)
        self.xdo("click", str(button))
        self.pause()

    def double_click(self, x, y):
        self.move(x, y)
        self.xdo("click", "--repeat", "2", "--delay", "60", "1")
        self.pause()

    def key(self, *chords):
        for chord in chords:
            self.xdo("key", "--clearmodifiers", chord)
            self.pause()

    def type(self, text):
        self.xdo("type", "--delay", "40", text)
        self.pause()

    def screenshot_pixel(self, x, y):
        """The window pixel at (x, y) as (r, g, b), or None without ImageMagick."""
        tool = shutil.which("magick") or shutil.which("convert")
        if not tool or not shutil.which("import"):
            return None
        shot = self.file + ".png"
        env = dict(os.environ, DISPLAY=self.display)
        subprocess.run(["import", "-window", self.window, shot], env=env, check=True,
                       timeout=20)
        fmt = ",".join("%%[fx:int(255*p{%d,%d}.%s)]" % (x, y, c) for c in "rgb")
        text = subprocess.run([tool, shot, "-format", fmt, "info:"], check=True, timeout=20,
                              capture_output=True, text=True).stdout
        return tuple(int(v) for v in text.strip().split(","))

    def save_and_quit(self):
        before = os.path.getmtime(self.file)
        self.key("ctrl+s")
        for _ in range(50):
            if os.path.getmtime(self.file) != before:
                break
            time.sleep(0.1)
        check(os.path.getmtime(self.file) != before, "Ctrl+S did not save the file")
        self.key("ctrl+q")
        try:
            self.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            raise Failure("Ctrl+Q did not quit (a dialog left open?)")
        return read_slvs(self.file)

    def kill(self):
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()


# Scenarios: each gets a started App and returns nothing; failures raise. Coordinates are in the
# 1280x800 window over the fixture's empty canvas (its line is near the top).

def typed_line_makes_length_and_angle(app):
    app.move(640, 500)
    app.key("l")
    app.click(560, 600)
    app.move(660, 560)
    app.type("25")
    app.key("Tab")
    app.type("30")
    app.key("Return", "Escape")
    m = app.save_and_quit()
    check(values(m["Constraint"], DISTANCE) == [25.0], "no 25 mm length")
    check(values(m["Constraint"], ANGLE) == [30.0], "no 30 degree angle")


def right_click_ends_a_chain_without_an_extra_segment(app):
    app.key("l")
    app.click(560, 600)
    app.click(660, 600)
    app.click(660, 680)
    app.click(720, 720, button=3)
    m = app.save_and_quit()
    check(count(m["Request"], LINE) == 1 + 2, "expected the fixture's line and two segments, "
          "got %d lines" % count(m["Request"], LINE))


def double_click_ends_a_chain_there(app):
    app.key("l")
    app.click(560, 600)
    app.double_click(660, 600)
    app.move(700, 700)
    app.key("Escape")
    m = app.save_and_quit()
    check(count(m["Request"], LINE) == 1 + 1, "got %d lines" % count(m["Request"], LINE))


def backspace_takes_back_the_last_segment(app):
    app.key("l")
    app.click(560, 600)
    app.click(660, 600)
    app.click(660, 680)
    app.key("BackSpace")
    app.click(600, 700)
    app.key("Escape")
    m = app.save_and_quit()
    check(count(m["Request"], LINE) == 1 + 2, "got %d lines" % count(m["Request"], LINE))


def palette_draws_a_typed_circle(app):
    app.move(640, 500)
    app.key("space")
    app.type("circle")
    app.key("Return")
    app.click(600, 620)
    app.move(630, 620)
    app.type("8")
    app.key("Return")
    m = app.save_and_quit()
    check(count(m["Request"], CIRCLE) == 1, "no circle")
    check(values(m["Constraint"], DIAMETER) == [8.0], "no 8 mm diameter")


def undo_and_redo_from_the_keyboard(app):
    app.key("l")
    app.click(560, 600)
    app.click(660, 600)
    app.key("Escape")
    app.key("l")
    app.click(560, 680)
    app.click(660, 680)
    app.key("Escape", "ctrl+z", "ctrl+z", "ctrl+shift+z")
    m = app.save_and_quit()
    check(count(m["Request"], LINE) == 1 + 1, "got %d lines" % count(m["Request"], LINE))


def typing_a_group_name_runs_no_commands(app):
    # The Properties panel's name field; every letter of "plate" is also a tool key.
    app.click(1090, 340)  # Properties sits below the Outliner on the right (layout c)
    app.key("ctrl+a")
    app.type("plate")
    app.key("Return")
    app.move(640, 600)
    m = app.save_and_quit()
    names = [g.get("name") for g in m["Group"]]
    check("plate" in names, "group not renamed: %s" % names)
    check(count(m["Request"], LINE) == 1, "typing drew something")


def light_theme_lightens_the_canvas(app):
    dark = app.screenshot_pixel(500, 700)
    if dark is None:
        print("    (no ImageMagick: pixel checks skipped)")
    app.key("space")
    app.type("toggle theme")
    app.key("Return")
    app.pause(4)
    light = app.screenshot_pixel(500, 700)
    if dark is not None:
        check(max(dark) < 60, "canvas not dark at start: %s" % (dark,))
        check(min(light) > 200, "canvas not light after the toggle: %s" % (light,))
    app.save_and_quit()


SCENARIOS = [
    typed_line_makes_length_and_angle,
    right_click_ends_a_chain_without_an_extra_segment,
    double_click_ends_a_chain_there,
    backspace_takes_back_the_last_segment,
    palette_draws_a_typed_circle,
    undo_and_redo_from_the_keyboard,
    typing_a_group_name_runs_no_commands,
    light_theme_lightens_the_canvas,
]


def start_xvfb():
    for n in range(90, 140):
        if os.path.exists("/tmp/.X%d-lock" % n):
            continue
        proc = subprocess.Popen(["Xvfb", ":%d" % n, "-screen", "0", "1400x900x24",
                                 "-nolisten", "tcp"], stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        for _ in range(50):
            if os.path.exists("/tmp/.X11-unix/X%d" % n):
                return proc, ":%d" % n
            if proc.poll() is not None:
                break
            time.sleep(0.1)
        proc.kill()
    raise RuntimeError("could not start Xvfb")


def main():
    if len(sys.argv) < 2:
        print(__doc__ or "usage: run.py solvespace-ng [filter]")
        return 2
    binary = os.path.abspath(sys.argv[1])
    only = sys.argv[2] if len(sys.argv) > 2 else ""
    show = os.environ.get("NGX11_SHOW")
    for tool in ["xdotool"] + ([] if show else ["Xvfb"]):
        if not shutil.which(tool):
            print("x11 tests skipped: %s not found" % tool)
            return 77
    xvfb, display = (None, show) if show else start_xvfb()
    failed = 0
    try:
        for scenario in SCENARIOS:
            if only not in scenario.__name__:
                continue
            with tempfile.TemporaryDirectory(prefix="ngx11-") as workdir:
                app = None
                try:
                    app = App(binary, display, workdir)
                    scenario(app)
                    print("  OK   x11/%s" % scenario.__name__)
                except (Failure, subprocess.SubprocessError, OSError) as e:
                    failed += 1
                    print("  NG   x11/%s: %s" % (scenario.__name__, e))
                finally:
                    if app:
                        app.kill()
    finally:
        if xvfb:
            # SIGTERM, so Xvfb removes its /tmp/.X<n>-lock; killed, it leaves the display taken.
            xvfb.terminate()
            try:
                xvfb.wait(timeout=10)
            except subprocess.TimeoutExpired:
                xvfb.kill()
                xvfb.wait()
    print("x11: %s" % ("all passed" if not failed else "%d failed" % failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
