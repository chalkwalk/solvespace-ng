//-----------------------------------------------------------------------------
// Split with automatic joints (AGENTS.md section 7, M5b): a part too big to print is cut by a
// plane into two parts, the second new, with dowel pockets across the cut to line them up.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

bool Group::SplitPlane(Vector *p, Vector *n) const {
    Entity *e = SK.entity.FindByIdNoOops(predef.entityB);
    if(e == nullptr) return false;
    if(e->type == Entity::Type::WORKPLANE) {
        *p = e->WorkplaneGetOffset();
        *n = e->Normal()->NormalN();
    } else if(e->IsFace()) {
        *p = e->FaceGetPointNum();
        *n = e->FaceGetNormalNum();
    } else {
        return false;
    }
    if(n->Magnitude() < LENGTH_EPS) return false;
    *n = n->WithMagnitude(fit.flip ? -1 : 1);
    return true;
}

void SolveSpaceUI::MakeSplit() {
    Entity *plane = nullptr;
    if(GW.selection.n == 1 && GW.selection[0].entity.v) {
        plane = SK.entity.FindByIdNoOops(GW.selection[0].entity);
    }
    if(plane == nullptr ||
       !(plane->type == Entity::Type::WORKPLANE || plane->IsFace())) {
        Error(_("Select the plane to split the part by: a workplane or a flat face."));
        return;
    }
    Group *active = SK.GetGroup(GW.activeGroup);
    Group *src    = active->IsMeshGroup() ? active : active->RunningMeshGroup();
    if(src == nullptr || !src->runningShape) {
        Error(_("Activate a group of the part to split: its solid is what is split."));
        return;
    }
    if(!Kernel().Supports(KernelFeature::SPLIT)) {
        Error(_("Splitting needs the Open CASCADE kernel, which this build lacks."));
        return;
    }
    hEntity planeEntity = plane->h;
    GW.ClearSelection();
    UndoRemember();
    if(SK.part.IsEmpty()) {
        Part first = {};
        first.h    = Part::FIRST;
        first.name = "Part 1";
        SK.part.Add(&first);
    }
    Part next = {};
    next.name = ssprintf("Part %d", SK.part.n + 1);
    SK.part.AddAndAssignId(&next);

    int base = active->order;
    for(Group &g : SK.group) {
        if(g.order > base) g.order += 2;
    }
    hGroup made[2];
    for(int i = 0; i < 2; i++) {
        Group g          = {};
        g.visible        = true;
        g.scale          = 1;
        g.type           = Group::Type::SPLIT;
        g.name           = C_("group-name", "split");
        g.order          = base + 1 + i;
        g.opA            = src->h;
        g.opB            = src->h;
        g.predef.entityB = planeEntity;
        g.part           = i == 0 ? src->part : next.h;
        g.color          = src->color;
        g.valA           = 4;
        g.fit.kind       = FitTable()[0].h; // pressed in
        g.fit.dowel      = 1;
        g.fit.depth      = 10;
        g.fit.female     = i;
        SK.group.AddAndAssignId(&g);
        made[i] = g.h;
    }
    SK.GetGroup(made[0])->fit.partner = made[1];
    SK.GetGroup(made[1])->fit.partner = made[0];
    GW.activeGroup = made[1];
    GenerateAll();
    SK.GetGroup(made[1])->Activate();
    ScheduleShowTW();
}

void SolveSpaceUI::SetSplit(hGroup hg, Group::FitSpec spec, double diameter) {
    Group *g = SK.group.FindByIdNoOops(hg);
    if(g == nullptr || g->type != Group::Type::SPLIT || FindFit(spec.kind) == nullptr) return;
    UndoRemember();
    for(hGroup h : { hg, SK.GetGroup(hg)->fit.partner }) {
        Group *gi = SK.group.FindByIdNoOops(h);
        if(gi == nullptr) continue;
        gi->fit.kind  = spec.kind;
        gi->fit.dowel = spec.dowel;
        gi->fit.flip  = spec.flip;
        gi->fit.depth = std::max(spec.depth, LENGTH_EPS);
        gi->valA      = std::max(diameter, LENGTH_EPS);
        MarkGroupDirty(h);
    }
    unsaved = true;
    GW.Invalidate();
    ScheduleShowTW();
}

} // namespace SolveSpace
