// Parts in the app (M4a): a second part sketched on the first part's top, both shown, faces
// of either picked, the part renamed in Properties (box.slvs: a 10 x 20 x 30 box).
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static Group *Active() {
    return SK.GetGroup(SS.GW.activeGroup);
}

TEST_CASE(ng_designs_a_second_part_on_the_first) {
    CHECK_LOAD("box.slvs");
    hGroup  box = SS.GW.activeGroup;
    hEntity top = Active()->Remap(Entity::NO_ENTITY, Group::REMAP_TOP);
    hEntity front = Active()->Remap(hRequest{ 5 }.entity(0), Group::REMAP_LINE_TO_FACE);
    Driver::Key("Shift+7");
    ClickAt(At({ 5, 10, 30 }));
    CHECK_TRUE(SS.GW.IsSelected(top));
    CHECK_TRUE(Driver::ClickMenu({ "New Group", "New Part" }));
    CHECK_TRUE(SK.part.n == 2);
    hPart lid = Active()->PartOf();
    CHECK_TRUE(lid != Part::FIRST);
    CHECK_TRUE(Active()->subtype == Group::Subtype::WORKPLANE_BY_FACE);
    Driver::Key("R");
    ClickAt(At({ 2, 2, 30 }));
    ClickAt(At({ 8, 18, 30 }));
    Driver::Key("Escape");
    Driver::Key("E");
    CHECK_TRUE(Active()->type == Group::Type::EXTRUDE && Active()->PartOf() == lid);
    // The first part is still shown, and its faces can be picked from the second.
    CHECK_TRUE(SS.GW.OtherPartSolids().size() == 1);
    CHECK_TRUE(SS.GW.OtherPartSolids()[0]->h == box);
    Driver::Key("Shift+1");
    Driver::Key("Shift+7");
    Driver::Key("Home");
    ClickAt(At({ 5, 0, 10 }));
    CHECK_TRUE(SS.GW.IsSelected(front));
    // Properties names the part.
    Driver::Key("Escape");
    CHECK_TRUE(EditField("part-name", "lid"));
    CHECK_EQ_STR(SS.PartName(lid), "lid");
}
