//-----------------------------------------------------------------------------
// Implementation of the Group class, which represents a set of entities and
// constraints that are solved together, in some cases followed by another
// operation, like to extrude surfaces from the entities or to step and
// repeat them parametrically.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

const hParam   Param::NO_PARAM = { 0 };
#define NO_PARAM (Param::NO_PARAM)

const hGroup Group::HGROUP_REFERENCES = { 1 };

//-----------------------------------------------------------------------------
// The group structure includes pointers to other dynamically-allocated
// memory. This clears and frees them all.
//-----------------------------------------------------------------------------
void Group::Clear() {
    solved.remove.Clear();
    polyLoops.Clear();
    bezierLoops.Clear();
    bezierOpens.Clear();
    thisMesh.Clear();
    runningMesh.Clear();
    thisShell.Clear();
    runningShell.Clear();
    thisShape.reset();
    runningShape.reset();
    runningTessellated = false;
    displayMesh.Clear();
    displayOutlines.Clear();
    impMesh.Clear();
    impShell.Clear();
    impShape.reset();
    impEntity.Clear();
    // remap is the only one that doesn't get recreated when we regen
    remap.clear();
}

void Group::AddParam(ParamList *param, hParam hp, double v) {
    Param pa = {};
    pa.h = hp;
    pa.val = v;

    param->Add(&pa);
}

bool Group::IsVisible() {
    if(!visible) return false;
    Group *active = SK.GetGroup(SS.GW.activeGroup);
    if(order > active->order) return false;
    return true;
}

size_t Group::GetNumConstraints() {
    return std::count_if(SK.constraint.begin(), SK.constraint.end(),
                         [&](Constraint const &c) { return c.group == h; });
}

Vector Group::ExtrusionGetVector() {
    return Vector::From(h.param(0), h.param(1), h.param(2));
}

void Group::ExtrusionForceVectorTo(const Vector &v) {
    SK.GetParam(h.param(0))->val = v.x;
    SK.GetParam(h.param(1))->val = v.y;
    SK.GetParam(h.param(2))->val = v.z;
}

bool Group::IsEdgeEntity(const Entity *e) {
    if(e->construction) return false;
    switch(e->type) {
        case Entity::Type::LINE_SEGMENT:
        case Entity::Type::ARC_OF_CIRCLE:
        case Entity::Type::CIRCLE:
        case Entity::Type::CUBIC:
        case Entity::Type::CUBIC_PERIODIC: break;
        default: return false;
    }
    Group *g = SK.group.FindByIdNoOops(e->group);
    if(g == nullptr) return false;
    switch(g->type) {
        case Type::EXTRUDE:
        case Type::LATHE:
        case Type::REVOLVE:
        case Type::HELIX:
        case Type::TRANSLATE:
        case Type::ROTATE: return true;
        default: return false;
    }
}

bool Group::IsSolidFace(const Entity *e) {
    if(!e->IsFace()) return false;
    Group *g = SK.group.FindByIdNoOops(e->group);
    return g != nullptr && g->IsMeshGroup();
}

// The length of an edge, from its curves.
static double EdgeLength(Entity *e) {
    SBezierList sbl = {};
    e->GenerateBezierCurves(&sbl);
    double length = 0;
    for(const SBezier &sb : sbl.l) {
        Vector prev = sb.Start();
        for(int i = 1; i <= 16; i++) {
            Vector p = sb.PointAt(i / 16.0);
            length += p.Minus(prev).Magnitude();
            prev = p;
        }
    }
    sbl.Clear();
    return length;
}

void Group::MenuGroup(Command id)  {
    MenuGroup(id, Platform::Path());
}

void Group::MenuGroup(Command id, Platform::Path linkFile) {
    Platform::SettingsRef settings = Platform::GetSettings();

    Group g = {};
    g.visible = true;
    g.color = RGBi(100, 100, 100);
    g.scale = 1;
    g.linkFile = linkFile;

    SS.GW.GroupSelection();
    auto const &gs = SS.GW.gs;

    switch(id) {
        case Command::GROUP_3D:
            g.type = Type::DRAWING_3D;
            g.name = C_("group-name", "sketch-in-3d");
            break;

        case Command::GROUP_WRKPL:
            g.type = Type::DRAWING_WORKPLANE;
            g.name = C_("group-name", "sketch-in-plane");
            if(gs.points == 1 && gs.n == 1) {
                g.subtype = Subtype::WORKPLANE_BY_POINT_ORTHO;

                Vector u = SS.GW.projRight, v = SS.GW.projUp;
                u = u.ClosestOrtho();
                v = v.Minus(u.ScaledBy(v.Dot(u)));
                v = v.ClosestOrtho();

                g.predef.q = Quaternion::From(u, v);
                g.predef.origin = gs.point[0];
            } else if(gs.points == 1 && gs.lineSegments == 2 && gs.n == 3) {
                g.subtype = Subtype::WORKPLANE_BY_LINE_SEGMENTS;

                g.predef.origin = gs.point[0];
                g.predef.entityB = gs.entity[0];
                g.predef.entityC = gs.entity[1];

                Vector ut = SK.GetEntity(g.predef.entityB)->VectorGetNum();
                Vector vt = SK.GetEntity(g.predef.entityC)->VectorGetNum();
                ut = ut.WithMagnitude(1);
                vt = vt.WithMagnitude(1);

                if(fabs(SS.GW.projUp.Dot(vt)) < fabs(SS.GW.projUp.Dot(ut))) {
                    swap(ut, vt);
                    g.predef.swapUV = true;
                }
                if(SS.GW.projRight.Dot(ut) < 0) g.predef.negateU = true;
                if(SS.GW.projUp.   Dot(vt) < 0) g.predef.negateV = true;
            } else if(gs.workplanes == 1 && gs.n == 1) {
                if(gs.entity[0].isFromRequest()) {
                    Entity *wrkpl = SK.GetEntity(gs.entity[0]);
                    Entity *normal = SK.GetEntity(wrkpl->normal);
                    g.subtype = Subtype::WORKPLANE_BY_POINT_ORTHO;
                    g.predef.origin = wrkpl->point[0];
                    g.predef.q = normal->NormalGetNum();
                } else {
                    Group *wrkplg = SK.GetGroup(gs.entity[0].group());
                    g.subtype = wrkplg->subtype;
                    g.predef.origin = wrkplg->predef.origin;
                    if(wrkplg->subtype == Subtype::WORKPLANE_BY_LINE_SEGMENTS) {
                        g.predef.entityB = wrkplg->predef.entityB;
                        g.predef.entityC = wrkplg->predef.entityC;
                        g.predef.swapUV = wrkplg->predef.swapUV;
                        g.predef.negateU = wrkplg->predef.negateU;
                        g.predef.negateV = wrkplg->predef.negateV;
                    } else if(wrkplg->subtype == Subtype::WORKPLANE_BY_POINT_ORTHO) {
                        g.predef.q = wrkplg->predef.q;
                    } else if(wrkplg->subtype == Subtype::WORKPLANE_BY_POINT_NORMAL) {
                        g.predef.q = wrkplg->predef.q;
                        g.predef.entityB = wrkplg->predef.entityB;
                    } else ssassert(false, "Unexpected workplane subtype");
                }
            } else if(gs.anyNormals == 1 && gs.points == 1 && gs.n == 2) {
                g.subtype       = Subtype::WORKPLANE_BY_POINT_NORMAL;
                g.predef.entityB = gs.anyNormal[0];
                g.predef.q      = SK.GetEntity(gs.anyNormal[0])->NormalGetNum();
                g.predef.origin = gs.point[0];
            //} else if(gs.faces == 1 && gs.points == 1 && gs.n == 2) {
            //    g.subtype = Subtype::WORKPLANE_BY_POINT_FACE;
            //    g.predef.q      = SK.GetEntity(gs.face[0])->NormalGetNum();
            //    g.predef.origin = gs.point[0];
            } else {
                Error(_("Bad selection for new sketch in workplane. This "
                        "group can be created with:\n\n"
                        "    * a point (through the point, orthogonal to coordinate axes)\n"
                        "    * a point and two line segments (through the point, "
                        "parallel to the lines)\n"
                        "    * a point and a normal (through the point, "
                        "orthogonal to the normal)\n"
                        /*"    * a point and a face (through the point, "
                        "parallel to the face)\n"*/
                        "    * a workplane (copy of the workplane)\n"));
                return;
            }
            break;

        case Command::GROUP_EXTRUDE:
            if(!SS.GW.LockedInWorkplane()) {
                Error(_("Activate a workplane (Sketch -> In Workplane) before "
                        "extruding. The sketch will be extruded normal to the "
                        "workplane."));
                return;
            }
            g.type = Type::EXTRUDE;
            g.opA = SS.GW.activeGroup;
            g.predef.entityB = SS.GW.ActiveWorkplane();
            g.subtype = Subtype::ONE_SIDED;
            g.name = C_("group-name", "extrude");
            break;

        case Command::GROUP_LATHE:
            if(!SS.GW.LockedInWorkplane()) {
                Error(_("Lathe operation can only be applied to planar sketches."));
                return;
            }
            if(gs.points == 1 && gs.vectors == 1 && gs.n == 2) {
                g.predef.origin = gs.point[0];
                g.predef.entityB = gs.vector[0];
            } else if(gs.lineSegments == 1 && gs.n == 1) {
                g.predef.origin = SK.GetEntity(gs.entity[0])->point[0];
                g.predef.entityB = gs.entity[0];
                // since a line segment is a vector
            } else {
                Error(_("Bad selection for new lathe group. This group can "
                        "be created with:\n\n"
                        "    * a point and a line segment or normal "
                                 "(revolved about an axis parallel to line / "
                                 "normal, through point)\n"
                        "    * a line segment (revolved about line segment)\n"));
                return;
            }
            g.type = Type::LATHE;
            g.opA = SS.GW.activeGroup;
            g.name = C_("group-name", "lathe");
            break;

        case Command::GROUP_REVOLVE:
            if(!SS.GW.LockedInWorkplane()) {
                Error(_("Revolve operation can only be applied to planar sketches."));
                return;
            }
            if(gs.points == 1 && gs.vectors == 1 && gs.n == 2) {
                g.predef.origin  = gs.point[0];
                g.predef.entityB = gs.vector[0];
            } else if(gs.lineSegments == 1 && gs.n == 1) {
                g.predef.origin  = SK.GetEntity(gs.entity[0])->point[0];
                g.predef.entityB = gs.entity[0];
                // since a line segment is a vector
            } else {
                Error(_("Bad selection for new revolve group. This group can "
                        "be created with:\n\n"
                        "    * a point and a line segment or normal "
                                 "(revolved about an axis parallel to line / "
                                 "normal, through point)\n"
                        "    * a line segment (revolved about line segment)\n"));
                return;
            }
            g.type    = Type::REVOLVE;
            g.opA     = SS.GW.activeGroup;
            g.valA    = 2;
            g.subtype = Subtype::ONE_SIDED;
            g.name    = C_("group-name", "revolve");
            break;

        case Command::GROUP_HELIX:
            if(!SS.GW.LockedInWorkplane()) {
                Error(_("Helix operation can only be applied to planar sketches."));
                return;
            }
            if(gs.points == 1 && gs.vectors == 1 && gs.n == 2) {
                g.predef.origin  = gs.point[0];
                g.predef.entityB = gs.vector[0];
            } else if(gs.lineSegments == 1 && gs.n == 1) {
                g.predef.origin  = SK.GetEntity(gs.entity[0])->point[0];
                g.predef.entityB = gs.entity[0];
                // since a line segment is a vector
            } else {
                Error(_("Bad selection for new helix group. This group can "
                        "be created with:\n\n"
                        "    * a point and a line segment or normal "
                                 "(revolved about an axis parallel to line / "
                                 "normal, through point)\n"
                        "    * a line segment (revolved about line segment)\n"));
                return;
            }
            g.type    = Type::HELIX;
            g.opA     = SS.GW.activeGroup;
            g.valA    = 2;
            g.subtype = Subtype::ONE_SIDED;
            g.name    = C_("group-name", "helix");
            break;

        case Command::GROUP_ROT: {
            if(gs.points == 1 && gs.n == 1 && SS.GW.LockedInWorkplane()) {
                g.predef.origin = gs.point[0];
                Entity *w = SK.GetEntity(SS.GW.ActiveWorkplane());
                g.predef.entityB = w->Normal()->h;
                g.activeWorkplane = w->h;
            } else if(gs.points == 1 && gs.vectors == 1 && gs.n == 2) {
                g.predef.origin = gs.point[0];
                g.predef.entityB = gs.vector[0];
            } else {
                Error(_("Bad selection for new rotation. This group can "
                        "be created with:\n\n"
                        "    * a point, while locked in workplane (rotate "
                              "in plane, about that point)\n"
                        "    * a point and a line or a normal (rotate about "
                              "an axis through the point, and parallel to "
                              "line / normal)\n"));
                return;
            }
            g.type = Type::ROTATE;
            g.opA = SS.GW.activeGroup;
            g.valA = 3;
            g.subtype = Subtype::ONE_SIDED;
            g.name = C_("group-name", "rotate");
            break;
        }

        case Command::GROUP_TRANS:
            g.type = Type::TRANSLATE;
            g.opA = SS.GW.activeGroup;
            g.valA = 3;
            g.subtype = Subtype::ONE_SIDED;
            g.predef.entityB = SS.GW.ActiveWorkplane();
            g.activeWorkplane = SS.GW.ActiveWorkplane();
            g.name = C_("group-name", "translate");
            break;

        case Command::GROUP_LINK: {
            g.type = Type::LINKED;
            g.meshCombine = CombineAs::ASSEMBLE;
            if(g.linkFile.IsEmpty()) {
                Platform::FileDialogRef dialog = Platform::CreateOpenFileDialog(SS.GW.window);
                dialog->AddFilters(Platform::SolveSpaceLinkFileFilters);
                dialog->ThawChoices(settings, "LinkSketch");
                if(!dialog->RunModal()) return;
                dialog->FreezeChoices(settings, "LinkSketch");
                g.linkFile = dialog->GetFilename();
            }

            // Assign the default name of the group based on the name of
            // the linked file.
            g.name = g.linkFile.FileStem();
            for(size_t i = 0; i < g.name.length(); i++) {
                if(!(isalnum(g.name[i]) || (unsigned)g.name[i] >= 0x80)) {
                    // convert punctuation to dashes
                    g.name[i] = '-';
                }
            }
            break;
        }

        case Command::GROUP_FILLET:
        case Command::GROUP_CHAMFER: {
            bool   fillet   = id == Command::GROUP_FILLET;
            double shortest = VERY_POSITIVE;
            for(const GraphicsWindow::Selection &s : SS.GW.selection) {
                Entity *e = s.entity.v ? SK.entity.FindByIdNoOops(s.entity) : nullptr;
                if(e == nullptr || !IsEdgeEntity(e)) {
                    g.edges.clear();
                    break;
                }
                g.edges.push_back(e->h);
                shortest = std::min(shortest, EdgeLength(e));
            }
            if(g.edges.empty()) {
                Error(_("Bad selection for a new %s group. Select one or more edges of "
                        "the solid: the lines and curves an extrusion, lathe, revolve, "
                        "helix or pattern makes."), fillet ? _("fillet") : _("chamfer"));
                return;
            }
            if(!Kernel().Supports(fillet ? KernelFeature::FILLET : KernelFeature::CHAMFER)) {
                Error(_("Fillets and chamfers need the Open CASCADE kernel (Preferences, "
                        "SolveSpace NG)."));
                return;
            }
            g.type = fillet ? Type::FILLET : Type::CHAMFER;
            g.valA = shortest / 10;
            g.name = fillet ? C_("group-name", "fillet") : C_("group-name", "chamfer");
            break;
        }

        case Command::GROUP_SHELL: {
            for(const GraphicsWindow::Selection &s : SS.GW.selection) {
                Entity *e = s.entity.v ? SK.entity.FindByIdNoOops(s.entity) : nullptr;
                if(e == nullptr || !IsSolidFace(e)) {
                    Error(_("Bad selection for a new shell group. Select the faces of the "
                            "solid to leave open, or nothing for a closed shell."));
                    return;
                }
                g.faces.push_back(e->h);
            }
            if(!Kernel().Supports(KernelFeature::SHELL)) {
                Error(_("Shells need the Open CASCADE kernel (Preferences, SolveSpace NG)."));
                return;
            }
            // A tenth of the solid's smallest extent.
            Group *running = SK.GetRunningMeshGroupFor(SS.GW.activeGroup);
            if(running == nullptr) {
                Error(_("There is no solid to make a shell of."));
                return;
            }
            SMesh m = {};
            m.MakeFromCopyOf(&running->runningMesh);
            running->runningShell.TriangulateInto(&m);
            Vector lo = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);
            Vector hi = lo.ScaledBy(-1);
            for(const STriangle &t : m.l) {
                for(const Vector &v : t.vertices) {
                    lo = Vector::From(std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z));
                    hi = Vector::From(std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z));
                }
            }
            m.Clear();
            Vector size = hi.Minus(lo);
            g.valA = std::max(LENGTH_EPS, std::min({ size.x, size.y, size.z }) / 10);
            g.type = Type::SHELL;
            g.name = C_("group-name", "shell");
            break;
        }

        case Command::GROUP_DRAFT: {
            for(const GraphicsWindow::Selection &s : SS.GW.selection) {
                Entity *e = s.entity.v ? SK.entity.FindByIdNoOops(s.entity) : nullptr;
                Group  *eg = e ? SK.group.FindByIdNoOops(e->group) : nullptr;
                if(e == nullptr || e->type != Entity::Type::FACE_XPROD || eg == nullptr ||
                   eg->type != Type::EXTRUDE) {
                    g.faces.clear();
                    break;
                }
                g.faces.push_back(e->h);
            }
            if(g.faces.empty()) {
                Error(_("Bad selection for a new draft group. Select side faces of an "
                        "extrusion; they taper about its sketch plane, along its direction."));
                return;
            }
            if(!Kernel().Supports(KernelFeature::DRAFT)) {
                Error(_("Drafts need the Open CASCADE kernel (Preferences, SolveSpace NG)."));
                return;
            }
            g.valA = 2;
            g.type = Type::DRAFT;
            g.name = C_("group-name", "draft");
            break;
        }

        case Command::GROUP_HOLE: {
            for(const GraphicsWindow::Selection &s : SS.GW.selection) {
                Entity *e = s.entity.v ? SK.entity.FindByIdNoOops(s.entity) : nullptr;
                if(e == nullptr || !e->IsPoint() || e->workplane == Entity::FREE_IN_3D) {
                    g.points.clear();
                    break;
                }
                g.points.push_back(e->h);
            }
            if(g.points.empty()) {
                Error(_("Bad selection for a new hole group. Select the hole centres: points "
                        "in a workplane, such as a sketch on a face. The holes go into the "
                        "face, against the workplane's normal."));
                return;
            }
            if(!Kernel().Supports(KernelFeature::HOLE)) {
                Error(_("Holes need the Open CASCADE kernel (Preferences, SolveSpace NG)."));
                return;
            }
            g.hole.diameter     = 5;
            g.hole.headDiameter = 10;
            g.hole.headDepth    = 3;
            g.hole.headAngle    = 90;
            g.type              = Type::HOLE;
            g.name              = C_("group-name", "hole");
            break;
        }

        case Command::GROUP_LOFT: {
            std::vector<hGroup> sketches;
            for(const GraphicsWindow::Selection &s : SS.GW.selection) {
                Profile p;
                Entity *e = s.entity.v ? SK.entity.FindByIdNoOops(s.entity) : nullptr;
                if(e == nullptr || !ProfileOf(e->h, &p) ||
                   std::find(sketches.begin(), sketches.end(), e->group) != sketches.end()) {
                    g.profiles.clear();
                    break;
                }
                sketches.push_back(e->group);
                g.profiles.push_back(e->h);
            }
            if(g.profiles.size() < 2) {
                Error(_("Bad selection for a new loft group. Select a curve of each profile, "
                        "in order: two or more closed shapes, each in its own sketch."));
                return;
            }
            if(!Kernel().Supports(KernelFeature::LOFT)) {
                Error(_("Lofts need the Open CASCADE kernel (Preferences, SolveSpace NG)."));
                return;
            }
            g.type = Type::LOFT;
            g.name = C_("group-name", "loft");
            break;
        }

        case Command::GROUP_SWEEP: {
            std::vector<hEntity> picked;
            for(const GraphicsWindow::Selection &s : SS.GW.selection) {
                if(s.entity.v) picked.push_back(s.entity);
            }
            Profile              profile;
            std::vector<SBezier> path;
            bool good = picked.size() >= 2 && picked.size() == (size_t)SS.GW.selection.n &&
                        ProfileOf(picked[0], &profile);
            std::vector<hEntity> curves(picked.begin() + (picked.empty() ? 0 : 1), picked.end());
            for(hEntity he : curves) {
                Entity *e = SK.entity.FindByIdNoOops(he);
                good = good && e && e->group != SK.GetEntity(picked[0])->group &&
                       (e->type == Entity::Type::LINE_SEGMENT ||
                        e->type == Entity::Type::ARC_OF_CIRCLE ||
                        e->type == Entity::Type::CUBIC);
            }
            if(!good || !PathOf(curves, profile.centre, &path)) {
                Error(_("Bad selection for a new sweep group. Select a curve of the profile (a "
                        "closed shape in a sketch) first, then the path's curves, connected "
                        "end to end, in another sketch."));
                return;
            }
            if(!Kernel().Supports(KernelFeature::SWEEP)) {
                Error(_("Sweeps need the Open CASCADE kernel (Preferences, SolveSpace NG)."));
                return;
            }
            g.profiles = picked;
            g.type     = Type::SWEEP;
            g.name     = C_("group-name", "sweep");
            break;
        }

        default: ssassert(false, "Unexpected menu ID");
    }

    // Copy color from the previous mesh-contributing group.
    if(g.IsMeshGroup() && !SK.groupOrder.IsEmpty()) {
        Group *running = SK.GetRunningMeshGroupFor(SS.GW.activeGroup);
        if(running != NULL) {
            g.color = running->color;
        }
    }

    SS.GW.ClearSelection();
    SS.UndoRemember();

    bool afterActive = false;
    for(hGroup hg : SK.groupOrder) {
        Group *gi = SK.GetGroup(hg);
        if(afterActive)
            gi->order += 1;
        if(gi->h == SS.GW.activeGroup) {
            g.order = gi->order + 1;
            afterActive = true;
        }
    }

    SK.group.AddAndAssignId(&g);
    Group *gg = SK.GetGroup(g.h);

    if(gg->type == Type::LINKED) {
        SS.ReloadAllLinked(SS.saveFile);
    }
    gg->clean = false;
    SS.GW.activeGroup = gg->h;
    SS.GenerateAll();
    if(gg->type == Type::DRAWING_WORKPLANE) {
        // Can't set the active workplane for this one until after we've
        // regenerated, because the workplane doesn't exist until then.
        gg->activeWorkplane = gg->h.entity(0);
    }
    gg->Activate();
    TextWindow::ScreenSelectGroup(0, gg->h.v);
    SS.GW.AnimateOntoWorkplane();
}

void Group::TransformImportedBy(Vector t, Quaternion q) {
    ssassert(type == Type::LINKED, "Expected a linked group");

    hParam tx, ty, tz, qw, qx, qy, qz;
    tx = h.param(0);
    ty = h.param(1);
    tz = h.param(2);
    qw = h.param(3);
    qx = h.param(4);
    qy = h.param(5);
    qz = h.param(6);

    Quaternion qg = Quaternion::From(qw, qx, qy, qz);
    qg = q.Times(qg);

    Vector tg = Vector::From(tx, ty, tz);
    tg = tg.Plus(t);

    SK.GetParam(tx)->val = tg.x;
    SK.GetParam(ty)->val = tg.y;
    SK.GetParam(tz)->val = tg.z;

    SK.GetParam(qw)->val = qg.w;
    SK.GetParam(qx)->val = qg.vx;
    SK.GetParam(qy)->val = qg.vy;
    SK.GetParam(qz)->val = qg.vz;
}

bool Group::IsForcedToMeshBySource() const {
    const Group *srcg = this;
    if(type == Type::TRANSLATE || type == Type::ROTATE) {
        // A step and repeat gets merged against the group's previous group,
        // not our own previous group.
        srcg = SK.GetGroup(opA);
        if(srcg->forceToMesh) return true;
    }
    Group *g = srcg->RunningMeshGroup();
    if(g == NULL) return false;
    return g->forceToMesh || g->IsForcedToMeshBySource();
}

bool Group::IsForcedToMesh() const {
    return forceToMesh || IsTriangleMeshAssembly() || IsForcedToMeshBySource();
}

bool Group::IsTriangleMeshAssembly() const {
    if (type != Type::LINKED) return false;
    // A linked STEP file is a B-rep to the OCCT kernel, a mesh to SolveSpace's.
    if(impShape && SS.kernelKind == KernelKind::OCCT) return false;
    if (!impMesh.IsEmpty() && impShell.IsEmpty()) return true;
    return false;
}

std::string Group::DescriptionString() {
    if(name.empty()) {
        return ssprintf("g%03x-%s", h.v, _("(unnamed)"));
    } else {
        return ssprintf("g%03x-%s", h.v, name.c_str());
    }
}

void Group::Activate() {
    if(type == Type::DRAWING_WORKPLANE || type == Type::DRAWING_3D) {
        SS.GW.showFaces = SS.GW.showFacesDrawing;
    } else {
        SS.GW.showFaces = SS.GW.showFacesNonDrawing;
    }
    SS.MarkGroupDirty(h); // for good measure; shouldn't be needed
    SS.ScheduleShowTW();
}

void Group::Generate(EntityList *entity, ParamList *param)
{
    Vector gn = (SS.GW.projRight).Cross(SS.GW.projUp);
    Vector gp = SS.GW.projRight.Plus(SS.GW.projUp);
    Vector gc = (SS.GW.offset).ScaledBy(-1);
    gn = gn.WithMagnitude(200/SS.GW.scale);
    gp = gp.WithMagnitude(200/SS.GW.scale);
    int a, i;
    switch(type) {
        case Type::DRAWING_3D:
            return;

        case Type::DRAWING_WORKPLANE: {
            Quaternion q;
            if(subtype == Subtype::WORKPLANE_BY_LINE_SEGMENTS) {
                Vector u = SK.GetEntity(predef.entityB)->VectorGetNum();
                Vector v = SK.GetEntity(predef.entityC)->VectorGetNum();
                u = u.WithMagnitude(1);
                Vector n = u.Cross(v);
                v = (n.Cross(u)).WithMagnitude(1);

                if(predef.swapUV) swap(u, v);
                if(predef.negateU) u = u.ScaledBy(-1);
                if(predef.negateV) v = v.ScaledBy(-1);
                q = Quaternion::From(u, v);
            } else if(subtype == Subtype::WORKPLANE_BY_POINT_ORTHO) {
                // Already given, numerically.
                q = predef.q;
            } else if(subtype == Subtype::WORKPLANE_BY_POINT_NORMAL) {
                q = SK.GetEntity(predef.entityB)->NormalGetNum();
            } else ssassert(false, "Unexpected workplane subtype");

            Entity normal = {};
            normal.type = Entity::Type::NORMAL_N_COPY;
            normal.numNormal = q;

            normal.point[0] = h.entity(2);
            normal.group = h;
            normal.h = h.entity(1);
            entity->Add(&normal);

            Entity point = {};
            point.type = Entity::Type::POINT_N_COPY;
            point.numPoint = SK.GetEntity(predef.origin)->PointGetNum();
            point.construction = true;
            point.group = h;
            point.h = h.entity(2);
            entity->Add(&point);

            Entity wp = {};
            wp.type = Entity::Type::WORKPLANE;
            wp.normal = normal.h;
            wp.point[0] = point.h;
            wp.group = h;
            wp.h = h.entity(0);
            entity->Add(&wp);
            return;
        }

        case Type::EXTRUDE: {
            AddParam(param, h.param(0), gn.x);
            AddParam(param, h.param(1), gn.y);
            AddParam(param, h.param(2), gn.z);
            int ai, af;
            if((subtype == Subtype::ONE_SIDED) || (subtype == Subtype::ONE_SKEWED)) {
                ai = 0; af = 2;
            } else if((subtype == Subtype::TWO_SIDED) || (subtype == Subtype::TWO_SKEWED)) {
                ai = -1; af = 1;
            } else ssassert(false, "Unexpected extrusion subtype");

            // Get some arbitrary point in the sketch, that will be used
            // as a reference when defining top and bottom faces.
            hEntity pt = { 0 };
            // Not using range-for here because we're changing the size of entity in the loop.
            for(i = 0; i < entity->n; i++) {
                Entity *e = &(entity->Get(i));
                if(e->group != opA) continue;

                if(e->IsPoint()) pt = e->h;

                e->CalculateNumerical(/*forExport=*/false);
                hEntity he = e->h; e = NULL;
                // As soon as I call CopyEntity, e may become invalid! That
                // adds entities, which may cause a realloc.
                CopyEntity(entity, SK.GetEntity(he), ai, REMAP_BOTTOM,
                    h.param(0), h.param(1), h.param(2),
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    CopyAs::N_TRANS);
                CopyEntity(entity, SK.GetEntity(he), af, REMAP_TOP,
                    h.param(0), h.param(1), h.param(2),
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    CopyAs::N_TRANS);
                MakeExtrusionLines(entity, he);
            }
            // Remapped versions of that arbitrary point will be used to
            // provide points on the plane faces.
            MakeExtrusionTopBottomFaces(entity, pt);
            return;
        }

        case Type::LATHE: {
            Vector axis_pos = SK.GetEntity(predef.origin)->PointGetNum();
            Vector axis_dir = SK.GetEntity(predef.entityB)->VectorGetNum();

            // Not using range-for here because we're changing the size of entity in the loop.
            for(i = 0; i < entity->n; i++) {
                Entity *e = &(entity->Get(i));
                if(e->group != opA) continue;

                e->CalculateNumerical(/*forExport=*/false);
                hEntity he = e->h;

                // As soon as I call CopyEntity, e may become invalid! That
                // adds entities, which may cause a realloc.

                // this is the regular copy of all entities
                CopyEntity(entity, SK.GetEntity(he), 0, REMAP_LATHE_START,
                    NO_PARAM, NO_PARAM, NO_PARAM,
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    CopyAs::NUMERIC);

                e = &(entity->Get(i)); // because we copied.
                if (e->IsPoint()) {
                // for points this copy is used for the circle centers
                    CopyEntity(entity, SK.GetEntity(he), 0, REMAP_LATHE_ARC_CENTER,
                        NO_PARAM, NO_PARAM, NO_PARAM,
                        NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                        CopyAs::NUMERIC);
                    MakeLatheCircles(entity, param, he, axis_pos, axis_dir);
                };
                MakeLatheSurfacesSelectable(entity, he, axis_dir);
            }
            return;
        }

        case Type::REVOLVE: {
            // this was borrowed from LATHE and ROTATE
            Vector axis_pos = SK.GetEntity(predef.origin)->PointGetNum();
            Vector axis_dir = SK.GetEntity(predef.entityB)->VectorGetNum();

            // The center of rotation
            AddParam(param, h.param(0), axis_pos.x);
            AddParam(param, h.param(1), axis_pos.y);
            AddParam(param, h.param(2), axis_pos.z);
            // The rotation quaternion
            AddParam(param, h.param(3), 30 * PI / 180);
            AddParam(param, h.param(4), axis_dir.x);
            AddParam(param, h.param(5), axis_dir.y);
            AddParam(param, h.param(6), axis_dir.z);

            // Get some arbitrary point in the sketch, that will be used
            // as a reference when defining end faces.
            hEntity pt = { 0 };

            int ai = 0, af = 2;
            if (subtype == Subtype::TWO_SIDED)
            {
                ai = -1;
                af = 1;
            }
            // Not using range-for here because we're changing the size of entity in the loop.
            for(i = 0; i < entity->n; i++) {
                Entity *e = &(entity->Get(i));
                if(e->group != opA)
                    continue;

                if(e->IsPoint()) pt = e->h;

                e->CalculateNumerical(/*forExport=*/false);
                hEntity he = e->h;
                // one copy for each end of the revolved surface
                CopyEntity(entity, e, ai, REMAP_LATHE_START, h.param(0),
                       h.param(1), h.param(2), h.param(3), h.param(4), h.param(5),
                       h.param(6), NO_PARAM, CopyAs::N_ROT_AA);

                e = &(entity->Get(i)); // because we copied.
                CopyEntity(entity, e, af, REMAP_LATHE_END, h.param(0),
                       h.param(1), h.param(2), h.param(3), h.param(4), h.param(5),
                       h.param(6), NO_PARAM, CopyAs::N_ROT_AA);

                // Arcs are not generated for revolve groups, for now, because our current arc
                // entity is not chiral, and dragging a revolve may break the arc by inverting it.
                // MakeLatheCircles(entity, param, he, axis_pos, axis_dir);
                MakeLatheSurfacesSelectable(entity, he, axis_dir);
            }
            MakeRevolveEndFaces(entity, pt, ai, af);
            return;
        }

        case Type::HELIX:   {
            Vector axis_pos = SK.GetEntity(predef.origin)->PointGetNum();
            Vector axis_dir = SK.GetEntity(predef.entityB)->VectorGetNum();

            // The center of rotation
            AddParam(param, h.param(0), axis_pos.x);
            AddParam(param, h.param(1), axis_pos.y);
            AddParam(param, h.param(2), axis_pos.z);
            // The rotation quaternion
            AddParam(param, h.param(3), 30 * PI / 180);
            AddParam(param, h.param(4), axis_dir.x);
            AddParam(param, h.param(5), axis_dir.y);
            AddParam(param, h.param(6), axis_dir.z);
            // distance to translate along the rotation axis
            AddParam(param, h.param(7), 20);

            // Get some arbitrary point in the sketch, that will be used
            // as a reference when defining end faces.
            hEntity pt = { 0 };

            int ai = 0, af = 2;  // initial and final number of transformations
            if (subtype != Subtype::ONE_SIDED)
            {
                ai = -1;
                af = 1;
            }

            // Not using range-for here because we're changing the size of entity in the loop.
            for(i = 0; i < entity->n; i++) {
                Entity *e = &(entity->Get(i));
                if((e->group.v != opA.v) && !(e->h == predef.origin))
                    continue;

                if(e->IsPoint()) pt = e->h;

                e->CalculateNumerical(/*forExport=*/false);

                // one copy for each end of the helix
                CopyEntity(entity, e, ai, REMAP_LATHE_START, h.param(0),
                           h.param(1), h.param(2), h.param(3), h.param(4), h.param(5),
                           h.param(6), h.param(7), CopyAs::N_ROT_AXIS_TRANS);

                e = &(entity->Get(i)); // because we copied.
                CopyEntity(entity, e, af, REMAP_LATHE_END, h.param(0),
                           h.param(1), h.param(2), h.param(3), h.param(4), h.param(5),
                           h.param(6), h.param(7), CopyAs::N_ROT_AXIS_TRANS);

                // For point entities on the axis, create a construction line
                e = &(entity->Get(i));
                if(e->IsPoint()) {
                    Vector check = e->PointGetNum().Minus(axis_pos).Cross(axis_dir);
                    if (check.Dot(check) < LENGTH_EPS) {
                        //! @todo isn't this the same as &(ent[i])?
                        Entity *ep = SK.GetEntity(e->h);
                        Entity en = {};
                        // A point gets extruded to form a line segment
                        en.point[0] = Remap(ep->h, REMAP_LATHE_START);
                        en.point[1] = Remap(ep->h, REMAP_LATHE_END);
                        en.group = h;
                        en.construction = ep->construction;
                        en.style = ep->style;
                        en.h = Remap(ep->h, REMAP_PT_TO_LINE);
                        en.type = Entity::Type::LINE_SEGMENT;
                        entity->Add(&en);
                    }
                }
            }
            MakeRevolveEndFaces(entity, pt, ai, af);
            return;
        }

        case Type::TRANSLATE: {
            // inherit meshCombine from source group
            Group *srcg = SK.GetGroup(opA);
            meshCombine = srcg->meshCombine;
            // The translation vector
            AddParam(param, h.param(0), gp.x);
            AddParam(param, h.param(1), gp.y);
            AddParam(param, h.param(2), gp.z);

            int n = (int)valA, a0 = 0;
            if(subtype == Subtype::ONE_SIDED && skipFirst) {
                a0++; n++;
            }

            for(a = a0; a < n; a++) {
                // Not using range-for here because we're changing the size of entity in the loop.
                for(i = 0; i < entity->n; i++) {
                    Entity *e = &(entity->Get(i));
                    if(e->group != opA) continue;

                    e->CalculateNumerical(/*forExport=*/false);
                    CopyEntity(entity, e,
                        a*2 - (subtype == Subtype::ONE_SIDED ? 0 : (n-1)),
                        (a == (n - 1)) ? REMAP_LAST : a,
                        h.param(0), h.param(1), h.param(2),
                        NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                        CopyAs::N_TRANS);
                }
            }
            return;
        }
        case Type::ROTATE: {
            // inherit meshCombine from source group
            Group *srcg = SK.GetGroup(opA);
            meshCombine = srcg->meshCombine;
            // The center of rotation
            AddParam(param, h.param(0), gc.x);
            AddParam(param, h.param(1), gc.y);
            AddParam(param, h.param(2), gc.z);
            // The rotation quaternion
            AddParam(param, h.param(3), 30*PI/180);
            AddParam(param, h.param(4), gn.x);
            AddParam(param, h.param(5), gn.y);
            AddParam(param, h.param(6), gn.z);

            int n = (int)valA, a0 = 0;
            if(subtype == Subtype::ONE_SIDED && skipFirst) {
                a0++; n++;
            }

            for(a = a0; a < n; a++) {
                // Not using range-for here because we're changing the size of entity in the loop.
                for(i = 0; i < entity->n; i++) {
                    Entity *e = &(entity->Get(i));
                    if(e->group != opA) continue;

                    e->CalculateNumerical(/*forExport=*/false);
                    CopyEntity(entity, e,
                        a*2 - (subtype == Subtype::ONE_SIDED ? 0 : (n-1)),
                        (a == (n - 1)) ? REMAP_LAST : a,
                        h.param(0), h.param(1), h.param(2),
                        h.param(3), h.param(4), h.param(5), h.param(6), NO_PARAM,
                        CopyAs::N_ROT_AA);
                }
            }
            return;
        }
        case Type::FILLET:
        case Type::CHAMFER:
            MakeFeatureLines(entity);
            return;

        case Type::SHELL:
            MakeShellFaces(entity);
            return;

        case Type::DRAFT:
            MakeDraftFaces(entity);
            return;

        case Type::HOLE:
            MakeHoleEntities(entity);
            return;

        case Type::LOFT:
            MakeLoftFaces(entity);
            return;

        case Type::SWEEP:
            MakeSweepFaces(entity);
            return;

        case Type::LINKED:
            // The translation vector
            AddParam(param, h.param(0), gp.x);
            AddParam(param, h.param(1), gp.y);
            AddParam(param, h.param(2), gp.z);
            // The rotation quaternion
            AddParam(param, h.param(3), 1);
            AddParam(param, h.param(4), 0);
            AddParam(param, h.param(5), 0);
            AddParam(param, h.param(6), 0);

            // Not using range-for here because we're changing the size of entity in the loop.
            for(i = 0; i < impEntity.n; i++) {
                Entity *ie = &(impEntity[i]);
                CopyEntity(entity, ie, 0, 0,
                    h.param(0), h.param(1), h.param(2),
                    h.param(3), h.param(4), h.param(5), h.param(6), NO_PARAM,
                    CopyAs::N_ROT_TRANS);
            }
            return;
    }
    ssassert(false, "Unexpected group type");
}

bool Group::IsSolvedOkay() {
    return this->solved.how == SolveResult::OKAY ||
           (this->allowRedundant && this->solved.how == SolveResult::REDUNDANT_OKAY);
}

void Group::AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index) {
    Equation eq;
    eq.e = expr;
    eq.h = h.equation(index);
    l->Add(&eq);
}

void Group::GenerateEquations(IdList<Equation,hEquation> *l) {
    if(type == Type::LINKED) {
        // Normalize the quaternion
        ExprQuaternion q = {
            Expr::From(h.param(3)),
            Expr::From(h.param(4)),
            Expr::From(h.param(5)),
            Expr::From(h.param(6)) };
        AddEq(l, (q.Magnitude())->Minus(Expr::From(1)), 0);
    } else if(type == Type::ROTATE || type == Type::REVOLVE || type == Type::HELIX) {
        // The axis and center of rotation are specified numerically
#define EC(x) (Expr::From(x))
#define EP(x) (Expr::From(h.param(x)))
        ExprVector orig = SK.GetEntity(predef.origin)->PointGetExprs();
        AddEq(l, (orig.x)->Minus(EP(0)), 0);
        AddEq(l, (orig.y)->Minus(EP(1)), 1);
        AddEq(l, (orig.z)->Minus(EP(2)), 2);
        // param 3 is the angle, which is free
        Vector axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis = axis.WithMagnitude(1);
        AddEq(l, (EC(axis.x))->Minus(EP(4)), 3);
        AddEq(l, (EC(axis.y))->Minus(EP(5)), 4);
        AddEq(l, (EC(axis.z))->Minus(EP(6)), 5);
#undef EC
#undef EP
        if(type == Type::HELIX) {
            if(valB != 0.0) {
                AddEq(l, Expr::From(h.param(7))->Times(Expr::From(PI))->
                Minus(Expr::From(h.param(3))->Times(Expr::From(valB))), 6);
            }
        }
    } else if((type == Type::EXTRUDE) && (subtype != Subtype::ONE_SKEWED) &&
              (subtype != Subtype::TWO_SKEWED)) {
        if(predef.entityB != Entity::FREE_IN_3D) {
            // The extrusion path is locked along a line, normal to the
            // specified workplane. Don't constrain for skewed extrusions.
            Entity *w = SK.GetEntity(predef.entityB);
            ExprVector u = w->Normal()->NormalExprsU();
            ExprVector v = w->Normal()->NormalExprsV();
            ExprVector extruden = {
                Expr::From(h.param(0)),
                Expr::From(h.param(1)),
                Expr::From(h.param(2)) };

            AddEq(l, u.Dot(extruden), 0);
            AddEq(l, v.Dot(extruden), 1);
        }
    } else if(type == Type::TRANSLATE) {
        if(predef.entityB != Entity::FREE_IN_3D) {
            Entity *w = SK.GetEntity(predef.entityB);
            ExprVector n = w->Normal()->NormalExprsN();
            ExprVector trans;
            trans = ExprVector::From(h.param(0), h.param(1), h.param(2));

            // The translation vector is parallel to the workplane
            AddEq(l, trans.Dot(n), 0);
        }
    }
}

hEntity Group::Remap(hEntity in, int copyNumber) {
    auto it = remap.find({ in, copyNumber });
    if(it == remap.end()) {
        // Due to the way the remap value is combined with the group handle into a 32-bit
        // handle value to generate an entity handle, the mapped value must fit in a 16-bit
        // variable.
        // This limit can be lifted once the handle values are extended to 64-bit.
        ssassert(remap.size() < (1 << 16) - 1, "Too many entities in group");
        std::tie(it, std::ignore) =
            remap.insert({ { in, copyNumber }, { (uint32_t)remap.size() + 1 } });
    }
    return h.entity(it->second.v);
}

SnapshotFaceNamer Group::FaceSnapshot() const {
    std::unordered_set<uint32_t> existing;
    for(const Entity &e : SK.entity) {
        if(e.group == h) existing.insert(e.h.v);
    }
    return SnapshotFaceNamer(h, remap, std::move(existing));
}

hEntity GroupFaceNamer::Name(hEntity input, int copy) {
    return group->Remap(input, copy);
}

hEntity GroupFaceNamer::ExistingFace(hEntity input, int copy) {
    hEntity face = group->Remap(input, copy);
    return SK.entity.FindByIdNoOops(face) ? face : Entity::NO_ENTITY;
}

void Group::MakeExtrusionLines(EntityList *el, hEntity in) {
    Entity *ep = SK.GetEntity(in);

    Entity en = {};
    if(ep->IsPoint()) {
        // A point gets extruded to form a line segment
        en.point[0] = Remap(ep->h, REMAP_TOP);
        en.point[1] = Remap(ep->h, REMAP_BOTTOM);
        en.group = h;
        en.construction = ep->construction;
        en.style = ep->style;
        en.h = Remap(ep->h, REMAP_PT_TO_LINE);
        en.type = Entity::Type::LINE_SEGMENT;
        el->Add(&en);
    } else if(ep->type == Entity::Type::LINE_SEGMENT) {
        // A line gets extruded to form a plane face; an endpoint of the
        // original line is a point in the plane, and the line is in the plane.
        Vector a = SK.GetEntity(ep->point[0])->PointGetNum();
        Vector b = SK.GetEntity(ep->point[1])->PointGetNum();
        Vector ab = b.Minus(a);

        en.param[0] = h.param(0);
        en.param[1] = h.param(1);
        en.param[2] = h.param(2);
        en.numPoint = a;
        // we need a point somewhere on the face, and it must be from the current group.
        en.point[0] = Remap(ep->point[0], REMAP_TOP);
        en.numNormal = Quaternion::From(0, ab.x, ab.y, ab.z);

        en.group = h;
        en.construction = ep->construction;
        en.style = ep->style;
        en.h = Remap(ep->h, REMAP_LINE_TO_FACE);
        en.type = Entity::Type::FACE_XPROD;
        el->Add(&en);
    }
}

void Group::AddFeatureLine(EntityList *el, hEntity edge, int which, Vector a, Vector b) {
    Entity en = {};
    en.group  = h;
    en.style  = hStyle{ Style::REFERENCE };
    en.type   = Entity::Type::POINT_N_COPY;
    en.h      = Remap(edge, which + 10);
    en.numPoint = a;
    el->Add(&en);
    en.h        = Remap(edge, which + 20);
    en.numPoint = b;
    el->Add(&en);

    en.type     = Entity::Type::LINE_SEGMENT;
    en.numPoint = {};
    en.point[0] = Remap(edge, which + 10);
    en.point[1] = Remap(edge, which + 20);
    en.h        = Remap(edge, which);
    el->Add(&en);
}

void Group::AddFeatureFace(EntityList *el, hEntity face, int which, Vector point, Vector normal) {
    Entity en   = {};
    en.group    = h;
    en.style    = hStyle{ Style::REFERENCE };
    en.type     = Entity::Type::POINT_N_COPY;
    en.h        = Remap(face, which + 10);
    en.numPoint = point;
    el->Add(&en);

    en.type      = Entity::Type::FACE_NORMAL_PT;
    en.numPoint  = {};
    en.numNormal = Quaternion::From(0, normal.x, normal.y, normal.z);
    en.point[0]  = Remap(face, which + 10);
    en.h         = Remap(face, which);
    el->Add(&en);
}

// Which way each face entity of the previous solid faces, from that solid when it is there
// (topology only: the planes come from the entities); a change solves the later groups again.
void Group::UpdateFaceOut() {
    Group *prevg = RunningMeshGroup();
    if(prevg == nullptr) return;
    for(const KernelFaceNormal &fn : KernelFaceNormals(prevg->runningShape)) {
        Entity *f = SK.entity.FindByIdNoOops(hEntity{ fn.face });
        if(f == nullptr || !f->IsFace()) continue;
        double out = f->FaceGetNormalNum().Dot(fn.out) >= 0 ? 1 : -1;
        auto   it  = faceOut.find(fn.face);
        if(it == faceOut.end() || it->second != out) {
            if(SS.derivedTopologyChanged.v == 0) SS.derivedTopologyChanged = h;
            faceOut[fn.face] = out;
        }
    }
}

double Group::ShellInner() const {
    switch(shellSide) {
        case ShellSide::INSIDE:  return -valA;
        case ShellSide::OUTSIDE: return 0;
        case ShellSide::BOTH:    return -valA / 2;
    }
    return -valA;
}

double Group::ShellOuter() const {
    return ShellInner() + valA;
}

// A shell's derived entities (AGENTS.md section 6): for every face entity of the groups before
// it, the wall's inner and outer faces, offset from it by ShellInner() and ShellOuter(). They always exist; with the
// solid's faces not yet known they lie on the outer faces.
void Group::MakeShellFaces(EntityList *el) {
    UpdateFaceOut();
    std::vector<hEntity> outer;
    for(const Entity &e : SK.entity) {
        if(!IsSolidFace(&e)) continue;
        if(!SS.GroupsInOrder(e.group, h)) continue;
        outer.push_back(e.h);
    }
    for(hEntity he : outer) {
        Entity *f      = SK.GetEntity(he);
        Vector  normal = f->FaceGetNormalNum();
        if(normal.Magnitude() < LENGTH_EPS) normal = Vector::From(0, 0, 1); // degenerate
        normal = normal.WithMagnitude(1);
        Vector  point  = f->FaceGetPointNum();
        auto    it     = faceOut.find(he.v);
        double  out    = it == faceOut.end() ? 0 : it->second;
        AddFeatureFace(el, he, REMAP_SHELL_INNER, point.Plus(normal.ScaledBy(out * ShellInner())),
                       normal);
        AddFeatureFace(el, he, REMAP_SHELL_OUTER, point.Plus(normal.ScaledBy(out * ShellOuter())),
                       normal);
    }
}

// The direction an extrusion's side face is drafted along (the extrusion's), unit.
static Vector DraftPull(const Entity *face) {
    Group *eg = SK.GetGroup(face->group);
    return Vector::From(eg->h.param(0), eg->h.param(1), eg->h.param(2)).WithMagnitude(1);
}

// A draft's derived entities (AGENTS.md section 6): each drafted face, through its base line
// (the sketch line, in the neutral sketch plane) with its outward normal turned towards the
// pull by the angle, so the face leans in. Before the solid is known, the normal turns from
// the entity's.
void Group::MakeDraftFaces(EntityList *el) {
    UpdateFaceOut();
    double angle = valA * PI / 180;
    for(hEntity he : faces) {
        Entity *f = SK.entity.FindByIdNoOops(he);
        if(f == nullptr) continue;
        Vector pull   = DraftPull(f);
        Vector normal = f->FaceGetNormalNum().WithMagnitude(1);
        auto   it     = faceOut.find(he.v);
        if(it != faceOut.end()) normal = normal.ScaledBy(it->second);
        Vector drafted = normal.ScaledBy(cos(angle)).Plus(pull.ScaledBy(sin(angle)));
        AddFeatureFace(el, he, REMAP_DRAFT_FACE, f->FaceGetPointNum(), drafted);
    }
}

Vector Group::HoleDirection(const Entity *point) const {
    Vector n = Vector::From(0, 0, 1);
    if(point->workplane != Entity::FREE_IN_3D) {
        n = SK.GetEntity(point->workplane)->Normal()->NormalN();
    }
    return n.WithMagnitude(hole.flip ? 1 : -1);
}

// A hole's derived entities (AGENTS.md section 6), for each centre: the centre on the face and
// at the bottom (a through hole's bottom where the previous solid ends), the axis between
// them, and the seat face, the counterbore's floor or where the countersink meets the bore (at
// the top for a plain hole).
void Group::MakeHoleEntities(EntityList *el) {
    Group *prevg = RunningMeshGroup();
    for(hEntity he : points) {
        Entity *pt = SK.entity.FindByIdNoOops(he);
        if(pt == nullptr) continue;
        Vector p = pt->PointGetNum(), d = HoleDirection(pt);
        double depth = hole.depth;
        if(depth <= 0) {
            depth = hole.Bore();
            if(prevg != nullptr) {
                for(const STriangle &t : prevg->runningMesh.l) {
                    for(const Vector &v : t.vertices) depth = std::max(depth, v.Minus(p).Dot(d));
                }
            }
        }
        Entity en   = {};
        en.group    = h;
        en.style    = hStyle{ Style::REFERENCE };
        en.type     = Entity::Type::POINT_N_COPY;
        en.h        = Remap(he, REMAP_HOLE_TOP);
        en.numPoint = p;
        el->Add(&en);
        en.h        = Remap(he, REMAP_HOLE_BOTTOM);
        en.numPoint = p.Plus(d.ScaledBy(depth));
        el->Add(&en);
        en.type     = Entity::Type::LINE_SEGMENT;
        en.numPoint = {};
        en.point[0] = Remap(he, REMAP_HOLE_TOP);
        en.point[1] = Remap(he, REMAP_HOLE_BOTTOM);
        en.h        = Remap(he, REMAP_HOLE_AXIS);
        el->Add(&en);
        AddFeatureFace(el, he, REMAP_HOLE_SEAT, p.Plus(d.ScaledBy(hole.SeatDepth())),
                       d.ScaledBy(-1));
    }
}

// The area a loop encloses, seen along normal (its outline, sampled).
static double LoopArea(const SBezierLoop &loop, Vector normal) {
    Vector u = normal.Normal(0), v = normal.Normal(1);
    std::vector<Vector> pts;
    for(const SBezier &sb : loop.l) {
        for(int i = 0; i < 8; i++) pts.push_back(sb.PointAt(i / 8.0));
    }
    double area = 0;
    for(size_t i = 0; i < pts.size(); i++) {
        const Vector &a = pts[i], &b = pts[(i + 1) % pts.size()];
        area += a.Dot(u) * b.Dot(v) - b.Dot(u) * a.Dot(v);
    }
    return fabs(area) / 2;
}

bool Group::ProfileOf(hEntity curve, Profile *out) {
    Entity *e = SK.entity.FindByIdNoOops(curve);
    Group  *g = e ? SK.group.FindByIdNoOops(e->group) : nullptr;
    if(g == nullptr || (g->type != Type::DRAWING_3D && g->type != Type::DRAWING_WORKPLANE)) {
        return false;
    }
    // The curve's own pieces, to find it in a loop by where they run (a circle's pieces don't
    // carry their entity).
    SBezierList own = {};
    e->GenerateBezierCurves(&own);
    auto isPiece = [&](const SBezier &sb) {
        if(sb.entity == curve.v) return true;
        for(const SBezier &o : own.l) {
            if(o.Start().Equals(sb.Start()) && o.Finish().Equals(sb.Finish())) return true;
            if(o.Start().Equals(sb.Finish()) && o.Finish().Equals(sb.Start())) return true;
        }
        return false;
    };
    for(const SBezierLoopSet &sbls : g->bezierLoops.l) {
        bool holds = false;
        for(const SBezierLoop &loop : sbls.l) {
            for(const SBezier &sb : loop.l) holds = holds || isPiece(sb);
        }
        if(!holds) continue;
        const SBezierLoop *outer = nullptr;
        for(const SBezierLoop &loop : sbls.l) {
            if(!outer || LoopArea(loop, sbls.normal) > LoopArea(*outer, sbls.normal)) outer = &loop;
        }
        out->loop.assign(outer->l.begin(), outer->l.end());
        out->normal = sbls.normal.WithMagnitude(1);
        out->holes  = sbls.l.n > 1;
        Vector sum  = {};
        for(const SBezier &sb : out->loop) sum = sum.Plus(sb.Start());
        out->centre = sum.ScaledBy(1.0 / out->loop.size());
        own.Clear();
        return !out->loop.empty();
    }
    own.Clear();
    return false;
}

bool Group::PathOf(const std::vector<hEntity> &curves, Vector near, std::vector<SBezier> *out) {
    std::vector<SBezier> pieces;
    for(hEntity he : curves) {
        Entity *e = SK.entity.FindByIdNoOops(he);
        if(e == nullptr) return false;
        SBezierList sbl = {};
        e->GenerateBezierCurves(&sbl);
        pieces.insert(pieces.end(), sbl.l.begin(), sbl.l.end());
        sbl.Clear();
    }
    if(pieces.empty()) return false;
    // Start at the piece end nearest near, then keep taking the piece that continues.
    size_t first = 0;
    bool   flip  = false;
    double best  = VERY_POSITIVE;
    for(size_t i = 0; i < pieces.size(); i++) {
        for(bool f : { false, true }) {
            double d = (f ? pieces[i].Finish() : pieces[i].Start()).Minus(near).Magnitude();
            if(d < best) best = d, first = i, flip = f;
        }
    }
    out->clear();
    std::vector<bool> used(pieces.size(), false);
    SBezier sb = pieces[first];
    if(flip) sb.Reverse();
    used[first] = true;
    out->push_back(sb);
    for(size_t n = 1; n < pieces.size(); n++) {
        Vector at   = out->back().Finish();
        bool   found = false;
        for(size_t i = 0; i < pieces.size() && !found; i++) {
            if(used[i]) continue;
            SBezier next = pieces[i];
            if(next.Finish().Equals(at)) next.Reverse();
            if(!next.Start().Equals(at)) continue;
            used[i] = found = true;
            out->push_back(next);
        }
        if(!found) return false;
    }
    return true;
}

// A sweep's derived entities: its end faces, where the path starts (facing back along it) and
// ends (facing along it).
void Group::MakeSweepFaces(EntityList *el) {
    if(profiles.empty()) return;
    Profile              profile;
    std::vector<SBezier> path;
    std::vector<hEntity> curves(profiles.begin() + 1, profiles.end());
    Vector start = {}, end = {}, back = Vector::From(0, 0, -1), along = Vector::From(0, 0, 1);
    if(ProfileOf(profiles[0], &profile) && PathOf(curves, profile.centre, &path)) {
        const SBezier &a = path.front(), &b = path.back();
        start = a.Start();
        end   = b.Finish();
        back  = a.ctrl[0].Minus(a.ctrl[1]);
        along = b.ctrl[b.deg].Minus(b.ctrl[b.deg - 1]);
    }
    AddFeatureFace(el, profiles[0], REMAP_SWEEP_START, start, back.WithMagnitude(1));
    AddFeatureFace(el, profiles[0], REMAP_SWEEP_END, end, along.WithMagnitude(1));
}

// A loft's derived entities: its end faces, in the first and last profiles' planes, facing out.
void Group::MakeLoftFaces(EntityList *el) {
    if(profiles.size() < 2) return;
    Profile first, second, last, beforeLast;
    bool    known = ProfileOf(profiles[0], &first) && ProfileOf(profiles[1], &second) &&
                 ProfileOf(profiles.back(), &last) &&
                 ProfileOf(profiles[profiles.size() - 2], &beforeLast);
    Vector start = Vector::From(0, 0, -1), end = Vector::From(0, 0, 1);
    if(known) {
        start = first.normal;
        if(start.Dot(second.centre.Minus(first.centre)) > 0) start = start.ScaledBy(-1);
        end = last.normal;
        if(end.Dot(last.centre.Minus(beforeLast.centre)) < 0) end = end.ScaledBy(-1);
    }
    AddFeatureFace(el, profiles.front(), REMAP_LOFT_START, known ? first.centre : Vector{}, start);
    AddFeatureFace(el, profiles.back(), REMAP_LOFT_END, known ? last.centre : Vector{}, end);
}

// A face entity's normal now, or zero if there is none.
static Vector FaceNormalNow(uint32_t face) {
    Entity *f = face ? SK.entity.FindByIdNoOops(hEntity{ face }) : nullptr;
    return (f && f->IsFace()) ? f->FaceGetNormalNum() : Vector::From(0, 0, 0);
}

// The derived entities of a fillet or chamfer (AGENTS.md section 6): along each straight edge
// between two planar faces, a fillet's two tangent lines and its axis, or a chamfer's two
// edge lines. They always exist, so constraints on them survive any regeneration; with no
// faces known (no OCCT solid yet) they lie on the edge.
void Group::MakeFeatureLines(EntityList *el) {
    bool   fillet = type == Type::FILLET;
    Group *prevg  = RunningMeshGroup();
    std::shared_ptr<const KernelShape> shape = prevg ? prevg->runningShape : nullptr;
    edgeFaces.resize(edges.size());
    for(size_t i = 0; i < edges.size(); i++) {
        Entity *e = SK.entity.FindByIdNoOops(edges[i]);
        if(e == nullptr) continue;
        e->roundedBy = h;
        if(e->type != Entity::Type::LINE_SEGMENT) continue;
        Vector a = SK.GetEntity(e->point[0])->PointGetNum();
        Vector b = SK.GetEntity(e->point[1])->PointGetNum();
        Vector t = b.Minus(a).WithMagnitude(1);

        // The faces, from the previous solid when it has this edge; else as last found.
        EdgeFaces     &known = edgeFaces[i];
        KernelEdgeFace found[2];
        if(KernelEdgeFaces(shape, a, b, found) == KernelEdgeFacesResult::FOUND) {
            if(found[1].face != 0 && found[1].face < found[0].face) swap(found[0], found[1]);
            EdgeFaces now;
            now.known = true;
            for(int j = 0; j < 2; j++) {
                now.face[j] = found[j].face;
                now.into[j] = found[j].into;
                Vector d    = FaceNormalNow(found[j].face).Cross(t);
                now.sign[j] = d.Magnitude() < LENGTH_EPS ? 0 : d.Dot(found[j].into) >= 0 ? 1 : -1;
            }
            bool changed = !known.known;
            for(int j = 0; j < 2; j++) {
                changed = changed || known.face[j] != now.face[j] || known.sign[j] != now.sign[j];
            }
            if(changed && SS.derivedTopologyChanged.v == 0) SS.derivedTopologyChanged = h;
            known = now;
        }

        Vector u[2] = {};
        for(int j = 0; j < 2 && known.known; j++) {
            Vector d = FaceNormalNow(known.face[j]).Cross(t);
            u[j]     = (known.sign[j] != 0 && d.Magnitude() > LENGTH_EPS)
                           ? d.WithMagnitude(known.sign[j]) : known.into[j];
        }
        double r = valA;
        if(fillet) {
            // The circle of radius r tangent to both faces: its tangent points are set back
            // r / tan(a/2) from the edge, its centre r / sin(a/2) along the bisector.
            double half = acos(std::max(-1.0, std::min(1.0, u[0].Dot(u[1])))) / 2;
            double back = 0, out = 0;
            Vector mid  = u[0].Plus(u[1]);
            if(known.known && half > 1e-6 && mid.Magnitude() > LENGTH_EPS) {
                back = r / tan(half);
                out  = r / sin(half);
                mid  = mid.WithMagnitude(1);
            }
            AddFeatureLine(el, edges[i], REMAP_FILLET_TANGENT_1, a.Plus(u[0].ScaledBy(back)),
                           b.Plus(u[0].ScaledBy(back)));
            AddFeatureLine(el, edges[i], REMAP_FILLET_TANGENT_2, a.Plus(u[1].ScaledBy(back)),
                           b.Plus(u[1].ScaledBy(back)));
            AddFeatureLine(el, edges[i], REMAP_FILLET_AXIS, a.Plus(mid.ScaledBy(out)),
                           b.Plus(mid.ScaledBy(out)));
        } else {
            AddFeatureLine(el, edges[i], REMAP_CHAMFER_EDGE_1, a.Plus(u[0].ScaledBy(r)),
                           b.Plus(u[0].ScaledBy(r)));
            AddFeatureLine(el, edges[i], REMAP_CHAMFER_EDGE_2, a.Plus(u[1].ScaledBy(r)),
                           b.Plus(u[1].ScaledBy(r)));
        }
    }
}

void Group::MakeLatheCircles(EntityList *el, ParamList *param, hEntity in, Vector pt, Vector axis) {
    Entity *ep = SK.GetEntity(in);

    Entity en = {};
    if(ep->IsPoint()) {
        // A point gets revolved to form an arc.
        en.point[0] = Remap(ep->h, REMAP_LATHE_ARC_CENTER);
        en.point[1] = Remap(ep->h, REMAP_LATHE_START);
        en.point[2] = en.point[1]; //Remap(ep->h, REMAP_LATHE_END);

        // Get arc center and point on arc.
        Entity *pc = SK.GetEntity(en.point[0]);
        Entity *pp = SK.GetEntity(en.point[1]);

        // Project arc point to the revolution axis and use it for arc center.
        double k = pp->numPoint.Minus(pt).Dot(axis) / axis.Dot(axis);
        pc->numPoint = pt.Plus(axis.ScaledBy(k));

        // Create arc entity.
        en.group = h;
        en.construction = ep->construction;
        en.style = ep->style;
        en.h = Remap(ep->h, REMAP_PT_TO_ARC);
        en.type = Entity::Type::ARC_OF_CIRCLE;

        // Generate a normal.
        Entity n = {};
        n.workplane = en.workplane;
        n.h = Remap(ep->h, REMAP_PT_TO_NORMAL);
        n.group = en.group;
        n.style = en.style;
        n.type = Entity::Type::NORMAL_N_COPY;

        // Create basis for the normal.
        Vector nu = pp->numPoint.Minus(pc->numPoint).WithMagnitude(1.0);
        Vector nv = nu.Cross(axis).WithMagnitude(1.0);
        n.numNormal = Quaternion::From(nv, nu);

        // The point determines where the normal gets displayed on-screen;
        // it's entirely cosmetic.
        n.point[0] = en.point[0];
        el->Add(&n);
        en.normal = n.h;
        el->Add(&en);
    }
}

void Group::MakeLatheSurfacesSelectable(EntityList *el, hEntity in, Vector axis) {
    Entity *ep = SK.GetEntity(in);

    Entity en = {};
    if(ep->type == Entity::Type::LINE_SEGMENT) {
        // An axis-perpendicular line gets revolved to form a face.
        Vector a = SK.GetEntity(ep->point[0])->PointGetNum();
        Vector b = SK.GetEntity(ep->point[1])->PointGetNum();
        Vector u = b.Minus(a).WithMagnitude(1.0);

        // Check for perpendicularity: calculate cosine of the angle
        // between axis and line direction and check that
        // cos(angle) == 0 <-> angle == +-90 deg.
        if(fabs(u.Dot(axis) / axis.Magnitude()) < ANGLE_COS_EPS) {
            en.param[0] = h.param(0);
            en.param[1] = h.param(1);
            en.param[2] = h.param(2);
            Vector v = axis.Cross(u).WithMagnitude(1.0);
            Vector n = u.Cross(v);
            en.numNormal = Quaternion::From(0, n.x, n.y, n.z);

            en.group = h;
            en.construction = ep->construction;
            en.style = ep->style;
            en.h = Remap(ep->h, REMAP_LINE_TO_FACE);
            en.type = Entity::Type::FACE_NORMAL_PT;
            en.point[0] = Remap(ep->point[0], REMAP_LATHE_START);
            el->Add(&en);
        }
    }
}

// For Revolve and Helix groups the end faces are remapped from an arbitrary
// point on the sketch. We reference the transformed point but there is
// no existing normal so we need to define the rotation and timesApplied.
void Group::MakeRevolveEndFaces(EntityList *el, hEntity pt, int ai, int af)
{
    if(pt.v == 0) return;
    Group *src = SK.GetGroup(opA);
    Vector n = src->polyLoops.normal;

    // When there is no loop normal (e.g. if the loop is broken), use normal of workplane
    // as fallback, to avoid breaking constraints depending on the faces.
    if(n.Equals({0, 0, 0}) && src->type == Group::Type::DRAWING_WORKPLANE) {
        n = SK.GetEntity(src->h.entity(0))->Normal()->NormalN();
    }

    Entity en = {};
    en.type = Entity::Type::FACE_ROT_NORMAL_PT;
    en.group = h;
    // The center of rotation
    en.param[0] = h.param(0);
    en.param[1] = h.param(1);
    en.param[2] = h.param(2);
    // The rotation quaternion
    en.param[3] = h.param(3);
    en.param[4] = h.param(4);
    en.param[5] = h.param(5);
    en.param[6] = h.param(6);

    en.numNormal = Quaternion::From(0, n.x, n.y, n.z);
    en.point[0] = Remap(pt, REMAP_LATHE_START);
    en.timesApplied = ai;
    en.h = Remap(Entity::NO_ENTITY, REMAP_LATHE_START);
    el->Add(&en);

    en.point[0] = Remap(pt, REMAP_LATHE_END);
    en.timesApplied = af;
    en.h = Remap(Entity::NO_ENTITY, REMAP_LATHE_END);
    el->Add(&en);
}

void Group::MakeExtrusionTopBottomFaces(EntityList *el, hEntity pt)
{
    if(pt.v == 0) return;
    Group *src = SK.GetGroup(opA);
    Vector n = src->polyLoops.normal;

    // When there is no loop normal (e.g. if the loop is broken), use normal of workplane
    // as fallback, to avoid breaking constraints depending on the faces.
    if(n.Equals({0, 0, 0}) && src->type == Group::Type::DRAWING_WORKPLANE) {
        n = SK.GetEntity(src->h.entity(0))->Normal()->NormalN();
    }

    Entity en = {};
    en.type = Entity::Type::FACE_NORMAL_PT;
    en.group = h;

    en.numNormal = Quaternion::From(0, n.x, n.y, n.z);
    en.point[0] = Remap(pt, REMAP_TOP);
    en.h = Remap(Entity::NO_ENTITY, REMAP_TOP);
    el->Add(&en);

    en.point[0] = Remap(pt, REMAP_BOTTOM);
    en.h = Remap(Entity::NO_ENTITY, REMAP_BOTTOM);
    el->Add(&en);
}

void Group::CopyEntity(EntityList *el,
                       Entity *ep, int timesApplied, int remap,
                       hParam dx, hParam dy, hParam dz,
                       hParam qw, hParam qvx, hParam qvy, hParam qvz, hParam dist,
                       CopyAs as)
{
    Entity en = {};
    en.type = ep->type;
    en.extraPoints = ep->extraPoints;
    en.h = Remap(ep->h, remap);
    en.timesApplied = timesApplied;
    en.group = h;
    en.construction = ep->construction;
    en.style = ep->style;
    en.str = ep->str;
    en.font = ep->font;
    en.file = ep->file;

    switch(ep->type) {
        case Entity::Type::WORKPLANE:
            // Don't copy these.
            return;

        case Entity::Type::POINT_N_COPY:
        case Entity::Type::POINT_N_TRANS:
        case Entity::Type::POINT_N_ROT_TRANS:
        case Entity::Type::POINT_N_ROT_AA:
        case Entity::Type::POINT_N_ROT_AXIS_TRANS:
        case Entity::Type::POINT_IN_3D:
        case Entity::Type::POINT_IN_2D:
            if(as == CopyAs::N_TRANS) {
                en.type = Entity::Type::POINT_N_TRANS;
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
            } else if(as == CopyAs::NUMERIC) {
                en.type = Entity::Type::POINT_N_COPY;
            } else {
                if(as == CopyAs::N_ROT_AA) {
                    en.type = Entity::Type::POINT_N_ROT_AA;
                } else if (as == CopyAs::N_ROT_AXIS_TRANS) {
                    en.type = Entity::Type::POINT_N_ROT_AXIS_TRANS;
                } else {
                    en.type = Entity::Type::POINT_N_ROT_TRANS;
                }
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
                en.param[3] = qw;
                en.param[4] = qvx;
                en.param[5] = qvy;
                en.param[6] = qvz;
                if (as ==  CopyAs::N_ROT_AXIS_TRANS) {
                    en.param[7] = dist;
                }
            }
            en.numPoint = (ep->actPoint).ScaledBy(scale);
            break;

        case Entity::Type::NORMAL_N_COPY:
        case Entity::Type::NORMAL_N_ROT:
        case Entity::Type::NORMAL_N_ROT_AA:
        case Entity::Type::NORMAL_IN_3D:
        case Entity::Type::NORMAL_IN_2D:
            if(as == CopyAs::N_TRANS || as == CopyAs::NUMERIC) {
                en.type = Entity::Type::NORMAL_N_COPY;
            } else {  // N_ROT_AXIS_TRANS probably doesn't warrant a new entity Type
                if(as == CopyAs::N_ROT_AA || as == CopyAs::N_ROT_AXIS_TRANS) {
                    en.type = Entity::Type::NORMAL_N_ROT_AA;
                } else {
                    en.type = Entity::Type::NORMAL_N_ROT;
                }
                en.param[0] = qw;
                en.param[1] = qvx;
                en.param[2] = qvy;
                en.param[3] = qvz;
            }
            en.numNormal = ep->actNormal;
            if(scale < 0) en.numNormal = en.numNormal.Mirror();

            en.point[0] = Remap(ep->point[0], remap);
            break;

        case Entity::Type::DISTANCE_N_COPY:
        case Entity::Type::DISTANCE:
            en.type = Entity::Type::DISTANCE_N_COPY;
            en.numDistance = ep->actDistance*fabs(scale);
            break;

        case Entity::Type::FACE_NORMAL_PT:
        case Entity::Type::FACE_XPROD:
        case Entity::Type::FACE_N_ROT_TRANS:
        case Entity::Type::FACE_N_TRANS:
        case Entity::Type::FACE_N_ROT_AA:
        case Entity::Type::FACE_ROT_NORMAL_PT:
        case Entity::Type::FACE_N_ROT_AXIS_TRANS:
            if(as == CopyAs::N_TRANS) {
                en.type = Entity::Type::FACE_N_TRANS;
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
            } else if (as == CopyAs::NUMERIC) {
                en.type = Entity::Type::FACE_NORMAL_PT;
            } else if (as == CopyAs::N_ROT_AXIS_TRANS) {
                en.type = Entity::Type::FACE_N_ROT_AXIS_TRANS;
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
                en.param[3] = qw;
                en.param[4] = qvx;
                en.param[5] = qvy;
                en.param[6] = qvz;
                en.param[7] = dist;
            } else {
                if(as == CopyAs::N_ROT_AA) {
                    en.type = Entity::Type::FACE_N_ROT_AA;
                } else {
                    en.type = Entity::Type::FACE_N_ROT_TRANS;
                }
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
                en.param[3] = qw;
                en.param[4] = qvx;
                en.param[5] = qvy;
                en.param[6] = qvz;
            }
            en.numPoint  = (ep->actPoint).ScaledBy(scale);
            en.numNormal = (ep->actNormal).ScaledBy(scale);
            // the new face needs an associated point (old files may not have one)
            if(ep->point[0].v != 0) {
                en.point[0] = Remap(ep->point[0], remap);
            }
            break;

        default: {
            if((Entity::Type::IMAGE == ep->type) && (true == ep->construction)) {
                // Do not copy image entities if they are construction.
                return;
            }

            int i, points;
            bool hasNormal, hasDistance;
            ssassert(EntReqTable::GetEntityInfo(ep->type, ep->extraPoints,
                NULL, &points, &hasNormal, &hasDistance), "No entity info");
            for(i = 0; i < points; i++) {
                en.point[i] = Remap(ep->point[i], remap);
            }
            if(hasNormal)   en.normal   = Remap(ep->normal, remap);
            if(hasDistance) en.distance = Remap(ep->distance, remap);
            break;
        }
    }

    // If the entity came from an linked file where it was invisible then
    // ep->actiVisble will be false, and we should hide it. Or if the entity
    // came from a copy (e.g. step and repeat) of a force-hidden linked
    // entity, then we also want to hide it.
    en.forceHidden = (!ep->actVisible) || ep->forceHidden;

    el->Add(&en);
}

bool Group::ShouldDrawExploded() const {
    return SS.explode && h == SS.GW.activeGroup && type == Type::DRAWING_WORKPLANE && !SS.exportMode;
}

} // namespace SolveSpace
