//-----------------------------------------------------------------------------
// Drives the real solvespace-ng app from test code, the way a person would: real SDL key, text
// and mouse events go through ImGui, the keymap, the modal tools and the core, and each call
// renders frames until the app is idle. Used by solvespace-ng-uitest (test/ngui/).
//
// In driver mode nothing ever blocks: message boxes are recorded and answered by a policy, file
// dialogs take queued paths, and nested loops (context menus) run queued steps.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_SDL_UIDRIVER_H
#define SOLVESPACE_SDL_UIDRIVER_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "solvespace.h"

namespace SolveSpace {
namespace Platform {
namespace Driver {

enum class Button { LEFT, MIDDLE, RIGHT };

struct DialogRecord {
    std::string              kind; // "message", "open", "save", "unanswered-modal"
    std::string              title, message;
    std::vector<std::string> buttons;
};

// Before Platform::InitGui(): selects SDL's offscreen video driver unless SDL_VIDEO_DRIVER is
// set, and keeps settings and the keymap in configDir instead of the user's.
void Enable(const Path &configDir);
bool Enabled();

// Per test case: forget recorded dialogs and queued answers, release held keys and buttons,
// and delete the settings and keymap files (call before SS.Init()).
void BeginCase();
// Problems the case left behind (unanswered modals, unused queued answers, an app that never
// went idle); empty if none.
std::vector<std::string> EndCase();

void Frame(int n = 1);
// Renders frames (at least three) until no events, deferred work or frame requests remain and
// no live timer is due within horizonMs.
void Settle(unsigned horizonMs = 1000);

// Chords use SDL key names with Ctrl+/Shift+/Alt+ prefixes: "Shift+A", "Return", "Keypad 1",
// "Ctrl+Shift+Z". Modifier keys are pressed first and released last, as on a keyboard, and a
// printable key also sends its text. KeyDown("Shift") / KeyUp("Shift") hold a modifier.
void Key(const std::string &chord);
void KeyDown(const std::string &chord);
void KeyUp(const std::string &chord);
void Type(const std::string &text); // one key press (with text) per character
// Holds a key down long enough for the keyboard to auto-repeat it `repeats` times.
void KeyRepeat(const std::string &chord, int repeats);

// Window coordinates in logical pixels, origin top-left.
void    Move(double x, double y);
void    Press(Button b = Button::LEFT); // at the current mouse position
void    Release(Button b = Button::LEFT);
void    Click(double x, double y, Button b = Button::LEFT);
void    DoubleClick(double x, double y);
void    Drag(double x0, double y0, double x1, double y1, Button b = Button::LEFT,
             int steps = 8);
void    Wheel(double x, double y, double dy);
// A touch screen (TouchGestures): a one-finger tap, and fingers put down together at `from`,
// moved together to `to` in `steps` frames, then lifted. One finger drags with the left
// button, two pan and pinch, three orbit.
void    Tap(double x, double y);
void    LongPress(double x, double y); // held still: a right click
void    TouchDrag(const std::vector<Point2d> &from, const std::vector<Point2d> &to,
                  int steps = 8);
Point2d MousePos();
Point2d WindowSize();
void    Resize(int width, int height); // as if the person resized the window
Point2d ToWindow(Vector modelPoint); // where a model point is drawn in the main window

// ImGui items (menu entries, buttons, palette rows, panels) visible in the last frame, by label
// without its "##id" suffix; scrolled out of view means not found. Windows are items too, e.g.
// "Property Browser".
struct Item {
    std::string label, id, window; // id: the "##id" part of the ImGui label, without hashes
    double      x0, y0, x1, y1;
    bool        checkable, checked, opened, openable, isWindow; // openable: a submenu, a header
    bool        fullyVisible;       // not cut off by its window's edge
    double      vx0, vy0, vx1, vy1; // the visible part
    // The middle of the visible part, where a person would click.
    Point2d Center() const { return Point2d::From((vx0 + vx1) / 2, (vy0 + vy1) / 2); }
};
std::vector<Item>   Items();
// The last widget drawn with the label (or, failing that, with the id), else the window with
// that name.
std::optional<Item> FindItem(const std::string &label);
bool ClickItem(const std::string &label);               // false if no such item
// Clicks through a menu path, e.g. { "Sketch", "Line Segment" }. False if a step is missing.
bool ClickMenu(const std::vector<std::string> &path);

bool Screenshot(const Path &png);
void ReloadKeymap(); // after writing keymap.txt or changing NG_KeymapPreset
Path ConfigDir();

std::vector<DialogRecord> &Dialogs();
// Answers message boxes; by default the default button (else the first) is pressed.
void OnMessage(std::function<MessageDialog::Response(const DialogRecord &)> policy);
void QueueFile(const Path &path); // the next file dialog returns it; none queued = cancel
// Runs inside the next nested loop (a context menu), e.g. to click one of its items.
void DuringModal(std::function<void()> step);

}
}
}

#endif
