// Fit joints and the fit table in the app (M5b): a peg on a box and a pocket in its lid, made
// from the menu; the fit chosen in Properties; the table retuned from Inspect > Fit Table.
// box.slvs: a 10 x 20 x 30 box.
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

#if defined(HAVE_OCCT)
namespace {

hRequest AddLine(Vector a, Vector b) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

void Select(std::vector<hEntity> entities) {
    SS.GW.ClearSelection();
    for(hEntity he : entities) {
        GraphicsWindow::Selection sel = {};
        sel.entity                    = he;
        SS.GW.selection.Add(&sel);
    }
}

double PartVolume(hPart hp) {
    MassProps mp;
    if(!SS.MassProperties(SS.PartSolid(hp), &mp)) return -1;
    return mp.volume;
}

}

TEST_CASE(ng_fits_a_peg_into_a_lid) {
    CHECK_LOAD("box.slvs");
    hEntity top = SK.GetGroup(SS.GW.activeGroup)->Remap(Entity::NO_ENTITY, Group::REMAP_TOP);
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    // A 10 x 20 x 5 lid, its own part.
    Select({ top });
    hPart lid = SS.NewPart();
    Vector p[4] = { { 0, 0, 30 }, { 10, 0, 30 }, { 10, 20, 30 }, { 0, 20, 30 } };
    hRequest side[4];
    for(int i = 0; i < 4; i++) side[i] = AddLine(p[i], p[(i + 1) % 4]);
    for(int i = 0; i < 4; i++) {
        Constraint::ConstrainCoincident(side[i].entity(2), side[(i + 1) % 4].entity(1));
    }
    Group::MenuGroup(Command::GROUP_EXTRUDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *e = SK.GetGroup(SS.GW.activeGroup);
    SK.GetParam(e->h.param(0))->val = 0;
    SK.GetParam(e->h.param(1))->val = 0;
    SK.GetParam(e->h.param(2))->val = 2.5;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hEntity lidTop = SK.GetGroup(SS.GW.activeGroup)->Remap(Entity::NO_ENTITY, Group::REMAP_TOP);
    // The shared profile: a square peg, 4 across, on the box's top.
    Select({ top });
    Group::MenuGroup(Command::GROUP_WRKPL);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector q[4] = { { 3, 8, 30 }, { 7, 8, 30 }, { 7, 12, 30 }, { 3, 12, 30 } };
    for(int i = 0; i < 4; i++) side[i] = AddLine(q[i], q[(i + 1) % 4]);
    for(int i = 0; i < 4; i++) {
        Constraint::ConstrainCoincident(side[i].entity(2), side[(i + 1) % 4].entity(1));
    }
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Select({ top, lidTop });
    CHECK_TRUE(Driver::ClickMenu({ "New Group", "Joints", "Fit Joint" }));
    Group *pocket = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(pocket->type == Group::Type::FIT && pocket->fit.female);
    // The square pocket, 4.4 across and 5 deep (through the lid: 5.2 is past it), its
    // corners rounded by the clearance so they clear the peg's by it too.
    double corners = 4 * (0.2 * 0.2 - PI * 0.2 * 0.2 / 4) * 5;
    CHECK_TRUE(fabs(PartVolume(lid) - (1000 - 4.4 * 4.4 * 5 + corners)) < 1e-3);
    // Pressed instead, in Properties.
    CHECK_TRUE(RevealIn("Properties", "fit-kind-press"));
    CHECK_TRUE(Driver::ClickItem("fit-kind-press"));
    CHECK_TRUE(fabs(PartVolume(lid) - (1000 - 3.8 * 3.8 * 5)) < 1e-3);
    // The fit table, from the Inspect menu: retune the press fit.
    CHECK_TRUE(Driver::ClickMenu({ "Inspect", "Fit Table..." }));
    hFit press = SS.FitTable()[0].h;
    CHECK_TRUE(EditField("fit-clearance-" + std::to_string(press.v), "-0.05"));
    CHECK_EQ_EPS(SS.FindFit(press)->clearance, -0.05);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(fabs(PartVolume(lid) - (1000 - 3.9 * 3.9 * 5)) < 1e-3);
    CHECK_TRUE(Driver::ClickItem("fit-add"));
    CHECK_TRUE(SS.FitTable().size() == 4);
}
#endif
