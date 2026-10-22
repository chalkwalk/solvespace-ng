// Shell, draft and mass properties in the app (box.slvs: a 10 x 20 rectangle extruded 30, as
// test/group/features.h).
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static Group *Active() {
    return SK.GetGroup(SS.GW.activeGroup);
}

static hEntity Face(int remap, uint32_t request = 0) {
    hEntity in = request ? hRequest{ request }.entity(0) : Entity::NO_ENTITY;
    return Active()->Remap(in, remap);
}

TEST_CASE(ng_shells_with_the_top_face_open) {
    CHECK_LOAD("box.slvs");
    hEntity top = Face(Group::REMAP_TOP);
    Driver::Key("Shift+7");
    ClickAt(At({ 5, 10, 30 }));
    CHECK_TRUE(SS.GW.IsSelected(top));
    CHECK_TRUE(Driver::ClickMenu({ "New Group", "Shell" }));
    CHECK_TRUE(Active()->type == Group::Type::SHELL);
    CHECK_TRUE(Active()->faces.size() == 1 && Active()->faces[0] == top);
    CHECK_TRUE(EditField("feature-size", "1"));
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp));
    CHECK_TRUE(fabs(mp.volume - (6000 - 8 * 18 * 29)) < 0.01);
}

TEST_CASE(ng_drafts_a_side) {
    CHECK_LOAD("box.slvs");
    hEntity front = Face(Group::REMAP_LINE_TO_FACE, 5);
    Driver::Key("Shift+1"); // looking at the front, y = 0
    Driver::Key("Shift+7");
    ClickAt(At({ 5, 0, 15 }));
    CHECK_TRUE(SS.GW.IsSelected(front));
    CHECK_TRUE(Driver::ClickMenu({ "New Group", "Draft Faces" }));
    CHECK_TRUE(Active()->type == Group::Type::DRAFT);
    CHECK_TRUE(EditField("feature-size", "5"));
    CHECK_EQ_EPS(Active()->valA, 5);
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp));
    CHECK_TRUE(mp.volume < 6000 - 300);
}

TEST_CASE(ng_shows_mass_properties) {
    CHECK_LOAD("box.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "Inspect", "Mass Properties..." }));
    CHECK_TRUE(Driver::Dialogs().size() == 1);
    CHECK_TRUE(Driver::Dialogs()[0].message.find("Volume") != std::string::npos);
    CHECK_TRUE(Driver::Dialogs()[0].message.find("7.44 g") != std::string::npos);
}

// Sketching on a face: click the top, sketch in a new workplane on it, draw a rectangle and
// extrude it up.
TEST_CASE(ng_sketches_on_a_picked_face) {
    CHECK_LOAD("box.slvs");
    hEntity top = Face(Group::REMAP_TOP);
    Driver::Key("Shift+7");
    ClickAt(At({ 5, 10, 30 }));
    CHECK_TRUE(SS.GW.IsSelected(top));
    Driver::Key("Shift+W");
    CHECK_TRUE(Active()->type == Group::Type::DRAWING_WORKPLANE);
    CHECK_TRUE(Active()->subtype == Group::Subtype::WORKPLANE_BY_FACE);
    Driver::Key("R");
    ClickAt(At({ 2, 2, 30 }));
    ClickAt(At({ 6, 6, 30 }));
    Driver::Key("Escape");
    Driver::Key("E");
    CHECK_TRUE(Active()->type == Group::Type::EXTRUDE);
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp));
    CHECK_TRUE(mp.volume > 6000 + 1);
}
