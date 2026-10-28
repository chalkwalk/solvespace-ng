//-----------------------------------------------------------------------------
// NG's menu bar layout.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "commandnames.h"
#include "ui/menulayout.h"
#include "ui/uiactions.h"

namespace SolveSpace {
namespace Ui {

using Platform::MenuBarSdl;
using Platform::MenuItemSdl;
using Platform::MenuSdl;

namespace {

struct Spec {
    enum class Kind { CMD, ACTION, SEP, SUB, UNITS, RECENT_FILES, RECENT_LINKS };
    Kind              kind;
    Command           cmd    = Command::NONE;
    const char       *action = nullptr; // ACTION: the ui.* action
    const char       *label  = nullptr; // the NG label (CMD: nullptr keeps the core's)
    bool              check  = false;   // ACTION: a toggle with a check mark
    const char       *alias  = nullptr; // CMD: a keymap name whose keys also do this
    std::vector<Spec> children;
};

Spec C(Command c, const char *label = nullptr, const char *alias = nullptr) {
    return { Spec::Kind::CMD, c, nullptr, label, false, alias, {} };
}
Spec A(const char *action, const char *label, bool check = false) {
    return { Spec::Kind::ACTION, Command::NONE, action, label, check, nullptr, {} };
}
Spec Sep() {
    return { Spec::Kind::SEP };
}
Spec Sub(const char *label, std::vector<Spec> children) {
    return { Spec::Kind::SUB, Command::NONE, nullptr, label, false, nullptr, std::move(children) };
}
Spec Special(Spec::Kind k, const char *label) {
    return { k, Command::NONE, nullptr, label };
}

struct Top {
    const char       *title;
    std::vector<Spec> entries;
};

// Names: short and unambiguous (Arc, Spline, Text); 2D/3D; industry names where SolveSpace
// has its own (Quit, Preferences, Inspect, Linear/Circular Pattern).
const std::vector<Top> &Layout() {
    using K = Spec::Kind;
    static const std::vector<Top> layout = {
        { N_("File"), {
            C(Command::NEW, N_("New")), C(Command::OPEN, N_("Open...")),
            Special(K::RECENT_FILES, N_("Open Recent")),
            C(Command::SAVE, N_("Save")), C(Command::SAVE_AS, N_("Save As...")), Sep(),
            C(Command::IMPORT, N_("Import...")),
            Sub(N_("Export"), {
                C(Command::EXPORT_IMAGE, N_("Image...")), C(Command::EXPORT_VIEW, N_("2D View...")),
                C(Command::EXPORT_SECTION, N_("2D Section...")),
                C(Command::EXPORT_WIREFRAME, N_("3D Wireframe...")),
                C(Command::EXPORT_MESH, N_("Triangle Mesh...")),
                C(Command::EXPORT_PARTS, N_("Parts, One File Each...")),
                C(Command::EXPORT_PARTS_LIST, N_("Parts List (CSV)...")),
                C(Command::EXPORT_ASSEMBLY, N_("Assembly (STEP)...")),
                C(Command::EXPORT_SURFACES, N_("Surfaces...")) }),
            Sep(), C(Command::EXIT, N_("Quit")) } },
        { N_("Edit"), {
            C(Command::UNDO, N_("Undo")), C(Command::REDO, N_("Redo")), Sep(),
            C(Command::CUT, N_("Cut")), C(Command::COPY, N_("Copy")),
            C(Command::PASTE, N_("Paste")),
            C(Command::PASTE_TRANSFORM, N_("Paste Transformed...")),
            C(Command::DELETE, N_("Delete")),
            Sep(),
            C(Command::SELECT_ALL, N_("Select All")), C(Command::UNSELECT_ALL, N_("Unselect All")),
            C(Command::SELECT_CHAIN, N_("Select Edge Chain")), Sep(),
            A("ui.grab", N_("Grab")), C(Command::SNAP_TO_GRID, N_("Snap to Grid")),
            C(Command::ROTATE_90, N_("Rotate Imported 90\xC2\xB0")),
            C(Command::REGEN_ALL, N_("Regenerate All")), Sep(),
            C(Command::EDIT_LINE_STYLES, N_("Line Styles...")),
            C(Command::CONFIGURATION, N_("Preferences...")) } },
        { N_("View"), {
            C(Command::ZOOM_IN, N_("Zoom In")), C(Command::ZOOM_OUT, N_("Zoom Out")),
            C(Command::ZOOM_TO_FIT, N_("Zoom to Fit")),
            A("ui.view-frame-selected", N_("Frame Selection")),
            C(Command::CENTER_VIEW, N_("Center View at Point")), Sep(),
            Sub(N_("Standard Views"), {
                A("ui.view-front", N_("Front")), A("ui.view-back", N_("Back")),
                A("ui.view-right", N_("Right")), A("ui.view-left", N_("Left")),
                A("ui.view-top", N_("Top")), A("ui.view-bottom", N_("Bottom")), Sep(),
                C(Command::NEAREST_ISO, N_("Nearest Isometric")),
                C(Command::NEAREST_ORTHO, N_("Nearest Orthographic")),
                C(Command::ONTO_WORKPLANE, N_("Align to Workplane")) }),
            Sep(),
            C(Command::PERSPECTIVE_PROJ, N_("Perspective")),
            C(Command::EXPLODE_SKETCH, N_("Exploded View")),
            C(Command::DIM_SOLID_MODEL, N_("Darken Inactive Solids")),
            C(Command::SHOW_GRID, N_("Snap Grid")), Special(K::UNITS, N_("Units")), Sep(),
            A("ui.toggle-outliner", N_("Outliner"), true),
            A("ui.toggle-properties", N_("Properties"), true),
            A("ui.toggle-problems", N_("Problems"), true),
            C(Command::SHOW_TOOLBAR, N_("Tool Shelf")),
            A("ui.toggle-menu-bar", N_("Menu Bar"), true),
            A("ui.command-palette", N_("Command Palette...")), Sep(),
            A("ui.hide", N_("Hide Group")), A("ui.isolate", N_("Isolate Group")),
            A("ui.unhide-all", N_("Show All Groups")), Sep(),
            C(Command::VIEW_PROJECTION, N_("View Settings...")),
            A("ui.toggle-theme", N_("Light Theme"), true),
            C(Command::FULL_SCREEN, N_("Full Screen")) } },
        { N_("New Group"), {
            C(Command::PART_NEW, N_("New Part")), C(Command::GROUP_SPLIT, N_("Split Part")),
            Sub(N_("Joints"), {
                C(Command::JOINT_REVOLUTE, N_("Revolute Joint")),
                C(Command::JOINT_SLIDER, N_("Slider Joint")),
                C(Command::JOINT_CYLINDRICAL, N_("Cylindrical Joint")),
                C(Command::JOINT_PLANAR, N_("Planar Joint")),
                C(Command::JOINT_BALL, N_("Ball Joint")), Sep(),
                C(Command::JOINT_GEAR, N_("Gear Coupling")), Sep(),
                C(Command::GROUP_FIT, N_("Fit Joint")) }), Sep(),
            C(Command::GROUP_3D, N_("Sketch in 3D")),
            C(Command::GROUP_WRKPL, N_("Sketch in New Workplane")), Sep(),
            C(Command::GROUP_EXTRUDE, N_("Extrude")),
            C(Command::GROUP_EXTRUDE_CUT, N_("Extrude as a Cut")),
            C(Command::GROUP_REVOLVE, N_("Revolve")),
            C(Command::GROUP_LATHE, N_("Lathe")), C(Command::GROUP_HELIX, N_("Helix")), Sep(),
            C(Command::GROUP_FILLET, N_("Fillet Edges")),
            C(Command::GROUP_CHAMFER, N_("Chamfer Edges")),
            C(Command::GROUP_SHELL, N_("Shell")), C(Command::GROUP_DRAFT, N_("Draft Faces")),
            C(Command::GROUP_HOLE, N_("Hole")), C(Command::GROUP_LOFT, N_("Loft")),
            C(Command::GROUP_SWEEP, N_("Sweep")),
            Sep(),
            C(Command::GROUP_TRANS, N_("Linear Pattern")),
            C(Command::GROUP_ROT, N_("Circular Pattern")), Sep(),
            C(Command::GROUP_LINK, N_("Link Part...")),
            Special(K::RECENT_LINKS, N_("Link Recent")) } },
        { N_("Sketch"), {
            C(Command::SEL_WORKPLANE, N_("In Workplane")),
            C(Command::FREE_IN_3D, N_("Anywhere in 3D")),
            Sep(),
            C(Command::DATUM_POINT, N_("Point")), C(Command::WORKPLANE, N_("Workplane")), Sep(),
            C(Command::LINE_SEGMENT, N_("Line")),
            C(Command::CONSTR_SEGMENT, N_("Construction Line")),
            C(Command::RECTANGLE, N_("Rectangle")), C(Command::CIRCLE, N_("Circle")),
            C(Command::ARC, N_("Arc")), C(Command::CUBIC, N_("Spline")), Sep(),
            C(Command::TTF_TEXT, N_("Text")), C(Command::IMAGE, N_("Image...")), Sep(),
            C(Command::CONSTRUCTION, N_("Toggle Construction")),
            C(Command::TANGENT_ARC, N_("Tangent Arc at Point")),
            C(Command::SPLIT_CURVES, N_("Split at Intersections")) } },
        { N_("Constrain"), {
            C(Command::DISTANCE_DIA, N_("Distance / Diameter"), "ui.smart-dimension"),
            C(Command::REF_DISTANCE, N_("Reference Dimension")),
            C(Command::ANGLE, N_("Angle / Equal Angle")),
            C(Command::REF_ANGLE, N_("Reference Angle")),
            C(Command::OTHER_ANGLE, N_("Supplementary Angle")),
            C(Command::REFERENCE, N_("Toggle Reference")), Sep(),
            C(Command::HORIZONTAL, N_("Horizontal")), C(Command::VERTICAL, N_("Vertical")), Sep(),
            C(Command::ON_ENTITY, N_("On Point / Curve / Plane")),
            C(Command::EQUAL, N_("Equal Length / Radius")),
            C(Command::RATIO, N_("Length / Arc Ratio")),
            C(Command::DIFFERENCE, N_("Length / Arc Difference")),
            C(Command::AT_MIDPOINT, N_("Midpoint")), C(Command::SYMMETRIC, N_("Symmetric")),
            C(Command::PARALLEL, N_("Parallel / Tangent")),
            C(Command::PERPENDICULAR, N_("Perpendicular")),
            C(Command::ORIENTED_SAME, N_("Same Orientation")),
            C(Command::WHERE_DRAGGED, N_("Lock Point Where Dragged")), Sep(),
            C(Command::COMMENT, N_("Comment")) } },
        { N_("Inspect"), {
            C(Command::VOLUME, N_("Measure Volume")), C(Command::AREA, N_("Measure Area")),
            C(Command::PERIMETER, N_("Measure Perimeter")),
            C(Command::INTERFERENCE, N_("Interfering Parts")),
            C(Command::NAKED_EDGES, N_("Naked Edges")),
            C(Command::CENTER_OF_MASS, N_("Center of Mass")),
            C(Command::MASS_PROPERTIES, N_("Mass Properties...")),
            C(Command::PARTS_LIST, N_("Parts List...")),
            C(Command::DESIGN_POSE, N_("Design Pose")), A("ui.fit-table", N_("Fit Table...")),
            Sep(),
            C(Command::SHOW_DOF, N_("Underconstrained Points")), Sep(),
            C(Command::TRACE_PT, N_("Trace Point")),
            C(Command::STOP_TRACING, N_("Stop Tracing...")),
            C(Command::STEP_DIM, N_("Step Dimension...")) } },
        { N_("Help"), {
            A("ui.show-shortcuts", N_("Keyboard Shortcuts")), Sep(),
            C(Command::WEBSITE, N_("Website / Manual")), C(Command::GITHUB, N_("GitHub Commit")),
            C(Command::ABOUT, N_("About")) } },
    };
    return layout;
}

// Where each of the core's entries is, so it can be moved.
struct Found {
    std::map<uint32_t, std::shared_ptr<MenuItemSdl>> items;
    std::shared_ptr<MenuSdl>                         units, recentFiles, recentLinks;
    std::set<const void *>                           placed;
};

void Collect(const MenuSdl &menu, Found *found) {
    for(const MenuSdl::Entry &e : menu.entries) {
        if(e.kind == MenuSdl::Entry::Kind::ITEM && e.item->commandId) {
            found->items.emplace(e.item->commandId, e.item);
        } else if(e.kind == MenuSdl::Entry::Kind::SUBMENU) {
            const void *p = e.submenu.get();
            if(SS.GW.openRecentMenu && p == SS.GW.openRecentMenu.get()) found->recentFiles = e.submenu;
            if(SS.GW.linkRecentMenu && p == SS.GW.linkRecentMenu.get()) found->recentLinks = e.submenu;
            for(const MenuSdl::Entry &c : e.submenu->entries) {
                if(c.kind == MenuSdl::Entry::Kind::ITEM &&
                   c.item->commandId == (uint32_t)Command::UNITS_MM) {
                    found->units = e.submenu;
                }
            }
            if(p != found->units.get()) Collect(*e.submenu, found);
        }
    }
}

void AddSeparator(MenuSdl *m) {
    if(!m->entries.empty() && m->entries.back().kind != MenuSdl::Entry::Kind::SEPARATOR) {
        m->entries.push_back({ MenuSdl::Entry::Kind::SEPARATOR, nullptr, nullptr, "" });
    }
}

void TrimSeparator(MenuSdl *m) {
    while(!m->entries.empty() && m->entries.back().kind == MenuSdl::Entry::Kind::SEPARATOR) {
        m->entries.pop_back();
    }
}

void Build(const std::vector<Spec> &specs, MenuSdl *out, Found *found,
           const std::function<bool(const std::string &)> &checked) {
    using K = Spec::Kind;
    for(const Spec &s : specs) {
        switch(s.kind) {
            case K::SEP: AddSeparator(out); break;
            case K::CMD: {
                auto it = found->items.find((uint32_t)s.cmd);
                if(it == found->items.end()) break; // not in this build
                std::shared_ptr<MenuItemSdl> item = it->second;
                if(s.label && item->label != _(s.label)) {
                    if(item->searchAlias.empty()) item->searchAlias = item->label;
                    item->label = _(s.label);
                }
                if(s.alias) item->keyAlias = s.alias;
                found->placed.insert(item.get());
                out->entries.push_back({ MenuSdl::Entry::Kind::ITEM, item, nullptr, item->label });
                break;
            }
            case K::ACTION: {
                auto item    = std::make_shared<MenuItemSdl>();
                item->label  = _(s.label);
                item->action = s.action;
                // The palette still finds it by its action-list name ("Toggle Theme").
                for(const UiActionInfo &a : UiActionList()) {
                    if(a.name != item->action) continue;
                    std::string name  = a.label;
                    size_t      arrow = name.rfind("\xE2\x80\xBA ");
                    if(arrow != std::string::npos) name = name.substr(arrow + 4);
                    if(name != item->label) item->searchAlias = name;
                }
                std::string name = s.action;
                item->onTrigger  = [name] { Actions().Run(name); };
                if(s.check) {
                    item->indicator = Platform::MenuItem::Indicator::CHECK_MARK;
                    item->isActive  = [checked, name] { return checked && checked(name); };
                }
                out->entries.push_back({ MenuSdl::Entry::Kind::ITEM, item, nullptr, item->label });
                break;
            }
            case K::SUB: {
                auto menu = std::make_shared<MenuSdl>();
                Build(s.children, menu.get(), found, checked);
                TrimSeparator(menu.get());
                if(!menu->entries.empty()) {
                    out->entries.push_back(
                        { MenuSdl::Entry::Kind::SUBMENU, nullptr, menu, _(s.label) });
                }
                break;
            }
            case K::UNITS:
            case K::RECENT_FILES:
            case K::RECENT_LINKS: {
                std::shared_ptr<MenuSdl> menu = s.kind == K::UNITS        ? found->units
                                              : s.kind == K::RECENT_FILES ? found->recentFiles
                                                                          : found->recentLinks;
                if(!menu) break;
                found->placed.insert(menu.get());
                out->entries.push_back(
                    { MenuSdl::Entry::Kind::SUBMENU, nullptr, menu, _(s.label) });
                break;
            }
        }
    }
}

// The core's entries the layout didn't place (new commands, the Language submenu).
void Leftovers(const MenuSdl &menu, const Found &found, std::vector<MenuSdl::Entry> *out) {
    for(const MenuSdl::Entry &e : menu.entries) {
        if(e.kind == MenuSdl::Entry::Kind::ITEM) {
            if(!found.placed.count(e.item.get())) out->push_back(e);
        } else if(e.kind == MenuSdl::Entry::Kind::SUBMENU) {
            if(found.placed.count(e.submenu.get())) continue;
            bool holdsPlaced = false;
            for(const MenuSdl::Entry &c : e.submenu->entries) {
                holdsPlaced |= c.kind == MenuSdl::Entry::Kind::ITEM && found.placed.count(c.item.get());
            }
            if(holdsPlaced) {
                Leftovers(*e.submenu, found, out);
            } else {
                MenuSdl::Entry copy = e;
                copy.label = Platform::StripMnemonic(e.label);
                out->push_back(copy);
            }
        }
    }
}

} // namespace

static const char *FindLabel(const std::vector<Spec> &specs, Command c) {
    for(const Spec &s : specs) {
        if(s.kind == Spec::Kind::CMD && s.cmd == c && s.label) return s.label;
        if(const char *l = FindLabel(s.children, c)) return l;
    }
    return nullptr;
}

std::string CommandLabel(Command c) {
    for(const Top &top : Layout()) {
        if(const char *l = FindLabel(top.entries, c)) return _(l);
    }
    std::string core = GraphicsWindow::MenuLabel(c);
    return core.empty() ? CommandName(c) : Platform::StripMnemonic(core);
}

std::string ShortcutText(const MenuItemSdl &item, const keymap::Keymap &keymap) {
    std::vector<std::string> names;
    if(!item.action.empty()) names.push_back(item.action);
    if(item.commandId) {
        if(const char *n = CommandName((Command)item.commandId)) names.push_back(n);
    }
    if(!item.keyAlias.empty()) names.push_back(item.keyAlias);
    for(const std::string &n : names) {
        std::vector<keymap::Chord> chords = keymap.ChordsFor(n);
        if(!chords.empty()) return keymap::FormatChord(chords[0]);
    }
    return "";
}

bool RemovedFromMenus(Command c) {
    return c == Command::SHOW_TEXT_WND;
}

void ApplyMenuLayout(MenuBarSdl &bar,
                     std::function<bool(const std::string &action)> actionChecked) {
    Found found;
    for(auto &m : bar.menus) Collect(*m.second, &found);
    // Dropped: counted as placed, so they aren't left over either.
    for(auto &[id, item] : found.items) {
        if(RemovedFromMenus((Command)id)) found.placed.insert(item.get());
    }
    std::vector<std::pair<std::string, std::shared_ptr<MenuSdl>>> menus;
    for(const Top &top : Layout()) {
        auto menu = std::make_shared<MenuSdl>();
        Build(top.entries, menu.get(), &found, actionChecked);
        TrimSeparator(menu.get());
        menus.push_back({ _(top.title), menu });
    }
    // Whatever wasn't placed stays with its menu: the one of the same name, else a menu of
    // its own at the end.
    for(auto &old : bar.menus) {
        std::vector<MenuSdl::Entry> rest;
        Leftovers(*old.second, found, &rest);
        if(rest.empty()) continue;
        std::string title = Platform::StripMnemonic(old.first);
        auto it = std::find_if(menus.begin(), menus.end(),
                               [&](const auto &m) { return m.first == title; });
        if(it == menus.end()) {
            menus.push_back({ title, std::make_shared<MenuSdl>() });
            it = menus.end() - 1;
        }
        AddSeparator(it->second.get());
        for(MenuSdl::Entry &e : rest) it->second->entries.push_back(e);
    }
    menus.erase(std::remove_if(menus.begin(), menus.end(),
                               [](const auto &m) { return m.second->entries.empty(); }),
                menus.end());
    bar.menus = std::move(menus);
}

}
}

namespace SolveSpace {
namespace Ui {

void ApplyContextMenuLayout(MenuSdl &menu, bool itemsSelected) {
    static const std::pair<const char *, const char *> renamed[] = {
        { "Group Info", "Group Properties" }, { "Style Info", "Style Properties" } };
    for(MenuSdl::Entry &e : menu.entries) {
        if(e.kind != MenuSdl::Entry::Kind::ITEM) continue;
        for(const auto &r : renamed) {
            if(e.label != r.first) continue;
            e.label = e.item->label = r.second;
            e.item->searchAlias     = r.first;
        }
    }
    if(!itemsSelected) return;
    // What one does next with a selection, with the keys that do it on the canvas.
    static const std::pair<const char *, const char *> actions[] = {
        { "Grab", "ui.grab" },
        { "Dimension", "ui.smart-dimension" },
        { "Constrain...", "ui.pie-constrain" },
        { "Hide Group", "ui.hide" } };
    std::vector<MenuSdl::Entry> first;
    for(const auto &a : actions) {
        auto        item = std::make_shared<MenuItemSdl>();
        std::string name = a.second;
        item->label      = a.first;
        item->action     = name;
        item->onTrigger  = [name] { RunNamedCommand(name); };
        first.push_back({ MenuSdl::Entry::Kind::ITEM, item, nullptr, item->label });
    }
    first.push_back({ MenuSdl::Entry::Kind::SEPARATOR, nullptr, nullptr, "" });
    menu.entries.insert(menu.entries.begin(), first.begin(), first.end());
}

size_t MenusThatFit(const std::vector<float> &widths, float available, float moreWidth) {
    float total = 0;
    for(float w : widths) total += w;
    if(total <= available) return widths.size();
    size_t n    = 0;
    float  used = moreWidth;
    while(n < widths.size() && used + widths[n] <= available) used += widths[n++];
    return n;
}

}
}
