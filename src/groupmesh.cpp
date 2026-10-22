//-----------------------------------------------------------------------------
// Routines to generate our watertight brep shells from the operations
// and entities specified by the user in each group; templated to work either
// on an SShell of ratpoly surfaces or on an SMesh of triangles.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

void Group::AssembleLoops(bool *allClosed,
                          bool *allCoplanar,
                          bool *allNonZeroLen)
{
    SBezierList sbl = {};

    int i;
    for(auto &e : SK.entity) {
        if(e.group != h)
            continue;
        if(e.construction)
            continue;
        if(e.forceHidden)
            continue;

        e.GenerateBezierCurves(&sbl);
    }

    SBezier *sb;
    *allNonZeroLen = true;
    for(sb = sbl.l.First(); sb; sb = sbl.l.NextAfter(sb)) {
        for(i = 1; i <= sb->deg; i++) {
            if(!(sb->ctrl[i]).Equals(sb->ctrl[0])) {
                break;
            }
        }
        if(i > sb->deg) {
            // This is a zero-length edge.
            *allNonZeroLen = false;
            polyError.errorPointAt = sb->ctrl[0];
            goto done;
        }
    }

    // Try to assemble all these Beziers into loops. The closed loops go into
    // bezierLoops, with the outer loops grouped with their holes. The
    // leftovers, if any, go in bezierOpens.
    bezierLoops.FindOuterFacesFrom(&sbl, &polyLoops, NULL,
                                   SS.ChordTolMm(),
                                   allClosed, &(polyError.notClosedAt),
                                   allCoplanar, &(polyError.errorPointAt),
                                   &bezierOpens);
    done:
    sbl.Clear();
}

void Group::GenerateLoops() {
    polyLoops.Clear();
    bezierLoops.Clear();
    bezierOpens.Clear();

    if(type == Type::DRAWING_3D || type == Type::DRAWING_WORKPLANE ||
       type == Type::ROTATE || type == Type::TRANSLATE || type == Type::LINKED)
    {
        bool allClosed = false, allCoplanar = false, allNonZeroLen = false;
        AssembleLoops(&allClosed, &allCoplanar, &allNonZeroLen);
        if(!allNonZeroLen) {
            polyError.how = PolyError::ZERO_LEN_EDGE;
        } else if(!allCoplanar) {
            polyError.how = PolyError::NOT_COPLANAR;
        } else if(!allClosed) {
            polyError.how = PolyError::NOT_CLOSED;
        } else {
            polyError.how = PolyError::GOOD;
            // The self-intersecting check is kind of slow, so don't run it
            // unless requested.
            if(SS.checkClosedContour) {
                if(polyLoops.SelfIntersecting(&(polyError.errorPointAt))) {
                    polyError.how = PolyError::SELF_INTERSECTING;
                }
            }
        }
    }
}

void SShell::RemapFaces(FaceNamer *namer, int remap) {
    for(SSurface &ss : surface){
        hEntity face = { ss.face };
        if(face == Entity::NO_ENTITY) continue;

        face = namer->Name(face, remap);
        ss.face = face.v;
    }
}

void SMesh::RemapFaces(FaceNamer *namer, int remap) {
    STriangle *tr;
    for(tr = l.First(); tr; tr = l.NextAfter(tr)) {
        hEntity face = { tr->meta.face };
        if(face == Entity::NO_ENTITY) continue;

        face = namer->Name(face, remap);
        tr->meta.face = face.v;
    }
}

static KernelCombine KernelCombineFor(Group::CombineAs how) {
    switch(how) {
        case Group::CombineAs::UNION:        return KernelCombine::UNION;
        case Group::CombineAs::DIFFERENCE:   return KernelCombine::DIFFERENCE;
        case Group::CombineAs::INTERSECTION: return KernelCombine::INTERSECTION;
        case Group::CombineAs::ASSEMBLE:     return KernelCombine::ASSEMBLE;
    }
    return KernelCombine::UNION;
}

KernelJob Group::BuildKernelJob() {
    KernelJob job;
    job.color        = color;
    job.forcedToMesh = IsForcedToMesh();
    job.suppress     = suppress;

    Group *srcg = this;

    // Don't attempt a lathe or extrusion unless the source section is good:
    // planar and not self-intersecting.
    bool haveSrc = true;
    if(type == Type::EXTRUDE || type == Type::LATHE || type == Type::REVOLVE) {
        Group *src = SK.GetGroup(opA);
        if(src->polyError.how != PolyError::GOOD) {
            haveSrc = false;
        }
    }

    if(type == Type::TRANSLATE || type == Type::ROTATE) {
        // A step and repeat gets merged against the group's previous group,
        // not our own previous group.
        srcg = SK.GetGroup(opA);
        job.op               = type == Type::TRANSLATE ? KernelJob::Op::STEP_TRANSLATE
                                                       : KernelJob::Op::STEP_ROTATE;
        job.steps            = (int)valA;
        job.oneSided         = subtype == Subtype::ONE_SIDED;
        job.skipFirst        = skipFirst;
        job.sourceSuppressed = srcg->suppress;
        job.sourceShell      = &srcg->thisShell;
        job.sourceMesh       = &srcg->thisMesh_; // as made: the kernel meshes a shape itself
        job.sourceShape      = srcg->thisShape;
        job.stepTrans        = Vector::From(h.param(0), h.param(1), h.param(2));
        if(type == Type::ROTATE) {
            job.stepTheta = SK.GetParam(h.param(3))->val;
            job.stepAxis  = Vector::From(h.param(4), h.param(5), h.param(6));
        }
    } else if(type == Type::EXTRUDE && haveSrc) {
        Group *src = SK.GetGroup(opA);
        Vector translate = Vector::From(h.param(0), h.param(1), h.param(2));
        job.op    = KernelJob::Op::EXTRUDE;
        job.loops = &src->bezierLoops;
        if(subtype == Subtype::ONE_SIDED || subtype == Subtype::ONE_SKEWED) {
            job.tbot = {}; job.ttop = translate.ScaledBy(2);
        } else {
            job.tbot = translate.ScaledBy(-1); job.ttop = translate.ScaledBy(1);
        }
        // The sides are named after the source sketch's line segments.
        for(Entity &e : SK.entity) {
            if(e.group != opA) continue;
            if(e.type != Entity::Type::LINE_SEGMENT) continue;
            job.sideLines.push_back({ e.h, SK.GetEntity(e.point[0])->PointGetNum(),
                                      SK.GetEntity(e.point[1])->PointGetNum() });
        }
    } else if((type == Type::LATHE || type == Type::REVOLVE || type == Type::HELIX) &&
              haveSrc) {
        Group *src     = SK.GetGroup(opA);
        job.loops      = &src->bezierLoops;
        job.axisPoint  = SK.GetEntity(predef.origin)->PointGetNum();
        job.axis       = SK.GetEntity(predef.entityB)->VectorGetNum().WithMagnitude(1);
        if(type == Type::LATHE) {
            job.op = KernelJob::Op::LATHE;
        } else if(type == Type::REVOLVE) {
            job.op = KernelJob::Op::REVOLVE;
            job.anglef = SK.GetParam(h.param(3))->val * 4; // why the 4 is needed?
            if(subtype != Subtype::ONE_SIDED) {
                job.anglef *= 0.5;
                job.angles = -job.anglef;
            }
        } else {
            job.op = KernelJob::Op::HELIX;
            job.anglef = SK.GetParam(h.param(3))->val * 4; // why the 4 is needed?
            job.distf  = SK.GetParam(h.param(7))->val * 2; // dist is applied twice
            if(subtype != Subtype::ONE_SIDED) {
                job.anglef *= 0.5;
                job.angles = -job.anglef;
                job.distf *= 0.5;
                job.dists = -job.distf;
            }
        }
    } else if(type == Type::FILLET || type == Type::CHAMFER) {
        job.op   = type == Type::FILLET ? KernelJob::Op::FILLET : KernelJob::Op::CHAMFER;
        job.size = FeatureSize();
        for(hEntity he : faces) job.faceNames.push_back(he.v);
        job.outlineOnly = featureOutline != 0;
        for(hEntity he : edges) {
            Entity *e = SK.entity.FindByIdNoOops(he);
            if(e == nullptr) continue;
            SBezierList sbl = {};
            e->GenerateBezierCurves(&sbl);
            job.edgeCurves.emplace_back(sbl.l.begin(), sbl.l.end());
            sbl.Clear();
        }
    } else if(type == Type::LOFT) {
        job.op = KernelJob::Op::LOFT;
        for(hEntity he : profiles) {
            Profile p;
            if(!ProfileOf(he, &p)) continue;
            job.sections.push_back(p.loop);
            job.sectionHoles = job.sectionHoles || p.holes;
        }
        if(!profiles.empty()) {
            job.startName = Remap(profiles.front(), REMAP_LOFT_START).v;
            job.endName   = Remap(profiles.back(), REMAP_LOFT_END).v;
        }
    } else if(type == Type::SWEEP) {
        job.op = KernelJob::Op::SWEEP;
        Profile              p;
        std::vector<hEntity> curves(profiles.begin() + (profiles.empty() ? 0 : 1), profiles.end());
        if(!profiles.empty() && ProfileOf(profiles[0], &p) && PathOf(curves, p.centre, &job.path)) {
            job.sections.push_back(p.loop);
            job.sectionHoles = p.holes;
            job.startName    = Remap(profiles[0], REMAP_SWEEP_START).v;
            job.endName      = Remap(profiles[0], REMAP_SWEEP_END).v;
            job.cornerRadius = valB;
        }
    } else if(type == Type::FIT) {
        Vector dir;
        double s0, s1;
        job.op    = KernelJob::Op::FIT;
        job.loops = &SK.GetGroup(opA)->bezierLoops;
        FitExtrusion(&dir, &s0, &s1, &job.size);
        job.tbot = dir.ScaledBy(s0);
        job.ttop = dir.ScaledBy(s1);
    } else if(type == Type::HOLE) {
        job.op   = KernelJob::Op::HOLE;
        job.hole = hole;
        for(hEntity he : points) {
            Entity *p = SK.entity.FindByIdNoOops(he);
            if(p == nullptr) continue;
            job.holes.push_back({ p->PointGetNum(), HoleDirection(p),
                                  Remap(he, REMAP_HOLE_SEAT).v });
        }
    } else if(type == Type::DRAFT) {
        job.op   = KernelJob::Op::DRAFT;
        job.size = atan(FeatureSize());
        for(hEntity he : faces) {
            Entity *f = SK.entity.FindByIdNoOops(he);
            if(f == nullptr) continue;
            Group *eg = SK.GetGroup(f->group);
            Vector pull = Vector::From(eg->h.param(0), eg->h.param(1), eg->h.param(2));
            job.draftFaces.push_back({ he.v, Remap(he, REMAP_DRAFT_FACE).v, f->FaceGetPointNum(),
                                       pull.WithMagnitude(1) });
        }
    } else if(type == Type::SHELL) {
        job.op        = KernelJob::Op::SHELL;
        job.size      = FeatureSize();
        job.shellSide = shellSide;
        for(hEntity he : faces) job.faceNames.push_back(he.v);
    } else if(type == Type::LINKED) {
        job.op       = KernelJob::Op::LINKED;
        job.impShell = &impShell;
        job.impMesh  = &impMesh;
        job.impShape = impShape;
        job.offset   = { SK.GetParam(h.param(0))->val, SK.GetParam(h.param(1))->val,
                         SK.GetParam(h.param(2))->val };
        job.q        = { SK.GetParam(h.param(3))->val, SK.GetParam(h.param(4))->val,
                         SK.GetParam(h.param(5))->val, SK.GetParam(h.param(6))->val };
        job.scale    = scale;
    }

    job.combine       = KernelCombineFor(srcg->meshCombine);
    job.sourceCombine = job.combine;
    Group *prevg      = srcg->RunningMeshGroup();
    // The first group of a part starts from nothing.
    static const SShell noShell = {};
    static const SMesh  noMesh  = {};
    job.prevShell     = prevg ? &prevg->runningShell : const_cast<SShell *>(&noShell);
    job.prevMesh      = prevg ? &prevg->runningMesh_ : const_cast<SMesh *>(&noMesh);
    job.prevShape     = prevg ? prevg->runningShape : nullptr;
    return job;
}

void Group::ApplyKernelResult(KernelResult *result) {
    bool prevBooleanFailed = booleanFailed;
    thisShell.Clear();
    thisMesh_.Clear();
    runningShell.Clear();
    runningMesh_.Clear();
    runningOutlines_.Clear();
    // The lists move over; the result is left empty.
    thisShell    = result->thisBody.shell;
    thisMesh_    = result->thisBody.mesh;
    runningShell = result->running.shell;
    runningMesh_ = result->running.mesh;
    result->thisBody.shell = {};
    result->thisBody.mesh  = {};
    result->running.shell  = {};
    result->running.mesh   = {};
    thisShape          = std::move(result->thisBody.shape);
    runningShape       = std::move(result->running.shape);
    runningTessellated = result->running.tessellated;
    thisTessellated    = result->thisBody.tessellated;

    // If the Boolean failed, then we should note that in the text screen
    // for this group.
    booleanFailed = result->running.booleanFailed;
    bool problemChanged = featureProblem != result->problem;
    featureProblem      = std::move(result->problem);
    if(booleanFailed != prevBooleanFailed || problemChanged) {
        SS.ScheduleShowTW();
    }
    displayDirty = true;
}

void Group::GenerateShellAndMesh() {
    KernelJob      job = BuildKernelJob();
    GroupFaceNamer namer(this);
    KernelResult   result;
    Kernel().Run(job, namer, &result);
    ApplyKernelResult(&result);
}

void Group::GenerateDisplayItems() {
    // This is potentially slow (since we've got to triangulate a shell, or
    // to find the emphasized edges for a mesh), so we will run it only
    // if its inputs have changed.
    if(displayDirty) {
        Group *pg = RunningMeshGroup();
        bool reshapes = type == Type::FILLET || type == Type::CHAMFER || type == Type::SHELL ||
                        type == Type::DRAFT || type == Type::HOLE;
        if(pg && !thisShape && thisMesh_.IsEmpty() && thisShell.IsEmpty() && !reshapes) {
            // We don't contribute any new solid model in this group, so our
            // display items are identical to the previous group's; which means
            // that we can just display those, and stop ourselves from
            // recalculating for those every time we get a change in this group.
            //
            // Note that this can end up recursing multiple times (if multiple
            // groups that contribute no solid model exist in sequence), but
            // that's okay.
            pg->GenerateDisplayItems();

            displayMesh.Clear();
            displayMesh.MakeFromCopyOf(&(pg->displayMesh));

            displayOutlines.Clear();
            if(SS.GW.showEdges || SS.GW.showOutlines) {
                displayOutlines.MakeFromCopyOf(&pg->displayOutlines);
            }
        } else {
            // We do contribute new solid model, so we have to triangulate the
            // shell, and edge-find the mesh.
            displayMesh.Clear();
            runningShell.TriangulateInto(&displayMesh);
            STriangle *t;
            SMesh &runningMesh = RunningMesh();
            for(t = runningMesh.l.First(); t; t = runningMesh.l.NextAfter(t)) {
                STriangle trn = *t;
                if(!runningTessellated) {
                    Vector n = trn.Normal();
                    trn.an = n;
                    trn.bn = n;
                    trn.cn = n;
                }
                displayMesh.AddTriangle(&trn);
            }

            displayOutlines.Clear();

            if(SS.GW.showEdges || SS.GW.showOutlines) {
                SOutlineList rawOutlines = {};
                if(!runningMesh.l.IsEmpty() && !runningTessellated) {
                    // Triangle mesh only; no shell or emphasized edges.
                    runningMesh.MakeOutlinesInto(&rawOutlines, EdgeKind::EMPHASIZED);
                } else if(runningTessellated && runningShape) {
                    // The kernel made them with the mesh, from its topology.
                    rawOutlines.MakeFromCopyOf(&runningOutlines_);
                } else {
                    displayMesh.MakeOutlinesInto(&rawOutlines, EdgeKind::SHARP);
                }

                PolylineBuilder builder;
                builder.MakeFromOutlines(rawOutlines);
                builder.GenerateOutlines(&displayOutlines);
                rawOutlines.Clear();
            }
        }

        // If we render this mesh, we need to know whether it's transparent,
        // and we'll want all transparent triangles last, to make the depth test
        // work correctly.
        displayMesh.PrecomputeTransparency();

        // Recalculate mass center if needed
        if(SS.centerOfMass.draw && SS.centerOfMass.dirty && h == SS.GW.activeGroup) {
            SS.UpdateCenterOfMass();
        }
        displayDirty = false;
        displayVersion++;
    }
}

// The display items moved to the part's pose, remade when either changes.
static void UpdatePosed(Group *g) {
    Pose pose = SS.PartPose(g->PartOf());
    g->posed  = !pose.IsIdentity();
    if(!g->posed) return;
    if(g->posedVersion == g->displayVersion && pose.Equals(g->posedFor)) return;
    g->posedMesh.Clear();
    for(const STriangle &t : g->displayMesh.l) {
        STriangle tr = t;
        for(int i = 0; i < 3; i++) {
            tr.vertices[i] = pose.Apply(t.vertices[i]);
            tr.normals[i]  = pose.q.Rotate(t.normals[i]);
        }
        g->posedMesh.AddTriangle(&tr);
    }
    g->posedMesh.PrecomputeTransparency();
    g->posedOutlines.Clear();
    g->posedOutlines.MakeFromCopyOf(&g->displayOutlines);
    for(SOutline &o : g->posedOutlines.l) {
        o.a  = pose.Apply(o.a);
        o.b  = pose.Apply(o.b);
        o.nl = pose.q.Rotate(o.nl);
        o.nr = pose.q.Rotate(o.nr);
    }
    g->posedFor     = pose;
    g->posedVersion = g->displayVersion;
}

SMesh &Group::ShownMesh() {
    UpdatePosed(this);
    return posed ? posedMesh : displayMesh;
}

SOutlineList &Group::ShownOutlines() {
    UpdatePosed(this);
    return posed ? posedOutlines : displayOutlines;
}

Group *Group::PreviousGroup() const {
    Group *prev = nullptr;
    for(auto const &gh : SK.groupOrder) {
        Group *g = SK.GetGroup(gh);
        if(g->h == h) {
            return prev;
        }
        prev = g;
    }
    return nullptr;
}

SMesh &Group::ThisMesh() {
    if(thisTessellated && thisMesh_.IsEmpty() && thisShape) {
        KernelTessellate(thisShape, color, &thisMesh_);
    }
    return thisMesh_;
}

SMesh &Group::RunningMesh() {
    if(runningTessellated && runningMesh_.IsEmpty() && runningShape) {
        // The previous group's solid passed on (a sketch): its mesh, in its colours.
        Group *pg = RunningMeshGroup();
        runningOutlines_.Clear();
        if(pg && pg->runningShape == runningShape) {
            runningMesh_.MakeFromCopyOf(&pg->RunningMesh());
            runningOutlines_.MakeFromCopyOf(&pg->runningOutlines_);
        } else {
            KernelTessellate(runningShape, color, &runningMesh_, &runningOutlines_);
        }
    }
    return runningMesh_;
}

bool Group::HasRunningSolid() const {
    return runningShape || !runningMesh_.IsEmpty() || !runningShell.IsEmpty();
}

Group *Group::PreviousGroupInPart() const {
    Group *prev = nullptr;
    for(auto const &gh : SK.groupOrder) {
        Group *g = SK.GetGroup(gh);
        if(g->h == h) return prev;
        if(g->PartOf() == PartOf()) prev = g;
    }
    return nullptr;
}

Group *Group::RunningMeshGroup() const {
    if(type == Type::TRANSLATE || type == Type::ROTATE) {
        return SK.GetGroup(opA)->RunningMeshGroup();
    } else {
        // Each part has its own solid (M4a).
        return PreviousGroupInPart();
    }
}

bool Group::IsMeshGroup() {
    switch(type) {
        case Group::Type::EXTRUDE:
        case Group::Type::LATHE:
        case Group::Type::REVOLVE:
        case Group::Type::HELIX:
        case Group::Type::ROTATE:
        case Group::Type::TRANSLATE:
        case Group::Type::FILLET:
        case Group::Type::CHAMFER:
        case Group::Type::SHELL:
        case Group::Type::DRAFT:
        case Group::Type::HOLE:
        case Group::Type::LOFT:
        case Group::Type::SWEEP:
        case Group::Type::FIT:
            return true;

        default:
            return false;
    }
}

void Group::DrawMesh(DrawMeshAs how, Canvas *canvas) {
    if(!(SS.GW.showShaded ||
         SS.GW.drawOccludedAs != GraphicsWindow::DrawOccludedAs::VISIBLE)) return;

    switch(how) {
        case DrawMeshAs::DEFAULT: {
            // Force the shade color to something dim to not distract from
            // the sketch.
            Canvas::Fill fillFront = {};
            if(!SS.GW.showShaded) {
                fillFront.layer = Canvas::Layer::DEPTH_ONLY;
            }
            if((type == Type::DRAWING_3D || type == Type::DRAWING_WORKPLANE)
               && SS.GW.dimSolidModel) {
                fillFront.color = Style::Color(Style::DIM_SOLID);
            }
            if(regenPending) {
                // A ghost of the old solid while the new one is made on the worker (regen.cpp).
                fillFront.color = Style::Color(Style::DIM_SOLID).WithAlpha(90);
            }
            Canvas::hFill hcfFront = canvas->GetFill(fillFront);

            // The back faces are drawn in red; should never seem them, since we
            // draw closed shells, so that's a debugging aid.
            Canvas::hFill hcfBack = {};
            if(SS.drawBackFaces && !displayMesh.isTransparent) {
                Canvas::Fill fillBack = {};
                fillBack.layer = fillFront.layer;
                fillBack.color = RgbaColor::FromFloat(1.0f, 0.1f, 0.1f);
                hcfBack = canvas->GetFill(fillBack);
            } else {
                hcfBack = hcfFront;
            }

            // Draw the shaded solid into the depth buffer for hidden line removal,
            // and if we're actually going to display it, to the color buffer too.
            canvas->DrawMesh(ShownMesh(), hcfFront, hcfBack);

            // Draw mesh edges, for debugging.
            if(SS.GW.showMesh) {
                Canvas::Stroke strokeTriangle = {};
                strokeTriangle.zIndex = 1;
                strokeTriangle.color  = RgbaColor::FromFloat(0.0f, 1.0f, 0.0f);
                strokeTriangle.width  = 1;
                strokeTriangle.unit   = Canvas::Unit::PX;
                Canvas::hStroke hcsTriangle = canvas->GetStroke(strokeTriangle);
                SEdgeList edges = {};
                for(const STriangle &t : ShownMesh().l) {
                    edges.AddEdge(t.a, t.b);
                    edges.AddEdge(t.b, t.c);
                    edges.AddEdge(t.c, t.a);
                }
                canvas->DrawEdges(edges, hcsTriangle);
                edges.Clear();
            }
            break;
        }

        case DrawMeshAs::HOVERED: {
            Canvas::Fill fill = {};
            fill.color   = Style::Color(Style::HOVERED);
            fill.pattern = Canvas::FillPattern::CHECKERED_A;
            fill.zIndex  = 2;
            Canvas::hFill hcf = canvas->GetFill(fill);

            std::vector<uint32_t> faces;
            hEntity he = SS.GW.hover.entity;
            if(he.v != 0 && SK.GetEntity(he)->IsFace()) {
                faces.push_back(he.v);
            }
            canvas->DrawFaces(ShownMesh(), faces, hcf);
            break;
        }

        case DrawMeshAs::SELECTED: {
            Canvas::Fill fill = {};
            fill.color   = Style::Color(Style::SELECTED);
            fill.pattern = Canvas::FillPattern::CHECKERED_B;
            fill.zIndex  = 1;
            Canvas::hFill hcf = canvas->GetFill(fill);

            std::vector<uint32_t> faces;
            SS.GW.GroupSelection();
            auto const &gs = SS.GW.gs;
            // See also GraphicsWindow::MakeSelected "if(c >= MAX_SELECTABLE_FACES)"
            // and GraphicsWindow::GroupSelection "if(e->IsFace())"
            for(auto &fc : gs.face) {
                faces.push_back(fc.v);
            }
            canvas->DrawFaces(ShownMesh(), faces, hcf);
            break;
        }
    }
}

void Group::Draw(Canvas *canvas) {
    // Everything here gets drawn whether or not the group is hidden; we
    // can control this stuff independently, with show/hide solids, edges,
    // mesh, etc.

    GenerateDisplayItems();
    DrawMesh(DrawMeshAs::DEFAULT, canvas);

    if(SS.GW.showEdges) {
        Canvas::Stroke strokeEdge = Style::Stroke(Style::SOLID_EDGE);
        strokeEdge.zIndex = 1;
        Canvas::hStroke hcsEdge = canvas->GetStroke(strokeEdge);

        canvas->DrawOutlines(ShownOutlines(), hcsEdge,
                             SS.GW.showOutlines
                             ? Canvas::DrawOutlinesAs::EMPHASIZED_WITHOUT_CONTOUR
                             : Canvas::DrawOutlinesAs::EMPHASIZED_AND_CONTOUR);

        if(SS.GW.drawOccludedAs != GraphicsWindow::DrawOccludedAs::INVISIBLE) {
            Canvas::Stroke strokeHidden = Style::Stroke(Style::HIDDEN_EDGE);
            if(SS.GW.drawOccludedAs == GraphicsWindow::DrawOccludedAs::VISIBLE) {
                strokeHidden.stipplePattern = StipplePattern::CONTINUOUS;
            }
            strokeHidden.layer  = Canvas::Layer::OCCLUDED;
            Canvas::hStroke hcsHidden = canvas->GetStroke(strokeHidden);

            canvas->DrawOutlines(ShownOutlines(), hcsHidden,
                                 Canvas::DrawOutlinesAs::EMPHASIZED_AND_CONTOUR);
        }
    }

    if(SS.GW.showOutlines) {
        Canvas::Stroke strokeOutline = Style::Stroke(Style::OUTLINE);
        strokeOutline.zIndex = 1;
        Canvas::hStroke hcsOutline = canvas->GetStroke(strokeOutline);

        canvas->DrawOutlines(ShownOutlines(), hcsOutline,
                             Canvas::DrawOutlinesAs::CONTOUR_ONLY);
    }
}

void Group::DrawPolyError(Canvas *canvas) {
    const Camera &camera = canvas->GetCamera();

    Canvas::Stroke strokeUnclosed = Style::Stroke(Style::DRAW_ERROR);
    strokeUnclosed.color = strokeUnclosed.color.WithAlpha(50);
    Canvas::hStroke hcsUnclosed = canvas->GetStroke(strokeUnclosed);

    Canvas::Stroke strokeError = Style::Stroke(Style::DRAW_ERROR);
    strokeError.layer = Canvas::Layer::FRONT;
    strokeError.width = 1.0f;
    Canvas::hStroke hcsError = canvas->GetStroke(strokeError);

    double textHeight = Style::DefaultTextHeight() / camera.scale;

    // And finally show the polygons too, and any errors if it's not possible
    // to assemble the lines into closed polygons.
    if(polyError.how == PolyError::NOT_CLOSED) {
        // Report this error only in sketch-in-workplane groups; otherwise
        // it's just a nuisance.
        if(type == Type::DRAWING_WORKPLANE) {
            canvas->DrawVectorText(_("not closed contour, or not all same style!"),
                                   textHeight,
                                   polyError.notClosedAt.b, camera.projRight, camera.projUp,
                                   hcsError);
            canvas->DrawLine(polyError.notClosedAt.a, polyError.notClosedAt.b, hcsUnclosed);
        }
    } else if(polyError.how == PolyError::NOT_COPLANAR ||
              polyError.how == PolyError::SELF_INTERSECTING ||
              polyError.how == PolyError::ZERO_LEN_EDGE) {
        // These errors occur at points, not lines
        if(type == Type::DRAWING_WORKPLANE) {
            const char *msg;
            if(polyError.how == PolyError::NOT_COPLANAR) {
                msg = _("points not all coplanar!");
            } else if(polyError.how == PolyError::SELF_INTERSECTING) {
                msg = _("contour is self-intersecting!");
            } else {
                msg = _("zero-length edge!");
            }
            canvas->DrawVectorText(msg, textHeight,
                                   polyError.errorPointAt, camera.projRight, camera.projUp,
                                   hcsError);
        }
    } else {
        // The contours will get filled in DrawFilledPaths.
    }
}

void Group::DrawFilledPaths(Canvas *canvas) {
    for(const SBezierLoopSet &sbls : bezierLoops.l) {
        if(sbls.l.IsEmpty() || sbls.l[0].l.IsEmpty())
            continue;

        // In an assembled loop, all the styles should be the same; so doesn't
        // matter which one we grab.
        const SBezier *sb = &(sbls.l[0].l[0]);
        Style *s = Style::Get({ (uint32_t)sb->auxA });

        Canvas::Fill fill = {};
        fill.zIndex = 1;
        if(s->filled) {
            // This is a filled loop, where the user specified a fill color.
            fill.color = s->fillColor;
        } else if(h == SS.GW.activeGroup && SS.checkClosedContour &&
                    polyError.how == PolyError::GOOD) {
            // If this is the active group, and we are supposed to check
            // for closed contours, and we do indeed have a closed and
            // non-intersecting contour, then fill it dimly.
            fill.color = Style::Color(Style::CONTOUR_FILL).WithAlpha(127);
        } else continue;
        Canvas::hFill hcf = canvas->GetFill(fill);

        SPolygon sp = {};
        sbls.MakePwlInto(&sp);
        canvas->DrawPolygon(sp, hcf);
        sp.Clear();
    }
}

void Group::DrawContourAreaLabels(Canvas *canvas) {
    const Camera &camera = canvas->GetCamera();
    Vector gr = camera.projRight.ScaledBy(1 / camera.scale);
    Vector gu = camera.projUp.ScaledBy(1 / camera.scale);

    for(SBezierLoopSet &sbls : bezierLoops.l) {
        if(sbls.l.IsEmpty() || sbls.l[0].l.IsEmpty())
            continue;

        Vector min = sbls.l[0].l[0].ctrl[0];
        Vector max = min;
        Vector zero = {};
        sbls.GetBoundingProjd({1.0, 0.0, 0.0}, zero, &min.x, &max.x);
        sbls.GetBoundingProjd({0.0, 1.0, 0.0}, zero, &min.y, &max.y);
        sbls.GetBoundingProjd({0.0, 0.0, 1.0}, zero, &min.z, &max.z);

        Vector mid = min.Plus(max).ScaledBy(0.5);

        hStyle hs = { Style::CONSTRAINT };
        Canvas::Stroke stroke = Style::Stroke(hs);
        stroke.layer = Canvas::Layer::FRONT;

        std::string label = SS.MmToStringSI(fabs(sbls.SignedArea()), /*dim=*/2);
        double fontHeight = Style::TextHeight(hs);
        double textWidth  = VectorFont::Builtin()->GetWidth(fontHeight, label),
               textHeight = VectorFont::Builtin()->GetCapHeight(fontHeight);
        Vector pos = mid.Minus(gr.ScaledBy(textWidth / 2.0))
                        .Minus(gu.ScaledBy(textHeight / 2.0));
        canvas->DrawVectorText(label, fontHeight, pos, gr, gu, canvas->GetStroke(stroke));
    }
}

} // namespace SolveSpace
