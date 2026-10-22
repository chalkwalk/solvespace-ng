// Motion joints in the app (M5a): a lid hinged on a box, made from the menu, shown in the
// Outliner and Properties, turned by a typed angle, the scrub slider and G, and swept for
// clearance (box.slvs: a 10 x 20 x 30 box; request 5 its sketch's line along +X).
#include "../uitest.h"
#include "ui/properties.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

namespace {

hRequest AddLine(Vector a, Vector b) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

// The Outliner scrolled to its end, where the joints are listed.
void OutlinerToEnd() {
    std::optional<Driver::Item> panel = Driver::FindItem("Outliner");
    if(!panel) return;
    for(int i = 0; i < 20; i++) {
        Driver::Wheel((panel->x0 + panel->x1) / 2, (panel->y0 + panel->y1) / 2, -1);
    }
}

void Select(std::vector<hEntity> entities) {
    SS.GW.ClearSelection();
    for(hEntity he : entities) {
        GraphicsWindow::Selection sel = {};
        sel.entity                    = he;
        SS.GW.selection.Add(&sel);
    }
}

// A 10 x 6 x 2 lid on the box's top, along its front edge; its solid group.
hGroup Lid(hRequest *hinge) {
    Group *box = SK.GetGroup(SS.GW.activeGroup);
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    Select({ box->Remap(Entity::NO_ENTITY, Group::REMAP_TOP) });
    SS.NewPart();
    Vector   p[4] = { { 0, 0, 30 }, { 10, 0, 30 }, { 10, 6, 30 }, { 0, 6, 30 } };
    hRequest side[4];
    for(int i = 0; i < 4; i++) side[i] = AddLine(p[i], p[(i + 1) % 4]);
    for(int i = 0; i < 4; i++) {
        Constraint::ConstrainCoincident(side[i].entity(2), side[(i + 1) % 4].entity(1));
    }
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group::MenuGroup(Command::GROUP_EXTRUDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *e = SK.GetGroup(SS.GW.activeGroup);
    SK.GetParam(e->h.param(0))->val = 0;
    SK.GetParam(e->h.param(1))->val = 0;
    SK.GetParam(e->h.param(2))->val = 1; // 2 thick
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    *hinge = side[0];
    return SS.GW.activeGroup;
}

}

TEST_CASE(ng_hinges_a_lid_and_turns_it) {
    CHECK_LOAD("box.slvs");
    hGroup   box = SS.GW.activeGroup;
    hRequest hinge;
    hGroup   lid = Lid(&hinge);
    Select({ SK.GetGroup(box)->Remap(hRequest{ 5 }.entity(0), Group::REMAP_TOP), hinge.entity(0) });
    CHECK_TRUE(Driver::ClickMenu({ "New Group", "Joints", "Revolute Joint" }));
    CHECK_TRUE(SK.joint.n == 1);
    hJoint hj = SK.joint.begin()->h;
    // The Outliner lists it; a click shows it in Properties, where an angle can be typed.
    OutlinerToEnd();
    CHECK_TRUE(Driver::ClickItem("joint:" + std::to_string(hj.v)));
    CHECK_TRUE(EditField("joint-value", "45"));
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 45);
    // The scrub slider turns it; one drag is one undo step.
    std::optional<Driver::Item> scrub = Driver::FindItem("joint-scrub");
    CHECK_TRUE(scrub.has_value());
    double y = (scrub->y0 + scrub->y1) / 2;
    Driver::Drag(scrub->x0 + (scrub->x1 - scrub->x0) * 0.5, y,
                 scrub->x0 + (scrub->x1 - scrub->x0) * 0.75, y);
    double scrubbed = SK.GetJoint(hj)->value[0];
    CHECK_TRUE(scrubbed > 45);
    SS.UndoUndo();
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 45);
    // G on the lid turns it: looking along the hinge, the mouse goes round it.
    Driver::Key("Escape");
    SS.GW.projRight = Vector::From(0, 1, 0);
    SS.GW.projUp    = Vector::From(0, 0, 1);
    SS.GW.Invalidate();
    hPart   lidPart = SK.GetGroup(lid)->PartOf();
    Point2d onLid   = At(SS.PartPose(lidPart).Apply({ 5, 3, 31 }));
    Driver::Move(onLid.x, onLid.y);
    CHECK_TRUE(SS.GW.hover.entity.v != 0 &&
               SK.GetGroup(SK.GetEntity(SS.GW.hover.entity)->group)->h == lid);
    Driver::Key("G");
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::GRABBING && SS.GW.grab.joint == hj);
    Driver::Move(At({ 5, 0, 34 }).x, At({ 5, 0, 34 }).y);
    double turned = SK.GetJoint(hj)->value[0];
    CHECK_TRUE(turned > 60);
    Driver::Key("Return");
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    // The clearance sweep reports in Properties: the lid sits on the box, so they touch.
    OutlinerToEnd();
    CHECK_TRUE(Driver::ClickItem("joint:" + std::to_string(hj.v)));
    CHECK_TRUE(RevealIn("Properties", "joint-sweep"));
    CHECK_TRUE(Driver::ClickItem("joint-sweep"));
    CHECK_TRUE(Ui::JointSweepText(hj).find("touch or overlap") != std::string::npos);
    // Back to the design pose.
    CHECK_TRUE(Driver::ClickMenu({ "Inspect", "Design Pose" }));
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 0);
}

// A joint of several motions (M5b): Properties has a row for each, and G with Z picks the turn.
TEST_CASE(ng_moves_a_planar_joint_by_each_motion) {
    CHECK_LOAD("box.slvs");
    hGroup   box = SS.GW.activeGroup;
    hRequest hinge;
    hGroup   lid = Lid(&hinge);
    Select({ SK.GetGroup(box)->Remap(Entity::NO_ENTITY, Group::REMAP_TOP),
             SK.GetGroup(lid)->Remap(Entity::NO_ENTITY, Group::REMAP_BOTTOM) });
    CHECK_TRUE(Driver::ClickMenu({ "New Group", "Joints", "Planar Joint" }));
    CHECK_TRUE(SK.joint.n == 1);
    hJoint hj = SK.joint.begin()->h;
    OutlinerToEnd();
    CHECK_TRUE(Driver::ClickItem("joint:" + std::to_string(hj.v)));
    CHECK_TRUE(RevealIn("Properties", "joint-value1"));
    CHECK_TRUE(EditField("joint-value1", "2"));
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[1], 2);
    CHECK_TRUE(RevealIn("Properties", "joint-value2"));
    CHECK_TRUE(EditField("joint-value2", "30"));
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[2], 30);
    // G on the lid, Z for the turn, a typed angle.
    Driver::Key("Escape");
    hPart   lidPart = SK.GetGroup(lid)->PartOf();
    Point2d onLid   = At(SS.PartPose(lidPart).Apply({ 5, 3, 32 }));
    Driver::Move(onLid.x, onLid.y);
    Driver::Key("G");
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::GRABBING && SS.GW.grab.joint == hj);
    Driver::Key("Z");
    Driver::Type("15");
    Driver::Key("Return");
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[2], 45);
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[1], 2);
}
