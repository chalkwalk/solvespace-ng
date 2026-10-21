// The tool shelf: NG's replacement for the old left toolbar, hidden until asked for.
#include "../uitest.h"
#include "platform/sdl/icontextures.h"
#include "ui/shelf.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static bool ShelfShown() {
    return Driver::FindItem("Tools").has_value();
}

static int ShelfButtons() {
    int n = 0;
    for(const Driver::Item &i : Driver::Items()) n += i.id.rfind("shelf-", 0) == 0;
    return n;
}

static bool ShowShelf() {
    // Every button is there as soon as the app is idle (an auto-sized window takes two frames).
    return Driver::ClickMenu({ "View", "Tool Shelf" }) && ShelfShown() && ShelfButtons() == 41;
}

TEST_CASE(shelf_starts_hidden_and_the_old_toolbar_is_gone) {
    CHECK_LOAD("line.slvs");
    CHECK_FALSE(ShelfShown());
    CHECK_FALSE(SS.showToolbar);
    Driver::Click(20, 60); // where the old toolbar's line button was
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(view_menu_shows_and_hides_the_shelf) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(ShowShelf());
    CHECK_TRUE(SS.showToolbar);
    CHECK_TRUE(ClosePanel("Tools"));
    CHECK_FALSE(SS.showToolbar);
    CHECK_TRUE(ShowShelf());
    CHECK_TRUE(Driver::ClickMenu({ "View", "Tool Shelf" }));
    CHECK_FALSE(ShelfShown());
}

TEST_CASE(a_shelf_button_starts_its_tool) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(ShowShelf());
    CHECK_TRUE(Driver::ClickItem("Line"));
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
    CHECK_TRUE(Ui::ShelfTextFallbacks() == 0); // every button had its icon
}

TEST_CASE(each_shelf_section_starts_its_own_row) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(ShowShelf());
    auto item = [](const char *label) { return *Driver::FindItem(label); };
    // A column two (or, in a short window, three) buttons wide; a section starts at the left
    // of a new row.
    CHECK_TRUE(item("Line").y0 == item("Rectangle").y0);
    CHECK_TRUE(item("Rectangle").x0 > item("Line").x0);
    CHECK_TRUE(item("Distance / Diameter").y0 > item("Split at Intersections").y0);
    CHECK_TRUE(item("Distance / Diameter").x0 == item("Line").x0);
    CHECK_TRUE(item("Extrude").y0 > item("Toggle Reference").y0);
}

TEST_CASE(the_shelf_is_a_column_on_the_left) {
    // (L1, owner review 4: layout c) Tools on the left, as Blender, SketchUp and Plasticity.
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(ShowShelf());
    std::optional<Driver::Item> tools = Driver::FindItem("Tools");
    CHECK_TRUE(tools.has_value());
    if(!tools) return;
    CHECK_TRUE(tools->x0 < 20);
    CHECK_TRUE(tools->y1 - tools->y0 > 2 * (tools->x1 - tools->x0));
    for(const Driver::Item &i : Driver::Items()) {
        if(i.id.rfind("shelf-", 0) == 0) CHECK_TRUE(i.fullyVisible);
    }
}

TEST_CASE(the_shelf_fits_the_smallest_window) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(ShowShelf());
    Driver::Resize(640, 480); // the app's minimum size stops it at 720 x 636
    CHECK_TRUE(ShelfButtons() == 41);
    for(const Driver::Item &i : Driver::Items()) {
        if(i.id.rfind("shelf-", 0) == 0) CHECK_TRUE(i.fullyVisible && i.y1 < 636 - 30);
    }
}

TEST_CASE(outliner_and_properties_stack_on_the_right) {
    CHECK_LOAD("line.slvs");
    Point2d ws = Driver::WindowSize();
    std::optional<Driver::Item> o = Driver::FindItem("Outliner"), p = Driver::FindItem("Properties");
    CHECK_TRUE(o && p);
    if(!o || !p) return;
    CHECK_TRUE(o->x0 > ws.x / 2 && o->x1 <= ws.x);
    CHECK_TRUE(p->x0 == o->x0 && p->x1 == o->x1);
    CHECK_TRUE(p->y0 > o->y1);
}

TEST_CASE(a_constraint_button_with_nothing_selected_explains) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(ShowShelf());
    CHECK_TRUE(Driver::ClickItem("Parallel / Tangent"));
    CHECK_TRUE(Driver::Dialogs().size() == 1 && Driver::Dialogs()[0].kind == "message");
}

TEST_CASE(shelf_stays_whole_when_the_window_shrinks) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(ShowShelf());
    Driver::Resize(640, 480); // the app's minimum size stops it a little larger
    Point2d ws = Driver::WindowSize();
    // It moves back on screen: every button can still be clicked.
    CHECK_TRUE(ShelfButtons() == 41);
    for(const Driver::Item &i : Driver::Items()) {
        if(i.id.rfind("shelf-", 0) == 0) CHECK_TRUE(i.fullyVisible && i.x1 <= ws.x);
    }
}

TEST_CASE(panels_come_back_on_screen_when_the_window_shrinks) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::FindItem("Properties").has_value());
    Driver::Resize(640, 480);
    Point2d ws = Driver::WindowSize();
    std::optional<Driver::Item> p = Driver::FindItem("Properties");
    CHECK_TRUE(p && p->x1 <= ws.x && p->y1 <= ws.y);
}

TEST_CASE(a_broken_icon_gives_no_texture) {
    // The shelf then draws the button as text. (A missing icon file can't reach users: the
    // ng/shelf model test loads every one.)
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Platform::IconTextureFromSvg("broken", "not an svg", 24) == 0);
    CHECK_TRUE(Platform::IconTexture("circle", 24) != 0);
}

TEST_CASE(icons_follow_the_light_theme) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(ShowShelf());
    Driver::Key("Space");
    Driver::Type("toggle theme");
    Driver::Key("Return");
    std::optional<Driver::Item> b = Driver::FindItem("Circle");
    CHECK_TRUE(b.has_value());
    Platform::Path shot = Driver::ConfigDir().Join("shelf.png");
    CHECK_TRUE(Driver::Screenshot(shot));
    std::shared_ptr<Pixmap> px = Pixmap::ReadPng(shot);
    CHECK_TRUE(px != nullptr);
    // Part of the circle icon is dark: tinted with the light theme's text colour.
    int dark = 0;
    for(int y = (int)b->y0; px && b && y < (int)b->y1; y++) {
        for(int x = (int)b->x0; x < (int)b->x1; x++) dark += px->GetPixel(x, y).red < 90;
    }
    CHECK_TRUE(dark > 5);
}
