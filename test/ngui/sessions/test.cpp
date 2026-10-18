// Whole working sessions: many features one after another on the same model, the way a person
// uses the app, so a change that only breaks a feature after another one has run shows up.
// Each session ends by checking that undo goes all the way back, redo all the way forward, and
// that saving and opening the file gives the same model.
#include "../uitest.h"
#include "ui/grabtool.h"
#include "ui/ngui.h"
#include "ui/problemsmodel.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

// What the model is, independent of handles' order on screen: groups, requests by type, and
// constraints with their values.
static std::string Fingerprint() {
    std::vector<std::string> parts;
    for(hGroup hg : SK.groupOrder) {
        Group *g = SK.GetGroup(hg);
        parts.push_back("g" + std::to_string((int)g->type) + ":" + g->name);
    }
    std::map<int, int> requests;
    for(const Request &r : SK.request) requests[(int)r.type]++;
    for(auto &[type, n] : requests) {
        parts.push_back("r" + std::to_string(type) + "x" + std::to_string(n));
    }
    std::vector<std::string> constraints;
    for(const Constraint &c : SK.constraint) {
        char buf[64];
        snprintf(buf, sizeof(buf), "c%d=%.3f%s", (int)c.type, c.valA, c.reference ? "r" : "");
        constraints.push_back(buf);
    }
    std::sort(constraints.begin(), constraints.end());
    parts.insert(parts.end(), constraints.begin(), constraints.end());
    std::string s;
    for(const std::string &p : parts) s += p + " ";
    return s;
}

static Group *Named(const char *name) {
    for(Group &g : SK.group) {
        if(g.name == name) return &g;
    }
    return nullptr;
}

// Ctrl+Z until the model is `target` again; the number of presses, or -1 if it never is.
static int UndoUntil(const std::string &target, int limit = 40) {
    for(int i = 0; i < limit; i++) {
        if(Fingerprint() == target) return i;
        Driver::Key("Ctrl+Z");
    }
    return Fingerprint() == target ? limit : -1;
}

static bool SaveAsThenReopen(const char *name) {
    Platform::Path out = Driver::ConfigDir().Join(name);
    Driver::QueueFile(out);
    Driver::Key("Ctrl+Shift+S");
    if(SS.unsaved) return false;
    Driver::Key("Ctrl+N");
    if(SK.request.n > 3) return false;
    Driver::QueueFile(out);
    Driver::Key("Ctrl+O");
    return true;
}

// Ends every session: the whole history undoes and redoes, and the file round-trips. Empty if
// all of that holds, else what went wrong.
static std::string UndoRedoAndRoundTrip(const std::string &start, const char *file) {
    std::string end = Fingerprint();
    if(end == start) return "the session changed nothing";
    int steps = UndoUntil(start);
    if(steps <= 0) return "undo never got back to the start: " + Fingerprint();
    for(int i = 0; i < steps; i++) Driver::Key("Ctrl+Shift+Z");
    if(Fingerprint() != end) return "redo did not get back to the end: " + Fingerprint();
    if(!SaveAsThenReopen(file)) return "save as / new / open failed";
    if(Fingerprint() != end) return "the reopened file differs: " + Fingerprint();
    return "";
}

TEST_CASE(a_plate_with_a_hole_drawn_named_and_extruded) {
    CHECK_LOAD("line.slvs");
    std::string start = Fingerprint();
    int requests = SK.request.n;

    // A 40 x 30 rectangle and a 8 mm hole, sized by typing.
    Driver::Key("B");
    ClickAt(Point2d::From(420, 600));
    Driver::Move(560, 500);
    Driver::Type("40");
    Driver::Key("Tab");
    Driver::Type("30");
    Driver::Key("Return");
    Driver::Key("C");
    ClickAt(Point2d::From(470, 560));
    Driver::Move(490, 560);
    Driver::Type("8");
    Driver::Key("Return");
    CHECK_TRUE(SK.GetConstraint(*LastConstraint(Constraint::Type::DIAMETER))->valA == 8.0);

    // A two-segment chain ended with the right button, then one segment taken back.
    Driver::Key("L");
    ClickAt(Point2d::From(780, 620));
    ClickAt(Point2d::From(860, 620));
    ClickAt(Point2d::From(860, 700));
    Driver::Key("Backspace");
    ClickAt(Point2d::From(900, 680));
    Driver::Click(880, 740, Driver::Button::RIGHT);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    // rectangle 4 + circle 1 + chain 2
    CHECK_TRUE(SK.request.n == requests + 7);

    // Name the sketch from the Outliner: every letter of "plate" is also a tool key.
    Point2d row = Driver::FindItem("sketch-in-plane")->Center();
    Driver::Click(row.x, row.y, Driver::Button::RIGHT);
    CHECK_TRUE(Driver::ClickItem("Rename"));
    Driver::Type("plate");
    Driver::Key("Return");
    CHECK_TRUE(Named("plate") != nullptr);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(SK.request.n == requests + 7);

    // Extrude, then make the solid see-through from Properties with an expression.
    Driver::Move(600, 400);
    Driver::Key("E");
    CHECK_TRUE(SK.GetGroup(SS.GW.activeGroup)->type == Group::Type::EXTRUDE);
    CHECK_TRUE(EditField("opacity", "1/2"));
    CHECK_TRUE(fabs(SK.GetGroup(SS.GW.activeGroup)->color.alphaF() - 0.5) < 0.01); // a byte

    // Switching theme in the middle of work changes nothing in the model.
    std::string before = Fingerprint();
    Driver::Key("Space");
    Driver::Type("toggle theme");
    Driver::Key("Return");
    CHECK_TRUE(SS.backgroundColor.red > 200);
    CHECK_EQ_STR(Fingerprint(), before);

    CHECK_EQ_STR(UndoRedoAndRoundTrip(start, "plate.slvs"), "");
}

TEST_CASE(constraining_into_a_problem_and_out_again) {
    CHECK_LOAD("line.slvs");
    std::string start = Fingerprint();

    hRequest line = DrawLine(Point2d::From(560, 520), Point2d::From(700, 470));
    ClickAt(AtMiddle(line));
    Driver::Key("D");
    Driver::Key("Return");
    CHECK_TRUE(LastConstraint(Constraint::Type::PT_PT_DISTANCE).has_value());

    // Horizontal from the pie, then Vertical too: the solve fails and Problems opens itself.
    for(const char *c : { "Horizontal", "Vertical" }) {
        Point2d m = AtMiddle(line);
        Driver::Move(m.x, m.y);
        Driver::Key("F");
        CHECK_TRUE(PickPie(c));
    }
    CHECK_FALSE(SS.ActiveGroupsOkay());
    CHECK_TRUE(Driver::FindItem("Problems").has_value());
    CHECK_TRUE(Driver::ClickItem("select all"));
    CHECK_TRUE(SS.GW.selection.n > 0);
    // Delete the culprits that Problems selected: solvable again.
    Driver::Move(900, 700);
    Driver::Key("Delete");
    CHECK_TRUE(SS.ActiveGroupsOkay());

    // Grab the line 10 mm along X, from the keyboard.
    ClickAt(AtMiddle(line));
    Vector a0 = SK.GetEntity(line.entity(1))->PointGetNum();
    Driver::Key("G");
    Point2d m = AtMiddle(line);
    Driver::Move(m.x + 40, m.y);
    Driver::Key("X");
    Driver::Type("10");
    Driver::Key("Return");
    CHECK_FALSE(Ui::Grab().Active());
    CHECK_EQ_EPS(SK.GetEntity(line.entity(1))->PointGetNum().Minus(a0).Magnitude(), 10.0);

    // Views from the numpad leave the model alone; Tab looks onto the sketch and back.
    std::string before = Fingerprint();
    Driver::Key("Keypad 1");
    Driver::Key("Keypad 7");
    Driver::Key("Tab");
    Driver::Key("Tab");
    CHECK_EQ_STR(Fingerprint(), before);

    CHECK_EQ_STR(UndoRedoAndRoundTrip(start, "problem.slvs"), "");
}

TEST_CASE(switching_keymaps_in_the_middle_of_a_session) {
    CHECK_LOAD("line.slvs");
    std::string start = Fingerprint();
    int requests = SK.request.n;

    // Classic from Preferences: S draws a line, as in stock SolveSpace.
    Driver::Key("Ctrl+,");
    CHECK_TRUE(RevealIn("Preferences", "SolveSpace NG"));
    CHECK_TRUE(Driver::ClickItem("SolveSpace NG"));
    CHECK_TRUE(RevealIn("Preferences", "classic keys"));
    CHECK_TRUE(Driver::ClickItem("classic keys"));
    CHECK_TRUE(ClosePanel("Preferences"));
    Driver::Move(600, 650);
    Driver::Key("S");
    ClickAt(Point2d::From(450, 600));
    ClickAt(Point2d::From(550, 600));
    Driver::Key("Escape");
    CHECK_TRUE(SK.request.n == requests + 1);

    // And back to Modern: Classic has no key for Preferences, so through the Edit menu.
    CHECK_TRUE(Driver::ClickMenu({ "Edit", "Preferences..." }));
    CHECK_TRUE(RevealIn("Preferences", "modern keys"));
    CHECK_TRUE(Driver::ClickItem("modern keys"));
    CHECK_TRUE(ClosePanel("Preferences"));
    Driver::Move(600, 650);
    Driver::Key("L");
    ClickAt(Point2d::From(450, 680));
    Driver::Move(520, 650);
    Driver::Type("25");
    Driver::Key("Return");
    Driver::Key("Escape");
    CHECK_TRUE(SK.request.n == requests + 2);
    CHECK_EQ_EPS(SK.GetConstraint(*LastConstraint(Constraint::Type::PT_PT_DISTANCE))->valA, 25.0);

    CHECK_EQ_STR(UndoRedoAndRoundTrip(start, "keymaps.slvs"), "");
}
