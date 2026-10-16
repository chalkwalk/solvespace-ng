#include "solvespace.h"
#include "ui/grabtool.h"
#include "ui/keyrouter.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static keymap::Chord C(const char *s) { return *keymap::ParseChord(s); }

static hRequest FreeLine() {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector  o  = SK.GetEntity(wp->point[0])->PointGetNum();
    Vector  u = wp->Normal()->NormalU(), v = wp->Normal()->NormalV();
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, false);
    SK.GetEntity(hr.entity(1))->PointForceTo(o.Plus(u.ScaledBy(200)).Plus(v.ScaledBy(200)));
    SK.GetEntity(hr.entity(2))->PointForceTo(o.Plus(u.ScaledBy(260)).Plus(v.ScaledBy(230)));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return hr;
}

TEST_CASE(grab_tool_axis_and_typed_distance) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    Vector a0 = SK.GetEntity(hr.entity(1))->PointGetNum();
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(Grab().Start());
    CHECK_TRUE(Grab().HandleChord(C("Y")));
    for(const char *k : {"1", "5"}) CHECK_TRUE(Grab().HandleChord(C(k)));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector ay = SS.GW.GrabAxisVector(1);
    CHECK_TRUE(SK.GetEntity(hr.entity(1))->PointGetNum().Minus(a0.Plus(ay.ScaledBy(15)))
                   .Magnitude() < 1e-6);
    CHECK_TRUE(Grab().Description().find("15") != std::string::npos);
    CHECK_TRUE(Grab().HandleChord(C("Enter")));
    CHECK_FALSE(Grab().Active());
}

TEST_CASE(grab_x_locks_axis_and_never_deletes) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    int requests = SK.request.n;
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("delete: X\n", &err));
    r.modal = [](const keymap::Chord &c) { return Grab().Active() && Grab().HandleChord(c); };
    CHECK_TRUE(Grab().Start());
    CHECK_TRUE(r.Route(C("X"), false) == RouteResult::MODAL);
    CHECK_TRUE(SS.GW.grab.axis == 0);
    CHECK_TRUE(SK.request.n == requests);
    CHECK_TRUE(Grab().HandleChord(C("Esc")));
    CHECK_FALSE(Grab().Active());
    CHECK_TRUE(r.Route(C("X"), false) == RouteResult::COMMAND); // after the grab, X deletes again
}
