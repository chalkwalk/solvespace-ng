// Screenshots of every menu, panel and overlay for the owner's review (blindweb rounds; see
// tools/review/). Runs only when NG_CAPTURE_DIR is set, so the normal suite skips it:
//   NG_CAPTURE_DIR=/tmp/shots [NG_CAPTURE_THEME=light] ./bin/solvespace-ng-uitest ngui/capture/
// Each case saves <name>-<theme>.png and appends "<name> <theme> x0 y0 x1 y1" (the part to
// crop, window coordinates) to crops.txt.
#include "../uitest.h"
#include "ui/piemenu.h"
#include "ui/theme.h"
#include "ui/viewcube.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static const char *CaptureDir() {
    return getenv("NG_CAPTURE_DIR");
}

static std::string Theme() {
    const char *t = getenv("NG_CAPTURE_THEME");
    return t ? t : "dark";
}

// Set up a case: nothing to do (false) unless capturing; a roomy window, the theme.
static bool Begin() {
    if(!CaptureDir()) return false;
    if(Theme() == "light") Ui::ToggleTheme();
    Driver::Move(700, 700);
    return true;
}

struct Box {
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    void Add(double ax0, double ay0, double ax1, double ay1) {
        x0 = std::min(x0, ax0); y0 = std::min(y0, ay0);
        x1 = std::max(x1, ax1); y1 = std::max(y1, ay1);
    }
    bool Empty() const { return x1 < x0; }
};

// Everything drawn in windows whose name contains `part` ("###Menu_", "###panel-View").
static Box WindowBox(const std::string &part) {
    Box b;
    for(const Driver::Item &i : Driver::Items()) {
        if(i.window.find(part) != std::string::npos) b.Add(i.x0, i.y0, i.x1, i.y1);
    }
    return b;
}

static void Shot(const std::string &name, Box b, double pad = 12) {
    std::string dir  = CaptureDir();
    std::string file = name + "-" + Theme() + ".png";
    Driver::Screenshot(Platform::Path::From(dir).Join(file));
    if(b.Empty()) b.Add(0, 0, Driver::WindowSize().x, Driver::WindowSize().y);
    FILE *f = fopen((dir + "/crops.txt").c_str(), "a");
    if(f) {
        fprintf(f, "%s %s %.0f %.0f %.0f %.0f\n", name.c_str(), Theme().c_str(),
                std::max(0.0, b.x0 - pad), std::max(0.0, b.y0 - pad), b.x1 + pad, b.y1 + pad);
        fclose(f);
    }
}

static void Menu(const char *title, const char *name) {
    Driver::ClickItem(title);
    Box b = WindowBox("###Menu_");
    std::optional<Driver::Item> t = Driver::FindItem(title);
    if(t) b.Add(t->x0, t->y0, t->x1, t->y1);
    Shot(name, b);
    Driver::Key("Escape");
}

static Box Panel(const char *title) {
    return WindowBox(std::string(title) + "###panel-");
}

TEST_CASE(menus) {
    if(!Begin()) return;
    CHECK_LOAD("line.slvs");
    const char *menus[][2] = { { "File", "menu-1-file" },       { "Edit", "menu-2-edit" },
                               { "View", "menu-3-view" },       { "New Group", "menu-4-new-group" },
                               { "Sketch", "menu-5-sketch" },   { "Constrain", "menu-6-constrain" },
                               { "Inspect", "menu-7-analyze" }, { "Help", "menu-8-help" } };
    for(auto &m : menus) Menu(m[0], m[1]);
}

TEST_CASE(main_window) {
    if(!Begin()) return;
    CHECK_LOAD("line.slvs");
    Driver::ClickMenu({ "View", "Tool Shelf" });
    Driver::Move(700, 700);
    Shot("window-1-overview", Box{});
}

TEST_CASE(outliner_and_properties) {
    if(!Begin()) return;
    CHECK_LOAD("line.slvs");
    Driver::ClickMenu({ "New Group", "Sketch in 3D" });
    Shot("panel-1-outliner", Panel("Outliner"));
    Shot("panel-2-properties-group", Panel("Properties"));
    hRequest line = DrawLine(Point2d::From(600, 600), Point2d::From(760, 560));
    ClickAt(AtMiddle(line));
    Shot("panel-3-properties-line", Panel("Properties"));
    Driver::Key("D");
    Driver::Key("Return");
    ClickAt(AtLabel(*LastConstraint(Constraint::Type::PT_PT_DISTANCE)));
    Shot("panel-4-properties-dimension", Panel("Properties"));
}

TEST_CASE(problems) {
    if(!Begin()) return;
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(600, 600), Point2d::From(760, 560));
    ClickAt(AtMiddle(line));
    Driver::Key("D");
    Driver::Key("Return");
    for(const char *c : { "Horizontal", "Vertical" }) {
        Point2d m = AtMiddle(line);
        Driver::Move(m.x, m.y);
        Driver::Key("F");
        PickPie(c);
    }
    Shot("panel-5-problems", Panel("Problems"));
}

TEST_CASE(preferences) {
    if(!Begin()) return;
    CHECK_LOAD("line.slvs");
    Driver::Key("Ctrl+,");
    Shot("panel-6-preferences", Panel("Preferences"));
    for(const char *section : { "Export" }) {
        if(RevealIn("Preferences", section)) Driver::ClickItem(section);
    }
    RevealIn("Preferences", "Export");
    Shot("panel-7-preferences-more", Panel("Preferences"));
}

TEST_CASE(styles) {
    if(!Begin()) return;
    CHECK_LOAD("line.slvs");
    Driver::ClickMenu({ "Edit", "Line Styles..." });
    Shot("panel-8-styles", Panel("Styles"));
}

TEST_CASE(view_panel) {
    if(!Begin()) return;
    CHECK_LOAD("line.slvs");
    Driver::ClickMenu({ "View", "View Settings..." });
    Shot("panel-9-view", Panel("View"));
}

TEST_CASE(tool_panels) {
    if(!Begin()) return;
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(600, 600), Point2d::From(760, 600));
    ClickAt(AtMiddle(line));
    Driver::Key("Ctrl+C");
    Driver::ClickMenu({ "Edit", "Paste Transformed..." });
    Shot("panel-10-paste-transformed", Panel("Paste Transformed"));
    ClosePanel("Paste Transformed");
    ClickAt(AtMiddle(line));
    Driver::Key("D");
    Driver::Key("Return");
    ClickAt(AtLabel(*LastConstraint(Constraint::Type::PT_PT_DISTANCE)));
    Driver::ClickMenu({ "Inspect", "Step Dimension..." });
    Shot("panel-11-step-dimension", Panel("Step Dimension"));
    ClosePanel("Step Dimension");
    Driver::Key("Escape");
    Driver::ClickMenu({ "Sketch", "Tangent Arc at Point" });
    Shot("panel-12-tangent-arc", Panel("Tangent Arc"));
}

TEST_CASE(overlays) {
    if(!Begin()) return;
    CHECK_LOAD("line.slvs");
    Driver::ClickMenu({ "View", "Tool Shelf" });
    Shot("overlay-1-tool-shelf", Panel("Tools"));
    Driver::ClickMenu({ "View", "Tool Shelf" });
    Driver::Move(700, 600);
    Driver::Key("Space");
    Driver::Type("con");
    Shot("overlay-2-palette", WindowBox("##palette"));
    Driver::Key("Escape");
    for(auto [key, name] : { std::pair{ "Shift+A", "overlay-3-pie-add" },
                             std::pair{ "`", "overlay-5-pie-view" } }) {
        Driver::Move(700, 600);
        Driver::Key(key);
        Box b;
        b.Add(Ui::Pie().CenterX() - 230, Ui::Pie().CenterY() - 170, Ui::Pie().CenterX() + 230,
              Ui::Pie().CenterY() + 170);
        Shot(name, b, 0);
        Driver::Key("Escape");
    }
    hRequest a = DrawLine(Point2d::From(560, 640), Point2d::From(760, 640));
    hRequest b = DrawLine(Point2d::From(560, 720), Point2d::From(760, 700));
    ClickAt(AtMiddle(a));
    ShiftClickAt(AtMiddle(b));
    Driver::Move(700, 600);
    Driver::Key("F");
    Box pb;
    pb.Add(Ui::Pie().CenterX() - 230, Ui::Pie().CenterY() - 170, Ui::Pie().CenterX() + 230,
           Ui::Pie().CenterY() + 170);
    Shot("overlay-4-pie-constrain", pb, 0);
    Driver::Key("Escape");
    Box s = WindowBox("##statusbar");
    Shot("overlay-6-status-bar", s, 0);
    Point2d c = Ui::ViewCubeCenter();
    Box vc;
    vc.Add(c.x - 110, c.y - 110, c.x + 110, c.y + 110);
    Shot("overlay-7-view-cube", vc, 0);
}

TEST_CASE(context_menu) {
    if(!Begin()) return;
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(600, 600), Point2d::From(760, 600));
    Point2d m = AtMiddle(line);
    Driver::DuringModal([] {
        Shot("overlay-8-context-menu", WindowBox("##Popup"));
        Driver::Key("Escape");
    });
    Driver::Click(m.x, m.y, Driver::Button::RIGHT);
}


TEST_CASE(glyph_states) {
    // Constraint glyphs by solve state: under-constrained, fully constrained, conflicting.
    if(!Begin()) return;
    CHECK_LOAD("line.slvs");
    ClosePanel("Outliner");
    ClosePanel("Properties");
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT); // the fixture's only line
    Constraint::Constrain(Constraint::Type::HORIZONTAL, Entity::NO_ENTITY, Entity::NO_ENTITY,
                          line.entity(0));
    Constraint::ConstrainCoincident(line.entity(1), Request::HREQUEST_REFERENCE_XY.entity(1));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Driver::Settle();
    Box b;
    Point2d a = At(SK.GetEntity(line.entity(1))->PointGetNum());
    b.Add(a.x - 120, a.y - 140, a.x + 320, a.y + 90);
    Shot("glyphs-1-under", b, 0);
    Constraint c = {};
    c.type      = Constraint::Type::PT_PT_DISTANCE;
    c.group     = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    c.ptA       = line.entity(1);
    c.ptB       = line.entity(2);
    c.valA      = 40;
    c.disp.offset = Vector::From(0, 8, 0);
    Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Driver::Settle();
    Shot("glyphs-2-full", b, 0);
    Constraint::Constrain(Constraint::Type::VERTICAL, Entity::NO_ENTITY, Entity::NO_ENTITY,
                          line.entity(0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Driver::Settle();
    Shot("glyphs-3-conflicting", b, 0);
}

TEST_CASE(ghost_solid) {
    // A solid drawn while its new shape is made on the worker (regen.cpp), beside the normal one.
    if(!Begin()) return;
    CHECK_LOAD("extrudes.slvs");
    ClosePanel("Outliner");
    ClosePanel("Properties");
    Group *g = SK.GetGroup(SK.groupOrder[SK.groupOrder.n - 1]);
    SS.GW.activeGroup = g->h;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Driver::Key("Shift+7");
    Driver::Key("Home");
    Shot("ghost-1-solid", Box{});
    g->regenPending = true;
    SS.GW.Invalidate(/*clearPersistent=*/true);
    Driver::Settle();
    Shot("ghost-2-pending", Box{});
    g->regenPending = false;
}
