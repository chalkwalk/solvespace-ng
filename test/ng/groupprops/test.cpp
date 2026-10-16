#include "solvespace.h"

#include "harness.h"

static Group *FirstOfType(Group::Type type) {
    for(hGroup hg : SK.groupOrder) {
        if(SK.GetGroup(hg)->type == type) return SK.GetGroup(hg);
    }
    return nullptr;
}

static Group *StepTranslate() {
    SS.GW.ActivateCommand(Command::GROUP_TRANS);
    SS.Refresh();
    return FirstOfType(Group::Type::TRANSLATE);
}

TEST_CASE(group_option_flags_toggle_and_undo) {
    CHECK_LOAD("line.slvs");
    Group *g = FirstOfType(Group::Type::DRAWING_WORKPLANE);
    hGroup hg = g->h;
    SS.ChangeGroupOption(hg, GroupOption::RELAX_CONSTRAINTS);
    CHECK_TRUE(SK.GetGroup(hg)->relaxConstraints);
    SS.ChangeGroupOption(hg, GroupOption::ALLOW_REDUNDANT);
    SS.ChangeGroupOption(hg, GroupOption::SUPPRESS_DOF);
    SS.ChangeGroupOption(hg, GroupOption::ALL_DIMS_REFERENCE);
    SS.ChangeGroupOption(hg, GroupOption::FORCE_TO_MESH);
    g = SK.GetGroup(hg);
    CHECK_TRUE(g->allowRedundant && g->suppressDofCalculation && g->allDimsReference &&
               g->forceToMesh);
    SS.UndoUndo();
    CHECK_FALSE(SK.GetGroup(hg)->forceToMesh);
}

TEST_CASE(extrude_sides_skew_and_combine) {
    CHECK_LOAD("extrude.slvs");
    hGroup hg = FirstOfType(Group::Type::EXTRUDE)->h;
    SS.ChangeGroupOption(hg, GroupOption::TWO_SIDED);
    CHECK_TRUE(SK.GetGroup(hg)->subtype == Group::Subtype::TWO_SIDED);
    SS.ChangeGroupOption(hg, GroupOption::SKEWED);
    CHECK_TRUE(SK.GetGroup(hg)->subtype == Group::Subtype::TWO_SKEWED);
    SS.ChangeGroupOption(hg, GroupOption::ONE_SIDED);
    CHECK_TRUE(SK.GetGroup(hg)->subtype == Group::Subtype::ONE_SKEWED);
    SS.ChangeGroupOption(hg, GroupOption::COMBINE, Group::CombineAs::ASSEMBLE);
    CHECK_TRUE(SK.GetGroup(hg)->meshCombine == Group::CombineAs::ASSEMBLE);
    SS.ChangeGroupOption(hg, GroupOption::SUPPRESS_SOLID);
    CHECK_TRUE(SK.GetGroup(hg)->suppress);
}

TEST_CASE(repeat_count_is_checked) {
    CHECK_LOAD("line.slvs");
    Group *g = StepTranslate();
    CHECK_TRUE(g != nullptr);
    hGroup hg = g->h;
    CHECK_FALSE(SS.SetGroupRepeat(hg, "0"));
    CHECK_FALSE(SS.SetGroupRepeat(hg, "1000"));
    CHECK_FALSE(SS.SetGroupRepeat(hg, "2+"));
    CHECK_TRUE(SS.SetGroupRepeat(hg, "1+2"));
    CHECK_EQ_EPS(SK.GetGroup(hg)->valA, 3.0);
    SS.ChangeGroupOption(hg, GroupOption::START_WITH_COPY);
    CHECK_TRUE(SK.GetGroup(hg)->skipFirst);
    SS.ChangeGroupOption(hg, GroupOption::START_WITH_ORIGINAL);
    CHECK_FALSE(SK.GetGroup(hg)->skipFirst);
}

TEST_CASE(colour_and_opacity) {
    CHECK_LOAD("extrude.slvs");
    hGroup hg = FirstOfType(Group::Type::EXTRUDE)->h;
    SS.SetGroupColor(hg, RgbaColor::FromFloat(1, 0, 0), /*rememberUndo=*/true);
    CHECK_TRUE(SK.GetGroup(hg)->color.red == 255 && SK.GetGroup(hg)->color.green == 0);
    CHECK_FALSE(SS.SetGroupOpacity(hg, "1.5"));
    CHECK_TRUE(SS.SetGroupOpacity(hg, "0.5"));
    CHECK_TRUE(SK.GetGroup(hg)->color.alpha == 127);
    CHECK_TRUE(SK.GetGroup(hg)->color.red == 255); // the colour stays
}
