#include "solvespace.h"

#include "harness.h"

static bool Near(Vector a, Vector b) { return a.Minus(b).Magnitude() < 1e-9; }

TEST_CASE(view_preset_bases_are_z_up) {
    Vector r, u;
    ViewPresetBasis(ViewPreset::FRONT, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(1, 0, 0)) && Near(u, Vector::From(0, 0, 1)));
    ViewPresetBasis(ViewPreset::BACK, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(-1, 0, 0)) && Near(u, Vector::From(0, 0, 1)));
    ViewPresetBasis(ViewPreset::RIGHT, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(0, 1, 0)) && Near(u, Vector::From(0, 0, 1)));
    ViewPresetBasis(ViewPreset::LEFT, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(0, -1, 0)) && Near(u, Vector::From(0, 0, 1)));
    ViewPresetBasis(ViewPreset::TOP, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(1, 0, 0)) && Near(u, Vector::From(0, 1, 0)));
    ViewPresetBasis(ViewPreset::BOTTOM, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(1, 0, 0)) && Near(u, Vector::From(0, -1, 0)));
}

TEST_CASE(snap_to_view_without_animation) {
    CHECK_LOAD("line.slvs");
    SS.GW.SnapToView(ViewPreset::FRONT, /*animate=*/false);
    CHECK_TRUE(Near(SS.GW.projRight, Vector::From(1, 0, 0)));
    CHECK_TRUE(Near(SS.GW.projUp, Vector::From(0, 0, 1)));
}

TEST_CASE(hover_fallback_selects_only_when_empty) {
    CHECK_LOAD("line.slvs");
    hEntity line = {};
    for(Entity &e : SK.entity) {
        if(e.type == Entity::Type::LINE_SEGMENT) line = e.h;
    }
    CHECK_TRUE(line.v != 0);
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    CHECK_FALSE(SS.GW.SelectHoveredIfNothingSelected()); // nothing hovered
    SS.GW.hover.entity = line;
    CHECK_TRUE(SS.GW.SelectHoveredIfNothingSelected());
    CHECK_TRUE(SS.GW.IsSelected(line));
    CHECK_FALSE(SS.GW.SelectHoveredIfNothingSelected()); // already has a selection
}
