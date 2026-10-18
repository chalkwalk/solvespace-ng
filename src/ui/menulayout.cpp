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
        { "File", {
            C(Command::NEW, "New"), C(Command::OPEN, "Open..."),
            Special(K::RECENT_FILES, "Open Recent"),
            C(Command::SAVE, "Save"), C(Command::SAVE_AS, "Save As..."), Sep(),
            C(Command::IMPORT, "Import..."),
            Sub("Export", {
                C(Command::EXPORT_IMAGE, "Image..."), C(Command::EXPORT_VIEW, "2D View..."),
                C(Command::EXPORT_SECTION, "2D Section..."),
                C(Command::EXPORT_WIREFRAME, "3D Wireframe..."),
                C(Command::EXPORT_MESH, "Triangle Mesh..."),
                C(Command::EXPORT_SURFACES, "Surfaces...") }),
            Sep(), C(Command::EXIT, "Quit") } },
        { "Edit", {
            C(Command::UNDO, "Undo"), C(Command::REDO, "Redo"), Sep(),
            C(Command::CUT, "Cut"), C(Command::COPY, "Copy"), C(Command::PASTE, "Paste"),
            C(Command::PASTE_TRANSFORM, "Paste Transformed..."), C(Command::DELETE, "Delete"),
            Sep(),
            C(Command::SELECT_ALL, "Select All"), C(Command::UNSELECT_ALL, "Unselect All"),
            C(Command::SELECT_CHAIN, "Select Edge Chain"), Sep(),
            A("ui.grab", "Grab"), C(Command::SNAP_TO_GRID, "Snap to Grid"),
            C(Command::ROTATE_90, "Rotate Imported 90\xC2\xB0"),
            C(Command::REGEN_ALL, "Regenerate All"), Sep(),
            C(Command::EDIT_LINE_STYLES, "Line Styles..."),
            C(Command::CONFIGURATION, "Preferences...") } },
        { "View", {
            C(Command::ZOOM_IN, "Zoom In"), C(Command::ZOOM_OUT, "Zoom Out"),
            C(Command::ZOOM_TO_FIT, "Zoom to Fit"),
            A("ui.view-frame-selected", "Frame Selection"),
            C(Command::CENTER_VIEW, "Center View at Point"), Sep(),
            Sub("Standard Views", {
                A("ui.view-front", "Front"), A("ui.view-back", "Back"),
                A("ui.view-right", "Right"), A("ui.view-left", "Left"),
                A("ui.view-top", "Top"), A("ui.view-bottom", "Bottom"), Sep(),
                C(Command::NEAREST_ISO, "Nearest Isometric"),
                C(Command::NEAREST_ORTHO, "Nearest Orthographic"),
                C(Command::ONTO_WORKPLANE, "Align to Workplane") }),
            Sep(),
            C(Command::PERSPECTIVE_PROJ, "Perspective"),
            C(Command::EXPLODE_SKETCH, "Exploded View"),
            C(Command::DIM_SOLID_MODEL, "Darken Inactive Solids"),
            C(Command::SHOW_GRID, "Snap Grid"), Special(K::UNITS, "Units"), Sep(),
            A("ui.toggle-outliner", "Outliner", true),
            A("ui.toggle-properties", "Properties", true),
            A("ui.toggle-problems", "Problems", true),
            C(Command::SHOW_TOOLBAR, "Tool Shelf"),
            A("ui.toggle-menu-bar", "Menu Bar", true),
            A("ui.command-palette", "Command Palette..."), Sep(),
            A("ui.hide", "Hide Group"), A("ui.isolate", "Isolate Group"),
            A("ui.unhide-all", "Show All Groups"), Sep(),
            C(Command::VIEW_PROJECTION, "View Settings..."),
            A("ui.toggle-theme", "Light Theme", true),
            C(Command::FULL_SCREEN, "Full Screen"),
            C(Command::SHOW_TEXT_WND, "Classic Property Browser") } },
        { "New Group", {
            C(Command::GROUP_3D, "Sketch in 3D"),
            C(Command::GROUP_WRKPL, "Sketch in New Workplane"), Sep(),
            C(Command::GROUP_EXTRUDE, "Extrude"), C(Command::GROUP_REVOLVE, "Revolve"),
            C(Command::GROUP_LATHE, "Lathe"), C(Command::GROUP_HELIX, "Helix"), Sep(),
            C(Command::GROUP_TRANS, "Linear Pattern"),
            C(Command::GROUP_ROT, "Circular Pattern"), Sep(),
            C(Command::GROUP_LINK, "Link Part..."),
            Special(K::RECENT_LINKS, "Link Recent") } },
        { "Sketch", {
            C(Command::SEL_WORKPLANE, "In Workplane"), C(Command::FREE_IN_3D, "Anywhere in 3D"),
            Sep(),
            C(Command::DATUM_POINT, "Point"), C(Command::WORKPLANE, "Workplane"), Sep(),
            C(Command::LINE_SEGMENT, "Line"), C(Command::CONSTR_SEGMENT, "Construction Line"),
            C(Command::RECTANGLE, "Rectangle"), C(Command::CIRCLE, "Circle"),
            C(Command::ARC, "Arc"), C(Command::CUBIC, "Spline"), Sep(),
            C(Command::TTF_TEXT, "Text"), C(Command::IMAGE, "Image..."), Sep(),
            C(Command::CONSTRUCTION, "Toggle Construction"),
            C(Command::TANGENT_ARC, "Tangent Arc at Point"),
            C(Command::SPLIT_CURVES, "Split at Intersections") } },
        { "Constrain", {
            C(Command::DISTANCE_DIA, "Distance / Diameter", "ui.smart-dimension"),
            C(Command::REF_DISTANCE, "Reference Dimension"),
            C(Command::ANGLE, "Angle / Equal Angle"), C(Command::REF_ANGLE, "Reference Angle"),
            C(Command::OTHER_ANGLE, "Supplementary Angle"),
            C(Command::REFERENCE, "Toggle Reference"), Sep(),
            C(Command::HORIZONTAL, "Horizontal"), C(Command::VERTICAL, "Vertical"), Sep(),
            C(Command::ON_ENTITY, "On Point / Curve / Plane"),
            C(Command::EQUAL, "Equal Length / Radius"),
            C(Command::RATIO, "Length / Arc Ratio"),
            C(Command::DIFFERENCE, "Length / Arc Difference"),
            C(Command::AT_MIDPOINT, "Midpoint"), C(Command::SYMMETRIC, "Symmetric"),
            C(Command::PARALLEL, "Parallel / Tangent"),
            C(Command::PERPENDICULAR, "Perpendicular"),
            C(Command::ORIENTED_SAME, "Same Orientation"),
            C(Command::WHERE_DRAGGED, "Lock Point Where Dragged"), Sep(),
            C(Command::COMMENT, "Comment") } },
        { "Inspect", {
            C(Command::VOLUME, "Measure Volume"), C(Command::AREA, "Measure Area"),
            C(Command::PERIMETER, "Measure Perimeter"),
            C(Command::INTERFERENCE, "Interfering Parts"),
            C(Command::NAKED_EDGES, "Naked Edges"),
            C(Command::CENTER_OF_MASS, "Center of Mass"), Sep(),
            C(Command::SHOW_DOF, "Underconstrained Points"), Sep(),
            C(Command::TRACE_PT, "Trace Point"), C(Command::STOP_TRACING, "Stop Tracing..."),
            C(Command::STEP_DIM, "Step Dimension...") } },
        { "Help", {
            A("ui.show-shortcuts", "Keyboard Shortcuts"), Sep(),
            C(Command::WEBSITE, "Website / Manual"), C(Command::GITHUB, "GitHub Commit"),
            C(Command::ABOUT, "About") } },
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
                if(s.label && item->label != s.label) {
                    if(item->searchAlias.empty()) item->searchAlias = item->label;
                    item->label = s.label;
                }
                if(s.alias) item->keyAlias = s.alias;
                found->placed.insert(item.get());
                out->entries.push_back({ MenuSdl::Entry::Kind::ITEM, item, nullptr, item->label });
                break;
            }
            case K::ACTION: {
                auto item    = std::make_shared<MenuItemSdl>();
                item->label  = s.label;
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
                out->entries.push_back({ MenuSdl::Entry::Kind::ITEM, item, nullptr, s.label });
                break;
            }
            case K::SUB: {
                auto menu = std::make_shared<MenuSdl>();
                Build(s.children, menu.get(), found, checked);
                TrimSeparator(menu.get());
                if(!menu->entries.empty()) {
                    out->entries.push_back({ MenuSdl::Entry::Kind::SUBMENU, nullptr, menu, s.label });
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
                out->entries.push_back({ MenuSdl::Entry::Kind::SUBMENU, nullptr, menu, s.label });
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
        if(const char *l = FindLabel(top.entries, c)) return l;
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

void ApplyMenuLayout(MenuBarSdl &bar,
                     std::function<bool(const std::string &action)> actionChecked) {
    Found found;
    for(auto &m : bar.menus) Collect(*m.second, &found);
    std::vector<std::pair<std::string, std::shared_ptr<MenuSdl>>> menus;
    for(const Top &top : Layout()) {
        auto menu = std::make_shared<MenuSdl>();
        Build(top.entries, menu.get(), &found, actionChecked);
        TrimSeparator(menu.get());
        menus.push_back({ top.title, menu });
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
