#include "solvespace.h"
#include "ui/drawtool.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static keymap::Chord C(const char *s) { return *keymap::ParseChord(s); }

static void ClickAt(double u, double v) {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector  o  = SK.GetEntity(wp->point[0])->PointGetNum();
    Point2d s  = SS.GW.ProjectPoint(
        o.Plus(wp->Normal()->NormalU().ScaledBy(u)).Plus(wp->Normal()->NormalV().ScaledBy(v)));
    SS.GW.MouseLeftDown(s.x, s.y, false, false);
    SS.GW.MouseLeftUp(s.x, s.y, false, false);
}

TEST_CASE(typed_keys_draw_a_constrained_line) {
    CHECK_LOAD("line.slvs");
    SS.showToolbar = false;
    SS.GW.LookOntoActiveWorkplane(false);
    SS.GW.ActivateCommand(Command::LINE_SEGMENT);
    CHECK_FALSE(Draw().Active()); // before the first click there is nothing to type
    ClickAt(300, 300);
    CHECK_TRUE(Draw().Active());
    int before = SK.constraint.n;
    for(const char *k : {"2", "5", "Tab", "3", "0"}) CHECK_TRUE(Draw().HandleChord(C(k)));
    CHECK_TRUE(Draw().Description().find("25") != std::string::npos);
    CHECK_TRUE(Draw().HandleChord(C("Enter")));
    CHECK_TRUE(SK.constraint.n >= before + 2); // length + angle (+ reference line constraints)
    CHECK_TRUE(Draw().Active());               // the chain continues
    CHECK_TRUE(Draw().Description().find("25") == std::string::npos); // fields reset
}

TEST_CASE(esc_clears_fields_first_then_falls_through) {
    CHECK_LOAD("line.slvs");
    SS.showToolbar = false;
    SS.GW.LookOntoActiveWorkplane(false);
    SS.GW.ActivateCommand(Command::CIRCLE);
    ClickAt(300, 300);
    CHECK_TRUE(Draw().HandleChord(C("4")));
    CHECK_TRUE(Draw().HandleChord(C("Esc")));    // clears the field
    CHECK_FALSE(Draw().HandleChord(C("Esc")));   // now the keymap's Esc cancels the circle
    CHECK_FALSE(Draw().HandleChord(C("Enter"))); // nothing typed: not consumed
    CHECK_FALSE(Draw().HandleChord(C("L")));
}
