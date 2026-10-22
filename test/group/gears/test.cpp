// Gear and rack couplings (AGENTS.md section 7, M5b): a coupling ties motion 0 of two joints,
// driven = ratio × driver. box.slvs: a 10 x 20 x 30 box (part 1).
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

hEntity Circle(Vector centre, double r) {
    hRequest c = SS.GW.AddRequest(Request::Type::CIRCLE, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(c.entity(1))->PointForceTo(centre);
    SK.GetEntity(c.entity(64))->DistanceForceTo(r);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return c.entity(0);
}

// On the box's top: two axles (the box's part), gear A (r 2) on the first and gear B (r 4)
// on the second, each its own part turning on its axle, and a rack sliding along the front.
struct Train {
    hEntity a, b, rackLine;
    hJoint  ja, jb, jr;
    hPart   pa, pb, pr;
};

Train MakeTrain() {
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    Train t;
    Select({ Cap(Group::REMAP_TOP) });
    Group::MenuGroup(Command::GROUP_WRKPL);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hEntity axleA = Circle({ 3, 5, 30 }, 0.5), axleB = Circle({ 3, 11, 30 }, 0.5);
    hEntity rail  = AddLine({ 0, 1, 30 }, { 10, 1, 30 }).entity(0);
    Select({ Cap(Group::REMAP_TOP) });
    t.pa = SS.NewPart();
    t.a  = Circle({ 3, 5, 30 }, 2);
    Select({ Cap(Group::REMAP_TOP) });
    t.pb = SS.NewPart();
    t.b  = Circle({ 3, 11, 30 }, 4);
    Select({ Cap(Group::REMAP_TOP) });
    t.pr       = SS.NewPart();
    t.rackLine = AddLine({ 0, 3, 30 }, { 10, 3, 30 }).entity(0);
    Select({ axleA, t.a });
    t.ja = SS.MakeJoint(Joint::Type::REVOLUTE);
    Select({ axleB, t.b });
    t.jb = SS.MakeJoint(Joint::Type::REVOLUTE);
    Select({ rail, t.rackLine });
    t.jr = SS.MakeJoint(Joint::Type::SLIDER);
    return t;
}

double Value(hJoint hj) {
    return SK.GetJoint(hj)->value[0];
}

}

TEST_CASE(meshed_gears_turn_opposite_at_their_ratio) {
    CHECK_LOAD("box.slvs");
    Train t = MakeTrain();
    CHECK_TRUE(t.ja.v && t.jb.v && t.jr.v);
    Select({ t.a, t.b });
    hJoint g = SS.MakeJoint(Joint::Type::GEAR);
    CHECK_TRUE(g.v != 0);
    Joint *c = SK.GetJoint(g);
    CHECK_TRUE(c->driver == t.ja && c->driven == t.jb);
    CHECK_EQ_EPS(c->ratio, -0.5); // pitch radii 2 and 4
    SS.SetJointValue(t.ja, 90);
    CHECK_EQ_EPS(Value(t.jb), -45);
    SS.SetJointValue(t.jb, 30); // either end drives
    CHECK_EQ_EPS(Value(t.ja), -60);
    // Limits on one clamp the pair.
    SS.SetJointLimits(t.jb, true, -30, 30);
    SS.SetJointValue(t.ja, 90);
    CHECK_EQ_EPS(Value(t.ja), 60);
    CHECK_EQ_EPS(Value(t.jb), -30);
    // A ratio typed in Properties; the pair moves back into step, within the limits.
    SS.SetJointRatio(g, -1);
    CHECK_EQ_EPS(Value(t.ja), 30);
    CHECK_EQ_EPS(Value(t.jb), -30);
    // One undo step.
    SS.UndoUndo();
    CHECK_EQ_EPS(SK.GetJoint(g)->ratio, -0.5);
}

TEST_CASE(a_rack_slides_with_its_pinion) {
    CHECK_LOAD("box.slvs");
    Train t = MakeTrain();
    Select({ t.a, t.rackLine });
    hJoint g = SS.MakeJoint(Joint::Type::GEAR);
    CHECK_TRUE(g.v != 0);
    CHECK_EQ_EPS(SK.GetJoint(g)->ratio, 2 * PI / 180); // mm per degree
    SS.SetJointValue(t.ja, 90);
    CHECK_EQ_EPS(Value(t.jr), PI);
    // Through a chain: gear B drives A drives the rack.
    Select({ t.b, t.a });
    hJoint g2 = SS.MakeJoint(Joint::Type::GEAR);
    CHECK_TRUE(g2.v != 0);
    SS.SetJointValue(t.jb, 45); // A turns -90
    CHECK_EQ_EPS(Value(t.ja), -90);
    CHECK_EQ_EPS(Value(t.jr), -PI);
    // A coupling needs two joints that aren't coupled already.
    Select({ t.a, t.b });
    CHECK_TRUE(SS.MakeJoint(Joint::Type::GEAR).v == 0);
    Select({ t.a, Cap(Group::REMAP_TOP) }); // the box: no joint moves it
    CHECK_TRUE(SS.MakeJoint(Joint::Type::GEAR).v == 0);
}

TEST_CASE(a_coupling_survives_save_and_goes_with_its_joints) {
    CHECK_LOAD("box.slvs");
    Train t = MakeTrain();
    Select({ t.a, t.b });
    hJoint g = SS.MakeJoint(Joint::Type::GEAR);
    SS.SetJointValue(t.ja, 20);
    Platform::Path path = helper->GetAssetPath(__FILE__, "gears.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    remove(path.raw.c_str());
    Joint *c = SK.GetJoint(g);
    CHECK_TRUE(c->type == Joint::Type::GEAR && c->driver == t.ja && c->driven == t.jb);
    CHECK_EQ_EPS(c->ratio, -0.5);
    SS.SetJointValue(t.ja, 40);
    CHECK_EQ_EPS(Value(t.jb), -20);
    // The design pose leaves the coupling in step.
    SS.DesignPose();
    CHECK_EQ_EPS(Value(t.jb), 0);
    SS.DeleteJoint(t.ja);
    CHECK_TRUE(SK.joint.FindByIdNoOops(g) == nullptr);
    CHECK_TRUE(SK.joint.FindByIdNoOops(t.jb) != nullptr);
}
