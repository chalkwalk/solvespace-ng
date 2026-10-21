//-----------------------------------------------------------------------------
// SolveSpace's own kernel: NURBS shells (srf/) and triangle meshes, as Group's solid generation
// always made them; moved here from groupmesh.cpp, reading a KernelJob instead of the group.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

namespace {

template<class T>
void StepAndRepeat(const KernelJob &job, T *steps, T *outs, FaceNamer *namer) {
    int n = job.steps, a0 = 0;
    if(job.oneSided && job.skipFirst) {
        a0++; n++;
    }

    int a;
    // create all the transformed copies
    std::vector <T> transd(n);
    std::vector <T> workA(n);
    workA[0] = {};
    // first generate a shell/mesh with each transformed copy
#pragma omp parallel for
    for(a = a0; a < n; a++) {
        transd[a] = {};
        workA[a] = {};
        int ap = a*2 - (job.oneSided ? 0 : (n-1));

        if(job.op == KernelJob::Op::STEP_TRANSLATE) {
            Vector trans = job.stepTrans.ScaledBy(ap);
            transd[a].MakeFromTransformationOf(steps,
                trans, Quaternion::IDENTITY, 1.0);
        } else {
            Vector trans = job.stepTrans;
            double theta = ap * job.stepTheta;
            double c = cos(theta), s = sin(theta);
            Vector axis = job.stepAxis;
            Quaternion q = Quaternion::From(c, s*axis.x, s*axis.y, s*axis.z);
            // Rotation is centered at t; so A(x - t) + t = Ax + (t - At)
            transd[a].MakeFromTransformationOf(steps,
                trans.Minus(q.Rotate(trans)), q, 1.0);
        }
    }
    for(a = a0; a < n; a++) {
        // We need to rewrite any plane face entities to the transformed ones.
        int remap = (a == (n - 1)) ? Group::REMAP_LAST : a;
        transd[a].RemapFaces(namer, remap);
    }

    std::vector<T> *soFar = &transd;
    std::vector<T> *scratch = &workA;
    // do the boolean operations on pairs of equal size
    while(n > 1) {
        for(a = 0; a < n; a+=2) {
            scratch->at(a/2).Clear();
            // combine a pair of shells
            if((a==0) && (a0==1)) { // if the first was skipped just copy the 2nd
                scratch->at(a/2).MakeFromCopyOf(&(soFar->at(a+1)));
                (soFar->at(a+1)).Clear();
                a0 = 0;
            } else if (a == n-1) { // for an odd number just copy the last one
                scratch->at(a/2).MakeFromCopyOf(&(soFar->at(a)));
                (soFar->at(a)).Clear();
            } else if(job.sourceCombine == KernelCombine::ASSEMBLE) {
                scratch->at(a/2).MakeFromAssemblyOf(&(soFar->at(a)), &(soFar->at(a+1)));
                (soFar->at(a)).Clear();
                (soFar->at(a+1)).Clear();
            } else {
                scratch->at(a/2).MakeFromUnionOf(&(soFar->at(a)), &(soFar->at(a+1)));
                (soFar->at(a)).Clear();
                (soFar->at(a+1)).Clear();
            }
        }
        swap(scratch, soFar);
        n = (n+1)/2;
    }
    outs->Clear();
    *outs = soFar->at(0);
}

template<class T>
void Combine(const KernelJob &job, T *prevs, T *thiss, T *outs) {
    // If this group contributes no new mesh, then our running mesh is the
    // same as last time, no combining required. Likewise if we have a mesh
    // but it's suppressed.
    if(thiss->IsEmpty() || job.suppress) {
        outs->MakeFromCopyOf(prevs);
        return;
    }

    // So our group's shell appears in thisShell. Combine this with the
    // previous group's shell, using the requested operation.
    switch(job.combine) {
        case KernelCombine::UNION:
            outs->MakeFromUnionOf(prevs, thiss);
            break;

        case KernelCombine::DIFFERENCE:
            outs->MakeFromDifferenceOf(prevs, thiss);
            break;

        case KernelCombine::INTERSECTION:
            outs->MakeFromIntersectionOf(prevs, thiss);
            break;

        case KernelCombine::ASSEMBLE:
            outs->MakeFromAssemblyOf(prevs, thiss);
            break;
    }
}

void Extrude(const KernelJob &job, SShell *thisShell, FaceNamer *namer) {
    SBezierLoopSetSet *sblss = job.loops;
    SBezierLoopSet *sbls;
    for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
        int is = thisShell->surface.n;
        // Extrude this outer contour (plus its inner contours, if present)
        thisShell->MakeFromExtrusionOf(sbls, job.tbot, job.ttop, job.color);

        // And for any plane faces, annotate the model with the entity for
        // that face, so that the user can select them with the mouse.
        Vector onOrig = sbls->point;
        int i;
        // Not using range-for here because we're starting at a different place and using
        // indices for meaning.
        for(i = is; i < thisShell->surface.n; i++) {
            SSurface *ss = &(thisShell->surface[i]);
            hEntity face = Entity::NO_ENTITY;

            Vector p = ss->PointAt(0, 0),
                   n = ss->NormalAt(0, 0).WithMagnitude(1);
            double d = n.Dot(p);

            if(i == is || i == (is + 1)) {
                // These are the top and bottom of the shell.
                if(fabs((onOrig.Plus(job.ttop)).Dot(n) - d) < LENGTH_EPS) {
                    face = namer->Name(Entity::NO_ENTITY, Group::REMAP_TOP);
                    ss->face = face.v;
                }
                if(fabs((onOrig.Plus(job.tbot)).Dot(n) - d) < LENGTH_EPS) {
                    face = namer->Name(Entity::NO_ENTITY, Group::REMAP_BOTTOM);
                    ss->face = face.v;
                }
                continue;
            }

            // So these are the sides
            if(ss->degm != 1 || ss->degn != 1) continue;

            for(const KernelJob::SideLine &line : job.sideLines) {
                Vector a = line.a.Plus(job.ttop),
                       b = line.b.Plus(job.ttop);
                // Could get taken backwards, so check all cases.
                if((a.Equals(ss->ctrl[0][0]) && b.Equals(ss->ctrl[1][0])) ||
                   (b.Equals(ss->ctrl[0][0]) && a.Equals(ss->ctrl[1][0])) ||
                   (a.Equals(ss->ctrl[0][1]) && b.Equals(ss->ctrl[1][1])) ||
                   (b.Equals(ss->ctrl[0][1]) && a.Equals(ss->ctrl[1][1])))
                {
                    face = namer->Name(line.entity, Group::REMAP_LINE_TO_FACE);
                    ss->face = face.v;
                    break;
                }
            }
        }
    }
}

class SolveSpaceKernel final : public IKernel {
public:
    void Run(const KernelJob &job, FaceNamer &namer, KernelResult *result) override {
        SShell &thisShell = result->thisBody.shell, &runningShell = result->running.shell;
        SMesh  &thisMesh  = result->thisBody.mesh;
        result->Clear();
        SBezierLoopSetSet *sblss = job.loops;
        SBezierLoopSet    *sbls;

        switch(job.op) {
            case KernelJob::Op::NONE: break;

            case KernelJob::Op::STEP_TRANSLATE:
            case KernelJob::Op::STEP_ROTATE:
                if(job.sourceSuppressed) break;
                if(!job.forcedToMesh) {
                    StepAndRepeat<SShell>(job, job.sourceShell, &thisShell, &namer);
                } else {
                    SMesh prevm = {};
                    prevm.MakeFromCopyOf(job.sourceMesh);
                    job.sourceShell->TriangulateInto(&prevm);
                    StepAndRepeat<SMesh>(job, &prevm, &thisMesh, &namer);
                }
                break;

            case KernelJob::Op::EXTRUDE: Extrude(job, &thisShell, &namer); break;

            case KernelJob::Op::LATHE:
                for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
                    thisShell.MakeFromRevolutionOf(sbls, job.axisPoint, job.axis, job.color,
                                                   &namer);
                }
                break;

            case KernelJob::Op::REVOLVE:
                for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
                    if(fabs(job.anglef - job.angles) < 2 * PI) {
                        thisShell.MakeFromHelicalRevolutionOf(sbls, job.axisPoint, job.axis,
                                                              job.color, &namer, job.angles,
                                                              job.anglef, job.dists, job.distf);
                    } else {
                        thisShell.MakeFromRevolutionOf(sbls, job.axisPoint, job.axis, job.color,
                                                       &namer);
                    }
                }
                break;

            case KernelJob::Op::HELIX:
                for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
                    thisShell.MakeFromHelicalRevolutionOf(sbls, job.axisPoint, job.axis,
                                                          job.color, &namer, job.angles,
                                                          job.anglef, job.dists, job.distf);
                }
                break;

            case KernelJob::Op::LINKED:
                // The imported shell or mesh are copied over, with the appropriate
                // transformation applied. We also must remap the face entities.
                thisMesh.MakeFromTransformationOf(job.impMesh, job.offset, job.q, job.scale);
                thisMesh.RemapFaces(&namer, 0);

                thisShell.MakeFromTransformationOf(job.impShell, job.offset, job.q, job.scale);
                thisShell.RemapFaces(&namer, 0);
                break;

            case KernelJob::Op::FILLET:
            case KernelJob::Op::CHAMFER:
            case KernelJob::Op::SHELL:
            case KernelJob::Op::DRAFT:
            case KernelJob::Op::HOLE:
                // The previous solid passes through.
                result->problem = "fillets, chamfers, shells, drafts and holes need the Open CASCADE kernel "
                                  "(Preferences, SolveSpace NG) and a solid that is not a "
                                  "triangle mesh";
                break;
        }

        if(job.sourceCombine != KernelCombine::ASSEMBLE) {
            thisShell.MergeCoincidentSurfaces();
        }

        // So now we've got the mesh or shell for this group. Combine it with
        // the previous group's mesh or shell with the requested Boolean, and
        // we're done.
        if(!job.forcedToMesh) {
            Combine<SShell>(job, job.prevShell, &thisShell, &runningShell);

            if(job.sourceCombine != KernelCombine::ASSEMBLE) {
                runningShell.MergeCoincidentSurfaces();
            }
            result->running.booleanFailed = runningShell.booleanFailed;
        } else {
            CombineAsMeshes(job, result);
        }
    }

    bool Supports(KernelFeature feature) const override {
        switch(feature) {
            case KernelFeature::EXTRUDE:
            case KernelFeature::LATHE:
            case KernelFeature::REVOLVE:
            case KernelFeature::HELIX:
            case KernelFeature::PATTERN:
            case KernelFeature::LINK:
            case KernelFeature::BOOLEAN: return true;
            case KernelFeature::FILLET:
            case KernelFeature::CHAMFER:
            case KernelFeature::SHELL:
            case KernelFeature::DRAFT:
            case KernelFeature::HOLE:
            case KernelFeature::LOFT:
            case KernelFeature::SWEEP: return false;
        }
        return false;
    }
};

}

void CombineAsMeshes(const KernelJob &job, KernelResult *result) {
    SMesh prevm, thism;
    prevm = {};
    thism = {};

    prevm.MakeFromCopyOf(job.prevMesh);
    job.prevShell->TriangulateInto(&prevm);

    thism.MakeFromCopyOf(&result->thisBody.mesh);
    result->thisBody.shell.TriangulateInto(&thism);

    SMesh outm = {};
    Combine<SMesh>(job, &prevm, &thism, &outm);

    // Remove degenerate triangles; if we don't, they'll get split in SnapToMesh
    // in every generated group, resulting in polynomial increase in triangle count,
    // and corresponding slowdown.
    outm.RemoveDegenerateTriangles();

    SMesh &runningMesh = result->running.mesh;
    if(job.sourceCombine != KernelCombine::ASSEMBLE) {
        // And make sure that the output mesh is vertex-to-vertex.
        SKdNode *root = SKdNode::From(&outm);
        root->SnapToMesh(&outm);
        root->MakeMeshInto(&runningMesh);
    } else {
        runningMesh.MakeFromCopyOf(&outm);
    }

    outm.Clear();
    thism.Clear();
    prevm.Clear();
}

IKernel &SolveSpaceKernelInstance() {
    static SolveSpaceKernel kernel;
    return kernel;
}

#if !defined(HAVE_OCCT)
IKernel &Kernel(KernelKind) {
    return SolveSpaceKernelInstance();
}

bool KernelAvailable(KernelKind kind) {
    return kind == KernelKind::SOLVESPACE;
}

bool KernelExportStep(const std::shared_ptr<const KernelShape> &, const Platform::Path &,
                      double) {
    return false;
}

KernelEdgeFacesResult KernelEdgeFaces(const std::shared_ptr<const KernelShape> &, Vector, Vector,
                                      KernelEdgeFace[2]) {
    return KernelEdgeFacesResult::NOT_FOUND;
}

bool KernelMassProperties(const std::shared_ptr<const KernelShape> &, MassProps *) {
    return false;
}

std::vector<KernelFaceNormal> KernelFaceNormals(const std::shared_ptr<const KernelShape> &) {
    return {};
}
#endif

IKernel &Kernel() {
    return Kernel(SS.kernelKind);
}

}
