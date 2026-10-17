//-----------------------------------------------------------------------------
// The tool shelf's contents.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "commandnames.h"
#include "platform/sdl/menumodel.h"
#include "ui/shelfmodel.h"

namespace SolveSpace {
namespace Ui {

static const struct {
    const char *section;
    Command     command;
    const char *icon;
} Buttons[] = {
    { "Sketch",    Command::LINE_SEGMENT,   "slash" },
    { "Sketch",    Command::RECTANGLE,      "rectangle-horizontal" },
    { "Sketch",    Command::CIRCLE,         "circle" },
    { "Sketch",    Command::ARC,            "arc" },
    { "Sketch",    Command::TANGENT_ARC,    "tangent-arc" },
    { "Sketch",    Command::CUBIC,          "spline" },
    { "Sketch",    Command::DATUM_POINT,    "dot-point" },
    { "Sketch",    Command::TTF_TEXT,       "type" },
    { "Sketch",    Command::IMAGE,          "image" },
    { "Sketch",    Command::CONSTRUCTION,   "construction-line" },
    { "Sketch",    Command::SPLIT_CURVES,   "scissors" },
    { "Constrain", Command::DISTANCE_DIA,   "ruler-dimension-line" },
    { "Constrain", Command::ANGLE,          "angle" },
    { "Constrain", Command::HORIZONTAL,     "horizontal" },
    { "Constrain", Command::VERTICAL,       "vertical" },
    { "Constrain", Command::PARALLEL,       "parallel" },
    { "Constrain", Command::PERPENDICULAR,  "perpendicular" },
    { "Constrain", Command::ON_ENTITY,      "point-on" },
    { "Constrain", Command::SYMMETRIC,      "symmetric" },
    { "Constrain", Command::EQUAL,          "equal" },
    { "Constrain", Command::ORIENTED_SAME,  "same-orientation" },
    { "Constrain", Command::OTHER_ANGLE,    "other-angle" },
    { "Constrain", Command::REFERENCE,      "parentheses" },
    { "Groups",    Command::GROUP_EXTRUDE,  "arrow-up-from-line" },
    { "Groups",    Command::GROUP_LATHE,    "rotate-3d" },
    { "Groups",    Command::GROUP_HELIX,    "helix" },
    { "Groups",    Command::GROUP_REVOLVE,  "rotate-ccw" },
    { "Groups",    Command::GROUP_ROT,      "rotate-cw-square" },
    { "Groups",    Command::GROUP_TRANS,    "copy" },
    { "Groups",    Command::GROUP_WRKPL,    "layers-plus" },
    { "Groups",    Command::GROUP_3D,       "axis-3d" },
    { "Groups",    Command::GROUP_LINK,     "link" },
    { "View",      Command::NEAREST_ISO,    "box" },
    { "View",      Command::ONTO_WORKPLANE, "scan" },
};

std::vector<ShelfSection> BuildShelf(const keymap::Keymap &keymap) {
    std::vector<ShelfSection> sections;
    for(const auto &b : Buttons) {
        if(sections.empty() || sections.back().title != b.section) {
            sections.push_back({ b.section, {} });
        }
        ShelfItem item;
        item.command = b.command;
        item.icon    = b.icon;
        std::string menu = GraphicsWindow::MenuLabel(b.command);
        item.label = menu.empty() ? CommandName(b.command) : Platform::StripMnemonic(menu);
        std::vector<keymap::Chord> chords = keymap.ChordsFor(CommandName(b.command));
        if(!chords.empty()) item.keys = keymap::FormatChord(chords[0]);
        sections.back().items.push_back(item);
    }
    return sections;
}

}
}
