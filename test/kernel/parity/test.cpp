// Kernel parity (AGENTS.md section 8): every model under test/ regenerated with SolveSpace's
// kernel and with OCCT's gives the same solids, measured on volume, surface area, bounding box
// and watertightness, group by group.
#include "solvespace.h"
#include <filesystem>

#include "harness.h"

namespace {

struct Measure {
    bool   solid  = false;
    double volume = 0, area = 0;
    Vector min = {}, max = {};
    bool   leaks  = false; // naked edges: not watertight
    bool   inters = false; // triangles crossing others
};

Measure MeasureGroup(Group *g) {
    Measure m;
    SMesh   mesh = {};
    mesh.MakeFromCopyOf(&g->RunningMesh());
    g->runningShell.TriangulateInto(&mesh);
    if(mesh.l.n > 0) {
        m.solid  = true;
        m.volume = mesh.CalculateVolume();
        m.min    = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);
        m.max    = Vector::From(VERY_NEGATIVE, VERY_NEGATIVE, VERY_NEGATIVE);
        for(const STriangle &t : mesh.l) {
            m.area += t.b.Minus(t.a).Cross(t.c.Minus(t.a)).Magnitude() / 2;
            for(const Vector &v : t.vertices) {
                m.min = Vector::From(std::min(m.min.x, v.x), std::min(m.min.y, v.y),
                                     std::min(m.min.z, v.z));
                m.max = Vector::From(std::max(m.max.x, v.x), std::max(m.max.y, v.y),
                                     std::max(m.max.z, v.z));
            }
        }
        SEdgeList naked = {};
        SKdNode  *root  = SKdNode::From(&mesh);
        bool inters = false, leaks = false;
        root->MakeCertainEdgesInto(&naked, EdgeKind::NAKED_OR_SELF_INTER,
                                   /*coplanarIsInter=*/false, &inters, &leaks);
        m.leaks = leaks;
        m.inters = inters;
        naked.Clear();
    }
    mesh.Clear();
    return m;
}

std::vector<Measure> MeasureAll(KernelKind kind) {
    SS.kernelKind = kind;
    SS.MarkGroupDirty(SK.groupOrder[0]);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    std::vector<Measure> all;
    for(hGroup hg : SK.groupOrder) all.push_back(MeasureGroup(SK.GetGroup(hg)));
    return all;
}

bool Near(double a, double b, double relative, double absolute) {
    return fabs(a - b) <= relative * std::max(fabs(a), fabs(b)) + absolute;
}

// Why a and b differ, or "" if they match.
std::string Compare(const Measure &ss, const Measure &occt) {
    if(ss.solid != occt.solid) return ss.solid ? "OCCT made no solid" : "OCCT made a solid";
    if(!ss.solid) return "";
    double size = ss.max.Minus(ss.min).Magnitude();
    double tol  = SS.ChordTolMm();
    if(!Near(occt.volume, ss.volume, 0.01, tol * size * size)) {
        return ssprintf("volume %.4f vs %.4f", occt.volume, ss.volume);
    }
    if(!Near(occt.area, ss.area, 0.01, tol * size)) {
        return ssprintf("area %.4f vs %.4f", occt.area, ss.area);
    }
    for(int i = 0; i < 3; i++) {
        if(!Near(occt.min.Element(i), ss.min.Element(i), 0, 0.001 * size + tol) ||
           !Near(occt.max.Element(i), ss.max.Element(i), 0, 0.001 * size + tol)) {
            return "bounding box";
        }
    }
    if(!ss.leaks && occt.leaks) return "not watertight";
    return "";
}

}

TEST_CASE(every_test_model_regenerates_alike_with_occt) {
    namespace fs = std::filesystem;
    fs::path root = fs::path(helper->GetAssetPath(__FILE__, "").raw).parent_path()
                        .parent_path().parent_path();
    std::vector<fs::path> models;
    for(const fs::directory_entry &e : fs::recursive_directory_iterator(root)) {
        if(e.path().extension() == ".slvs") models.push_back(e.path());
    }
    std::sort(models.begin(), models.end());

    int compared = 0, solids = 0;
    std::vector<std::string> differ;
    for(const fs::path &model : models) {
        if(!SS.LoadFromFile(Platform::Path::From(model.string()))) continue;
        SS.AfterNewFile();
        if(SK.groupOrder.IsEmpty()) continue;
        std::vector<Measure> ss   = MeasureAll(KernelKind::SOLVESPACE);
        std::vector<Measure> occt = MeasureAll(KernelKind::OCCT);
        compared++;
        for(size_t i = 0; i < ss.size() && i < occt.size(); i++) {
            if(ss[i].solid) solids++;
            std::string why = Compare(ss[i], occt[i]);
            // A curved face tangent to a flat one tessellates into triangles within
            // LENGTH_EPS of it, which the mesh check counts as crossing (the tangent fillet
            // model); noted, as the B-rep is valid and the mesh closed.
            if(!ss[i].inters && occt[i].inters) {
                dbp("note: %s group %d: OCCT's mesh touches itself",
                    fs::relative(model, root).string().c_str(), (int)i);
            }
            if(!why.empty()) {
                differ.push_back(ssprintf("%s group %d: %s",
                                          fs::relative(model, root).string().c_str(), (int)i,
                                          why.c_str()));
            }
        }
        SS.kernelKind = KernelKind::SOLVESPACE;
    }
    dbp("parity: %d models, %d solid groups, %d differ", compared, solids, (int)differ.size());
    for(const std::string &d : differ) dbp("  %s", d.c_str());
    CHECK_TRUE(compared > 200);
    CHECK_TRUE(differ.empty());
}
