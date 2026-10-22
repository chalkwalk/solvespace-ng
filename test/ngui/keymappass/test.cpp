// The keys the keymap pass gave (2026-10-22, docs/research/keymap-pass.md): bare = the
// command, Shift = its counterpart, Alt = its variant, Ctrl = the document, pies for big
// families. box.slvs: a 10 x 20 rectangle extruded 30 (request 5: its line along +X).
#include "../uitest.h"
#include "ui/piemenu.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

namespace {

Group *Active() {
    return SK.GetGroup(SS.GW.activeGroup);
}

hEntity TopFace() {
    return Active()->Remap(Entity::NO_ENTITY, Group::REMAP_TOP);
}

void Select(std::vector<hEntity> entities) {
    SS.GW.ClearSelection();
    for(hEntity he : entities) {
        GraphicsWindow::Selection sel = {};
        sel.entity                    = he;
        SS.GW.selection.Add(&sel);
    }
}

hRequest AddLine(Vector a, Vector b) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

}

// F fillets (Fusion, Shapr3D); Alt+F, its variant, chamfers.
TEST_CASE(alt_f_chamfers_the_picked_edge) {
    CHECK_LOAD("box.slvs");
    Driver::Key("Shift+7");
    hEntity edge = Active()->Remap(hRequest{ 5 }.entity(0), Group::REMAP_TOP);
    ClickAt(At({ 5, 0, 30 }));
    CHECK_TRUE(SS.GW.IsSelected(edge));
    Driver::Key("Alt+F");
    CHECK_TRUE(Active()->type == Group::Type::CHAMFER);
}

// Alt+W sketches on the picked face; Shift+E, extrude's counterpart, extrudes as a cut.
TEST_CASE(shift_e_extrudes_as_a_cut) {
    CHECK_LOAD("box.slvs");
    Select({ TopFace() });
    Driver::Key("Alt+W");
    CHECK_TRUE(Active()->type == Group::Type::DRAWING_WORKPLANE);
    hRequest c = SS.GW.AddRequest(Request::Type::CIRCLE, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(c.entity(1))->PointForceTo({ 5, 10, 30 });
    SK.GetEntity(c.entity(64))->DistanceForceTo(2);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SS.GW.ClearSelection();
    Driver::Key("Shift+E");
    CHECK_TRUE(Active()->type == Group::Type::EXTRUDE);
    CHECK_TRUE(Active()->meshCombine == Group::CombineAs::DIFFERENCE);
    Driver::Key("Ctrl+Z");
    Driver::Key("E");
    CHECK_TRUE(Active()->type == Group::Type::EXTRUDE);
    CHECK_TRUE(Active()->meshCombine == Group::CombineAs::UNION);
}

// Ctrl+Shift+N starts a part; J opens the joint pie; Shift+J, the joints' counterpart, goes
// back to the design pose.
TEST_CASE(j_makes_joints_and_shift_j_returns_to_the_design_pose) {
    CHECK_LOAD("box.slvs");
    hEntity edge = Active()->Remap(hRequest{ 5 }.entity(0), Group::REMAP_TOP);
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    Select({ TopFace() });
    Driver::Key("Ctrl+Shift+N");
    CHECK_TRUE(SK.part.n == 2);
    hRequest hinge = AddLine({ 0, 0, 30 }, { 10, 0, 30 });
    Select({ edge, hinge.entity(0) });
    Driver::Key("J");
    CHECK_TRUE(Ui::Pie().IsOpen() && Ui::Pie().Items().size() == 8);
    CHECK_TRUE(PickPie("Revolute"));
    CHECK_TRUE(SK.joint.n == 1);
    hJoint hj = SK.joint.begin()->h;
    SS.SetJointValue(hj, 30);
    Driver::Key("Escape");
    Driver::Key("Shift+J");
    CHECK_EQ_EPS(SK.GetJoint(hj)->value[0], 0);
}

// The view pie holds the grid (a view setting), and F8 the Problems panel.
TEST_CASE(the_view_pie_toggles_the_grid) {
    CHECK_LOAD("box.slvs");
    bool grid = SS.GW.showSnapGrid;
    Driver::Key("`");
    CHECK_TRUE(PickPie("Grid"));
    CHECK_TRUE(SS.GW.showSnapGrid != grid);
}
