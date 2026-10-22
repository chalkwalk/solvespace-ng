// Motion joints (AGENTS.md section 7, M5a): a joint turns or slides its child part about or
// along an axis on its parent, without rebuilding either. box.slvs: a 10 x 20 x 30 box;
// request 5 is its sketch's line from the origin along +X.
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

namespace {

hRequest AddLine(Vector a, Vector b) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

// Part 2: a 10 x 6 lid on the box's top, 2 thick, hinged (by design) along the box's top
// front edge, (0,0,30)-(10,0,30). Its sketch's lines: [0] along that edge, [2] at y = 6.
struct Lid {
    hPart    part;
    hRequest side[4];
};

Lid LidOnTop() {
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    Select({ Cap(Group::REMAP_TOP) });
    Lid lid;
    lid.part    = SS.NewPart();
    Vector p[4] = { { 0, 0, 30 }, { 10, 0, 30 }, { 10, 6, 30 }, { 0, 6, 30 } };
    for(int i = 0; i < 4; i++) lid.side[i] = AddLine(p[i], p[(i + 1) % 4]);
    for(int i = 0; i < 4; i++) {
        Constraint::ConstrainCoincident(lid.side[i].entity(2), lid.side[(i + 1) % 4].entity(1));
    }
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return lid;
}

hEntity BoxFrontTop() {
    return Edge(5, Group::REMAP_TOP);
}

bool Near(Vector a, Vector b) {
    return a.Equals(b, 1e-9);
}

// The lid made solid, 2 thick (up from the box's top).
hGroup ExtrudeLid() {
    Group::MenuGroup(Command::GROUP_EXTRUDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *e = SK.GetGroup(SS.GW.activeGroup);
    SK.GetParam(e->h.param(0))->val = 0;
    SK.GetParam(e->h.param(1))->val = 0;
    SK.GetParam(e->h.param(2))->val = 1;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return e->h;
}

}

TEST_CASE(a_revolute_joint_turns_the_child_about_the_shared_axis) {
    CHECK_LOAD("box.slvs");
    Lid lid = LidOnTop();
    Select({ BoxFrontTop(), lid.side[0].entity(0) });
    hJoint hj = SS.MakeJoint(Joint::Type::REVOLUTE);
    CHECK_TRUE(hj.v != 0);
    Joint *j = SK.GetJoint(hj);
    CHECK_TRUE(j->parent == Part::FIRST && j->child == lid.part);
    CHECK_TRUE(SS.PartPose(lid.part).IsIdentity()); // the design pose
    SS.SetJointValue(hj, 90);
    // About +X through (0,0,30): the lid's far corner swings up.
    CHECK_TRUE(Near(SS.PartPose(lid.part).Apply({ 10, 6, 30 }), { 10, 0, 36 }));
    CHECK_TRUE(SS.PartPose(Part::FIRST).IsIdentity());
    // Its sketch stays in the design pose, so it is hidden while the part is moved.
    CHECK_FALSE(SK.GetEntity(lid.side[0].entity(0))->IsVisible());
    CHECK_TRUE(SK.GetEntity(BoxFrontTop())->IsVisible());
    SS.DesignPose();
    CHECK_TRUE(SK.GetEntity(lid.side[0].entity(0))->IsVisible());
}

TEST_CASE(a_revolute_joint_needs_the_same_axis_on_both_parts) {
    CHECK_LOAD("box.slvs");
    Lid lid = LidOnTop();
    Select({ BoxFrontTop(), lid.side[2].entity(0) }); // parallel, 6 apart
    CHECK_TRUE(SS.MakeJoint(Joint::Type::REVOLUTE).v == 0);
    CHECK_TRUE(SK.joint.IsEmpty());
    // Two entities of one part make no joint either.
    Select({ lid.side[0].entity(0), lid.side[2].entity(0) });
    CHECK_TRUE(SS.MakeJoint(Joint::Type::SLIDER).v == 0);
}

TEST_CASE(a_slider_joint_moves_the_child_along_parallel_axes_within_its_limits) {
    CHECK_LOAD("box.slvs");
    Lid lid = LidOnTop();
    Select({ BoxFrontTop(), lid.side[2].entity(0) });
    hJoint hj = SS.MakeJoint(Joint::Type::SLIDER);
    CHECK_TRUE(hj.v != 0);
    SS.SetJointValue(hj, 5);
    CHECK_TRUE(Near(SS.PartPose(lid.part).Apply({ 0, 0, 30 }), { 5, 0, 30 }));
    SS.SetJointLimits(hj, true, 0, 3);
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 3);
    SS.SetJointValue(hj, -2);
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 0);
    // A part has one parent joint.
    Select({ BoxFrontTop(), lid.side[0].entity(0) });
    CHECK_TRUE(SS.MakeJoint(Joint::Type::REVOLUTE).v == 0);
}

TEST_CASE(a_chain_of_joints_carries_the_parts_after) {
    CHECK_LOAD("box.slvs");
    Lid lid = LidOnTop();
    Select({ BoxFrontTop(), lid.side[0].entity(0) });
    hJoint hinge = SS.MakeJoint(Joint::Type::REVOLUTE);
    // Part 3: a bar along the lid's far edge, sliding along it.
    SS.GW.ClearSelection();
    hPart    bar  = SS.NewPart();
    hRequest rail = AddLine({ 10, 6, 30 }, { 0, 6, 30 });
    Select({ lid.side[2].entity(0), rail.entity(0) });
    hJoint slide = SS.MakeJoint(Joint::Type::SLIDER);
    CHECK_TRUE(slide.v != 0 && SK.GetJoint(slide)->parent == lid.part);
    SS.SetJointValue(slide, 4); // along the lid's edge, from x = 10 to 0
    SS.SetJointValue(hinge, 90);
    CHECK_TRUE(Near(SS.PartPose(bar).Apply({ 5, 6, 30 }), { 1, 0, 36 }));
    SS.DesignPose();
    CHECK_TRUE(SS.PartPose(bar).IsIdentity());
}

TEST_CASE(joints_survive_save_reload_and_undo) {
    CHECK_LOAD("box.slvs");
    Lid lid = LidOnTop();
    Select({ BoxFrontTop(), lid.side[0].entity(0) });
    hJoint hj = SS.MakeJoint(Joint::Type::REVOLUTE);
    SS.SetJointLimits(hj, true, -10, 120);
    SS.SetJointValue(hj, 45);
    SS.SetJointValue(hj, 60);
    SS.UndoUndo();
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 45);
    Platform::Path path = helper->GetAssetPath(__FILE__, "joints.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    Joint *j = SK.GetJoint(hj);
    CHECK_TRUE(j->type == Joint::Type::REVOLUTE && j->child == lid.part && j->limited[0]);
    CHECK_EQ_EPS(j->value[0], 45);
    CHECK_EQ_EPS(j->hi[0], 120);
    CHECK_TRUE(Near(SS.PartPose(lid.part).Apply({ 10, 6, 30 }),
                    Vector::From(10, 6 * cos(PI / 4), 30 + 6 * sin(PI / 4))));
    remove(path.raw.c_str());
}

TEST_CASE(a_joint_goes_with_its_axis_entity) {
    CHECK_LOAD("box.slvs");
    Lid lid = LidOnTop();
    Select({ BoxFrontTop(), lid.side[0].entity(0) });
    CHECK_TRUE(SS.MakeJoint(Joint::Type::REVOLUTE).v != 0);
    SK.request.RemoveById(lid.side[0]);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.joint.IsEmpty());
}

// A posed part shows (and is picked as) its solid moved; its own mesh, exports and mass
// properties stay in the design pose.
TEST_CASE(a_posed_part_shows_its_solid_moved) {
    CHECK_LOAD("box.slvs");
    Lid lid = LidOnTop();
    Group::MenuGroup(Command::GROUP_EXTRUDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *e = SK.GetGroup(SS.GW.activeGroup);
    SK.GetParam(e->h.param(0))->val = 0;
    SK.GetParam(e->h.param(1))->val = 0;
    SK.GetParam(e->h.param(2))->val = 1; // 2 thick
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hGroup lidSolid = SS.GW.activeGroup;
    Select({ BoxFrontTop(), lid.side[0].entity(0) });
    hJoint hj = SS.MakeJoint(Joint::Type::REVOLUTE);
    auto zmax = [&](SMesh &m) {
        double z = VERY_NEGATIVE;
        for(const STriangle &t : m.l) {
            for(const Vector &v : t.vertices) z = std::max(z, v.z);
        }
        return z;
    };
    Group *g = SK.GetGroup(lidSolid);
    g->GenerateDisplayItems();
    CHECK_EQ_EPS(zmax(g->ShownMesh()), 32);
    SS.SetJointValue(hj, 90);
    g = SK.GetGroup(lidSolid);
    g->GenerateDisplayItems();
    CHECK_EQ_EPS(zmax(g->ShownMesh()), 36);
    CHECK_EQ_EPS(zmax(g->displayMesh), 32);
    CHECK_TRUE(!g->ShownOutlines().l.IsEmpty());
    SS.DesignPose();
    g = SK.GetGroup(lidSolid);
    g->GenerateDisplayItems();
    CHECK_EQ_EPS(zmax(g->ShownMesh()), 32);
}

// G on a face of a jointed part turns the joint with the mouse round its axis.
TEST_CASE(grab_turns_a_hinged_part_with_the_mouse) {
    CHECK_LOAD("box.slvs");
    SS.showToolbar = false;
    Lid lid = LidOnTop();
    Group::MenuGroup(Command::GROUP_EXTRUDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hGroup lidSolid = SS.GW.activeGroup;
    Select({ BoxFrontTop(), lid.side[0].entity(0) });
    hJoint hj = SS.MakeJoint(Joint::Type::REVOLUTE);
    // Looking along the hinge (+X).
    SS.GW.projRight = Vector::From(0, 1, 0);
    SS.GW.projUp    = Vector::From(0, 0, 1);
    SS.GW.offset    = Vector::From(0, 0, 0);
    Select({ SK.GetGroup(lidSolid)->Remap(Entity::NO_ENTITY, Group::REMAP_TOP) });
    CHECK_TRUE(SS.GW.StartGrab(SS.GW.ProjectPoint({ 5, 6, 30 })));
    SS.GW.GrabMouseMoved(SS.GW.ProjectPoint({ 5, 6 * cos(PI / 4), 30 + 6 * sin(PI / 4) }));
    SS.GW.GrabMouseMoved(SS.GW.ProjectPoint({ 5, 0, 36 }));
    CHECK_TRUE(fabs(SK.GetJoint(hj)->value[0] - 90) < 1e-6);
    SS.GW.GrabCancel();
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 0);
    // A typed angle; confirmed, it is one undo step.
    Select({ SK.GetGroup(lidSolid)->Remap(Entity::NO_ENTITY, Group::REMAP_TOP) });
    CHECK_TRUE(SS.GW.StartGrab(SS.GW.ProjectPoint({ 5, 6, 30 })));
    SS.GW.GrabSetTyped(30);
    SS.GW.GrabConfirm();
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 30);
    SS.UndoUndo();
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 0);
}

// Multi-motion joints (M5b): a cylindrical joint turns and slides about one line.
TEST_CASE(a_cylindrical_joint_turns_and_slides_about_its_axis) {
    CHECK_LOAD("box.slvs");
    Lid lid = LidOnTop();
    Select({ BoxFrontTop(), lid.side[2].entity(0) }); // parallel, not one line
    CHECK_TRUE(SS.MakeJoint(Joint::Type::CYLINDRICAL).v == 0);
    Select({ BoxFrontTop(), lid.side[0].entity(0) });
    hJoint hj = SS.MakeJoint(Joint::Type::CYLINDRICAL);
    CHECK_TRUE(hj.v != 0);
    CHECK_TRUE(SK.GetJoint(hj)->Motions() == 2);
    SS.SetJointValue(hj, 90, true, 0);
    SS.SetJointValue(hj, 5, true, 1);
    CHECK_TRUE(Near(SS.PartPose(lid.part).Apply({ 10, 6, 30 }), { 15, 0, 36 }));
    // Each motion has its own limits.
    SS.SetJointLimits(hj, true, 0, 3, 1);
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[1], 3);
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 90);
    SS.SetJointValue(hj, 400, true, 0);
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 400);
    SS.SetJointValue(hj, 90, true, 0);
    Platform::Path path = helper->GetAssetPath(__FILE__, "cylindrical.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    Joint *j = SK.GetJoint(hj);
    CHECK_TRUE(j->type == Joint::Type::CYLINDRICAL && !j->limited[0] && j->limited[1]);
    CHECK_EQ_EPS(j->value[0], 90);
    CHECK_EQ_EPS(j->value[1], 3);
    CHECK_EQ_EPS(j->hi[1], 3);
    remove(path.raw.c_str());
}

// A planar joint slides the child in a shared plane and turns it about the plane's normal.
TEST_CASE(a_planar_joint_slides_and_turns_in_its_plane) {
    CHECK_LOAD("box.slvs");
    Lid    lid      = LidOnTop();
    hGroup lidSolid = ExtrudeLid();
    hEntity under   = SK.GetGroup(lidSolid)->Remap(Entity::NO_ENTITY, Group::REMAP_BOTTOM);
    Select({ Cap(Group::REMAP_TOP), lid.side[0].entity(0) }); // a line in the plane: no
    CHECK_TRUE(SS.MakeJoint(Joint::Type::PLANAR).v == 0);
    Select({ Cap(Group::REMAP_TOP), under });
    hJoint hj = SS.MakeJoint(Joint::Type::PLANAR);
    CHECK_TRUE(hj.v != 0);
    CHECK_TRUE(SK.GetJoint(hj)->Motions() == 3);
    Vector p, n;
    CHECK_TRUE(Joint::AxisOf(SK.GetJoint(hj)->axisA, &p, &n));
    Vector u = n.Normal(0), v = n.Normal(1);
    SS.SetJointValue(hj, 3, true, 0);
    SS.SetJointValue(hj, 4, true, 1);
    Vector moved = SS.PartPose(lid.part).Apply({ 10, 6, 30 });
    CHECK_TRUE(Near(moved, Vector::From(10, 6, 30).Plus(u.ScaledBy(3)).Plus(v.ScaledBy(4))));
    SS.SetJointValue(hj, 90, true, 2);
    Pose pose = SS.PartPose(lid.part);
    // Turned a quarter about the normal: the lid's edge along X now runs along Y; it stays in
    // the plane.
    Vector edge = pose.Apply({ 10, 0, 30 }).Minus(pose.Apply({ 0, 0, 30 }));
    CHECK_TRUE(Near(edge, n.Cross(Vector::From(10, 0, 0))));
    CHECK_EQ_EPS(pose.Apply({ 3, 3, 30 }).z, 30);
}

// A ball joint turns the child about x, then y, then z through a shared centre.
TEST_CASE(a_ball_joint_turns_about_its_centre) {
    CHECK_LOAD("box.slvs");
    Lid     lid    = LidOnTop();
    hEntity corner = SK.GetEntity(BoxFrontTop())->point[0]; // (0,0,30)
    Select({ corner, lid.side[2].entity(1) });               // (10,6,30): apart
    CHECK_TRUE(SS.MakeJoint(Joint::Type::BALL).v == 0);
    Select({ corner, lid.side[0].entity(1) });
    hJoint hj = SS.MakeJoint(Joint::Type::BALL);
    CHECK_TRUE(hj.v != 0);
    SS.SetJointValue(hj, 90, true, 0);
    CHECK_TRUE(Near(SS.PartPose(lid.part).Apply({ 10, 6, 30 }), { 10, 0, 36 }));
    SS.SetJointValue(hj, 90, true, 2);
    CHECK_TRUE(Near(SS.PartPose(lid.part).Apply({ 10, 6, 30 }), { 0, 10, 36 }));
}

// G with a joint of several motions: X, Y and Z pick one, else the joint's natural drag.
TEST_CASE(grab_picks_a_motion_of_a_multi_motion_joint) {
    CHECK_LOAD("box.slvs");
    SS.showToolbar  = false;
    Lid    lid      = LidOnTop();
    hGroup lidSolid = ExtrudeLid();
    hEntity under   = SK.GetGroup(lidSolid)->Remap(Entity::NO_ENTITY, Group::REMAP_BOTTOM);
    hEntity top     = SK.GetGroup(lidSolid)->Remap(Entity::NO_ENTITY, Group::REMAP_TOP);
    Select({ Cap(Group::REMAP_TOP), under });
    hJoint hj = SS.MakeJoint(Joint::Type::PLANAR);
    // Looking down on the plane: the lid follows the mouse in it.
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    SS.GW.offset    = Vector::From(0, 0, 0);
    Select({ top });
    CHECK_TRUE(SS.GW.StartGrab(SS.GW.ProjectPoint({ 5, 3, 32 })));
    SS.GW.GrabMouseMoved(SS.GW.ProjectPoint({ 8, 7, 32 }));
    Vector moved = SS.PartPose(lid.part).Apply({ 5, 3, 30 });
    CHECK_TRUE(moved.Equals({ 8, 7, 30 }, 1e-6));
    // Z picks the turn; a typed angle sets it, and the slide goes back.
    SS.GW.GrabSetAxis(2);
    SS.GW.GrabSetTyped(30);
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[2], 30);
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 0);
    SS.GW.GrabConfirm();
    SS.UndoUndo();
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[2], 0);
}

// G on a ball-jointed part turns it like a trackball: the mouse pushes its near side.
TEST_CASE(grab_turns_a_ball_jointed_part_like_a_trackball) {
    CHECK_LOAD("box.slvs");
    SS.showToolbar  = false;
    Lid    lid      = LidOnTop();
    hGroup lidSolid = ExtrudeLid();
    hEntity top     = SK.GetGroup(lidSolid)->Remap(Entity::NO_ENTITY, Group::REMAP_TOP);
    Select({ SK.GetEntity(BoxFrontTop())->point[0], lid.side[0].entity(1) });
    hJoint hj = SS.MakeJoint(Joint::Type::BALL);
    CHECK_TRUE(hj.v != 0);
    // Looking down -Z; the mouse moves along +X on screen: a turn about +Y.
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    SS.GW.offset    = Vector::From(0, 0, 0);
    SS.GW.scale     = 5;
    Select({ top });
    Point2d from = SS.GW.ProjectPoint({ 5, 3, 32 });
    CHECK_TRUE(SS.GW.StartGrab(from));
    SS.GW.GrabMouseMoved({ from.x + 60, from.y });
    Joint *j = SK.GetJoint(hj);
    CHECK_TRUE(fabs(j->value[0]) < 1e-6 && fabs(j->value[2]) < 1e-6);
    CHECK_TRUE(fabs(j->value[1] - 30) < 1e-6);
    SS.GW.GrabCancel();
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[1], 0);
}
