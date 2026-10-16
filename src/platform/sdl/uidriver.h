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
// Problems the case left behind (unanswered modals, unused queued answers); empty if none.
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

// Window coordinates in logical pixels, origin top-left.
void    Move(double x, double y);
void    Press(Button b = Button::LEFT); // at the current mouse position
void    Release(Button b = Button::LEFT);
void    Click(double x, double y, Button b = Button::LEFT);
void    DoubleClick(double x, double y);
void    Drag(double x0, double y0, double x1, double y1, Button b = Button::LEFT,
             int steps = 8);
void    Wheel(double x, double y, double dy);
Point2d MousePos();
Point2d WindowSize();
Point2d ToWindow(Vector modelPoint); // where a model point is drawn in the main window

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
