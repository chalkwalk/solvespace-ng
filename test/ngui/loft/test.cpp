// A loft made in the app (squares.slvs: a 10 square at z = 0 and a 4 square at z = 20, each
// in its own 3D sketch, centred on the Z axis; their front sides are requests 5 and 9).
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

TEST_CASE(ng_lofts_two_picked_profiles) {
    CHECK_LOAD("squares.slvs");
    Driver::Key("Shift+1");
    Driver::Key("Shift+7");
    Driver::Key("Home");
    ClickAt(At({ 0, -5, 0 }));
    ShiftClickAt(At({ 0, -2, 20 }));
    CHECK_TRUE(SS.GW.IsSelected(hRequest{ 5 }.entity(0)));
    CHECK_TRUE(SS.GW.IsSelected(hRequest{ 9 }.entity(0)));
    CHECK_TRUE(Driver::ClickMenu({ "New Group", "Loft" }));
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->type == Group::Type::LOFT && g->profiles.size() == 2);
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp));
    CHECK_TRUE(fabs(mp.volume - 1040) < 1e-6);
    Driver::Key("Ctrl+Z");
    CHECK_TRUE(SK.GetGroup(SS.GW.activeGroup)->type != Group::Type::LOFT);
}
