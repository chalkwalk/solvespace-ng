//-----------------------------------------------------------------------------
// The fit table (AGENTS.md section 7, M5b): named clearances, tuned per printer, that fit
// joints take their gaps from. A document that never changed it uses the defaults and saves
// none; the first edit makes them its own.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

static const Fit DEFAULT_FITS[] = {
    { 0, { 1 }, "press", -0.1 },
    { 0, { 2 }, "slide", 0.2 },
    { 0, { 3 }, "loose", 0.4 },
};

std::vector<Fit> SolveSpaceUI::FitTable() {
    if(SK.fit.IsEmpty()) return { std::begin(DEFAULT_FITS), std::end(DEFAULT_FITS) };
    return { SK.fit.begin(), SK.fit.end() };
}

const Fit *SolveSpaceUI::FindFit(hFit hf) {
    if(!SK.fit.IsEmpty()) return SK.fit.FindByIdNoOops(hf);
    for(const Fit &f : DEFAULT_FITS) {
        if(f.h == hf) return &f;
    }
    return nullptr;
}

// An undo step, then the defaults made the document's own.
static void RememberFits() {
    SS.UndoRemember();
    if(!SK.fit.IsEmpty()) return;
    for(Fit f : DEFAULT_FITS) SK.fit.Add(&f);
}

static void FitsChanged() {
    SS.unsaved = true;
    // Fit joints take their clearances from the table.
    for(Group &g : SK.group) {
        if(g.type == Group::Type::FIT) g.clean = false;
    }
    SS.GW.Invalidate();
    SS.ScheduleShowTW();
}

hFit SolveSpaceUI::AddFit() {
    RememberFits();
    std::string name;
    for(int n = 1; name.empty(); n++) {
        name = ssprintf("fit %d", n);
        for(const Fit &f : SK.fit) {
            if(f.name == name) name.clear();
        }
    }
    Fit f       = {};
    f.name      = name;
    f.clearance = 0.2;
    SK.fit.AddAndAssignId(&f);
    FitsChanged();
    return f.h;
}

bool SolveSpaceUI::RenameFit(hFit hf, const std::string &name) {
    if(name.empty() || FindFit(hf) == nullptr) return false;
    for(const Fit &f : FitTable()) {
        if(f.name == name) return f.h == hf;
    }
    RememberFits();
    SK.fit.FindById(hf)->name = name;
    FitsChanged();
    return true;
}

void SolveSpaceUI::SetFitClearance(hFit hf, double mm) {
    const Fit *f = FindFit(hf);
    if(f == nullptr || f->clearance == mm) return;
    RememberFits();
    SK.fit.FindById(hf)->clearance = mm;
    FitsChanged();
}

bool SolveSpaceUI::DeleteFit(hFit hf) {
    if(FindFit(hf) == nullptr || FitTable().size() <= 1) return false;
    for(Group &g : SK.group) {
        if(g.type == Group::Type::FIT && g.fit.kind == hf) {
            Error(_("The fit '%s' is used by the fit joint '%s'."), FindFit(hf)->name.c_str(),
                  g.DescriptionString().c_str());
            return false;
        }
    }
    RememberFits();
    SK.fit.RemoveById(hf);
    FitsChanged();
    return true;
}

void Group::FitExtrusion(Vector *dir, double *s0, double *s1, double *offset) const {
    *dir = {};
    if(Entity *wp = SK.entity.FindByIdNoOops(predef.entityB)) {
        *dir = wp->Normal()->NormalN().ScaledBy(fit.flip ? -1 : 1);
    }
    const Fit *f = SS.FindFit(fit.kind);
    double     c = f ? f->clearance : 0, depth = fit.depth;
    if(!fit.dowel && !fit.female) {
        // The peg, as drawn.
        *s0     = 0;
        *s1     = depth;
        *offset = 0;
        return;
    }
    // A pocket: from a little behind the profile's plane (so its opening is clean) to the
    // depth, deeper by a clearance; a dowel's pocket in the male goes the other way. The
    // offset adds c at both ends, so the extrusion is shortened by it there.
    double behind = fabs(c) + 0.1, deep = (fit.dowel ? depth / 2 : depth) + std::max(c, 0.0);
    double a = -behind, b = deep;
    if(fit.dowel && !fit.female) {
        a = -deep;
        b = behind;
    }
    *s0     = a + c;
    *s1     = b - c;
    *offset = c;
}

void SolveSpaceUI::MakeFitJoint() {
    std::vector<hEntity> picked;
    for(const GraphicsWindow::Selection &s : GW.selection) {
        if(s.entity.v) picked.push_back(s.entity);
    }
    Group  *sketch = SK.GetGroup(GW.activeGroup);
    hEntity wrkpl  = GW.ActiveWorkplane();
    if(!GW.LockedInWorkplane() || sketch->polyError.how != PolyError::GOOD ||
       sketch->bezierLoops.l.IsEmpty()) {
        Error(_("A fit joint is made from the active sketch: a closed profile in a workplane, "
                "drawn where the parts meet. Sketch it, then pick something on the male part "
                "and on the female part."));
        return;
    }
    hPart part[2];
    for(size_t i = 0; i < picked.size() && i < 2; i++) {
        part[i] = SK.GetGroup(SK.GetEntity(picked[i])->group)->PartOf();
    }
    if(picked.size() != 2 || part[0] == part[1]) {
        Error(_("Pick something on the male part, then on the female part: the peg goes on "
                "the first, the pocket in the second."));
        return;
    }
    if(!Kernel().Supports(KernelFeature::FIT)) {
        Error(_("Fit joints need the Open CASCADE kernel, which this build lacks."));
        return;
    }
    // Towards the female part's solid.
    Group::FitSpec spec;
    spec.kind  = FitTable().size() > 1 ? FitTable()[1].h : FitTable()[0].h;
    spec.depth = 5;
    Vector n   = SK.GetEntity(wrkpl)->Normal()->NormalN(),
           o   = SK.GetEntity(SK.GetEntity(wrkpl)->point[0])->PointGetNum();
    if(Group *solid = PartSolid(part[1])) {
        Vector lo, hi;
        solid->RunningMesh().GetBounding(&hi, &lo);
        if(lo.Plus(hi).ScaledBy(0.5).Minus(o).Dot(n) < 0) spec.flip = 1;
    }

    GW.ClearSelection();
    UndoRemember();
    int base = sketch->order;
    for(Group &g : SK.group) {
        if(g.order > base) g.order += 2;
    }
    hGroup made[2];
    for(int i = 0; i < 2; i++) {
        Group g          = {};
        g.visible        = true;
        g.scale          = 1;
        g.type           = Group::Type::FIT;
        g.name           = i == 0 ? C_("group-name", "fit-peg") : C_("group-name", "fit-pocket");
        g.order          = base + 1 + i;
        g.opA            = sketch->h;
        g.predef.entityB = wrkpl;
        g.part           = SK.GetGroup(SK.GetEntity(picked[i])->group)->part;
        g.color          = RGBi(100, 100, 100);
        if(Group *solid = PartSolid(part[i])) g.color = solid->color;
        g.fit            = spec;
        g.fit.female     = i;
        g.meshCombine    = i == 0 ? Group::CombineAs::UNION : Group::CombineAs::DIFFERENCE;
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

void SolveSpaceUI::SetFitJoint(hGroup hg, Group::FitSpec spec) {
    Group *g = SK.group.FindByIdNoOops(hg);
    if(g == nullptr || g->type != Group::Type::FIT || FindFit(spec.kind) == nullptr) return;
    spec.depth = std::max(spec.depth, LENGTH_EPS);
    UndoRemember();
    for(hGroup h : { hg, SK.GetGroup(hg)->fit.partner }) {
        Group *gi = SK.group.FindByIdNoOops(h);
        if(gi == nullptr) continue;
        gi->fit.kind  = spec.kind;
        gi->fit.dowel = spec.dowel;
        gi->fit.flip  = spec.flip;
        gi->fit.depth = spec.depth;
        gi->meshCombine = !gi->fit.female && !gi->fit.dowel ? Group::CombineAs::UNION
                                                            : Group::CombineAs::DIFFERENCE;
        MarkGroupDirty(h);
    }
    unsaved = true;
    GW.Invalidate();
    ScheduleShowTW();
}

} // namespace SolveSpace
