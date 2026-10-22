// Export Parts as 3MF, one file with an object per part, and laid flat (AGENTS.md section 7,
// M4b). box.slvs: a 10 x 20 x 30 box.
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

namespace {

hRequest AddLine(Vector a, Vector b) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

// Part 2: a 4 x 6 x 5 block sketched on the box's top.
hPart LidOnTop() {
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    Select({ Cap(Group::REMAP_TOP) });
    hPart  lid  = SS.NewPart();
    Vector p[4] = { { 2, 2, 30 }, { 6, 2, 30 }, { 6, 8, 30 }, { 2, 8, 30 } };
    hRequest side[4];
    for(int i = 0; i < 4; i++) side[i] = AddLine(p[i], p[(i + 1) % 4]);
    for(int i = 0; i < 4; i++) {
        Constraint::ConstrainCoincident(side[i].entity(2), side[(i + 1) % 4].entity(1));
    }
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group::MenuGroup(Command::GROUP_EXTRUDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *e = SK.GetGroup(SS.GW.activeGroup);
    SK.GetParam(e->h.param(0))->val = 0;
    SK.GetParam(e->h.param(1))->val = 0;
    SK.GetParam(e->h.param(2))->val = 2.5;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return lid;
}

uint32_t Get32(const std::string &s, size_t at) {
    return (uint8_t)s[at] | (uint8_t)s[at + 1] << 8 | (uint8_t)s[at + 2] << 16 |
           (uint32_t)(uint8_t)s[at + 3] << 24;
}

uint32_t Crc(const std::string &s) {
    uint32_t crc = 0xFFFFFFFFu;
    for(unsigned char b : s) {
        crc ^= b;
        for(int k = 0; k < 8; k++) crc = (crc & 1) ? 0xEDB88320u ^ (crc >> 1) : crc >> 1;
    }
    return crc ^ 0xFFFFFFFFu;
}

// A stored zip's files by name; empty if a header or a CRC is wrong.
std::map<std::string, std::string> Unzip(const std::string &zip) {
    std::map<std::string, std::string> files;
    size_t at = 0;
    while(at + 30 <= zip.size() && Get32(zip, at) == 0x04034b50) {
        uint32_t crc = Get32(zip, at + 14), size = Get32(zip, at + 18);
        size_t   nameLen  = (uint8_t)zip[at + 26] | (uint8_t)zip[at + 27] << 8;
        size_t   extraLen = (uint8_t)zip[at + 28] | (uint8_t)zip[at + 29] << 8;
        std::string name = zip.substr(at + 30, nameLen);
        std::string data = zip.substr(at + 30 + nameLen + extraLen, size);
        if(Crc(data) != crc) return {};
        files[name] = data;
        at += 30 + nameLen + extraLen + size;
    }
    if(at + 4 > zip.size() || Get32(zip, at) != 0x02014b50) return {};
    return files;
}

// Each object in a 3MF model: its name, triangle count and volume.
struct Object {
    std::string name;
    int         triangles = 0;
    double      volume    = 0, zmin = VERY_POSITIVE, zmax = VERY_NEGATIVE;
};

std::vector<Object> Objects(const std::string &model) {
    std::vector<Object> objects;
    size_t at = 0;
    while((at = model.find("<object ", at)) != std::string::npos) {
        size_t end = model.find("</object>", at);
        std::string o = model.substr(at, end - at);
        Object obj;
        size_t n = o.find("name=\"") + 6;
        obj.name = o.substr(n, o.find('"', n) - n);
        std::vector<Vector> v;
        size_t p = 0;
        while((p = o.find("<vertex ", p)) != std::string::npos) {
            double x, y, z;
            sscanf(o.c_str() + p, "<vertex x=\"%lf\" y=\"%lf\" z=\"%lf\"", &x, &y, &z);
            v.push_back(Vector::From(x, y, z));
            obj.zmin = std::min(obj.zmin, z);
            obj.zmax = std::max(obj.zmax, z);
            p++;
        }
        p = 0;
        while((p = o.find("<triangle ", p)) != std::string::npos) {
            int a, b, c;
            sscanf(o.c_str() + p, "<triangle v1=\"%d\" v2=\"%d\" v3=\"%d\"", &a, &b, &c);
            obj.volume += v[a].Dot(v[b].Cross(v[c])) / 6;
            obj.triangles++;
            p++;
        }
        objects.push_back(obj);
        at = end;
    }
    return objects;
}

std::string Read(const Platform::Path &path) {
    std::string data;
    ReadFile(path, &data);
    return data;
}

}

TEST_CASE(parts_export_to_one_3mf_with_an_object_each) {
    CHECK_LOAD("box.slvs");
    hPart lid = LidOnTop();
    SS.RenamePart(lid, "lid <top> & \"cap\"");
    Platform::Path path = helper->GetAssetPath(__FILE__, "parts.3mf", "out");
    std::vector<Platform::Path> written = SS.ExportParts(path);
    CHECK_TRUE(written.size() == 1 && written[0].raw == path.raw);
    std::map<std::string, std::string> files = Unzip(Read(path));
    CHECK_TRUE(files.size() == 3);
    CHECK_TRUE(files.count("[Content_Types].xml") && files.count("_rels/.rels"));
    std::vector<Object> objects = Objects(files["3D/3dmodel.model"]);
    CHECK_TRUE(objects.size() == 2);
    CHECK_EQ_STR(objects[0].name, "Part 1");
    CHECK_EQ_STR(objects[1].name, "lid &lt;top&gt; &amp; &quot;cap&quot;");
    CHECK_TRUE(fabs(objects[0].volume - 6000) < 1e-3);
    CHECK_TRUE(fabs(objects[1].volume - 120) < 1e-3);
    CHECK_TRUE(objects[0].triangles == 12 && objects[1].triangles == 12);
    remove(path.raw.c_str());
}

TEST_CASE(a_mesh_lays_flat_on_its_largest_face) {
    CHECK_LOAD("box.slvs");
    SMesh m = {};
    m.MakeFromCopyOf(&Extrusion()->RunningMesh());
    Extrusion()->runningShell.TriangulateInto(&m);
    // Tilted every which way first.
    Quaternion q = Quaternion::From(Vector::From(1, 2, 3).WithMagnitude(1), 0.7);
    for(STriangle &t : m.l) {
        for(Vector &v : t.vertices) v = q.Rotate(v);
    }
    SolveSpaceUI::LayFlat(&m);
    double zmin = VERY_POSITIVE, zmax = VERY_NEGATIVE;
    for(const STriangle &t : m.l) {
        for(const Vector &v : t.vertices) {
            zmin = std::min(zmin, v.z);
            zmax = std::max(zmax, v.z);
        }
    }
    // The 20 x 30 face down: 10 high, standing on z = 0, still closed.
    CHECK_EQ_EPS(zmin, 0);
    CHECK_EQ_EPS(zmax, 10);
    CHECK_TRUE(fabs(m.CalculateVolume() - 6000) < 1e-3);
    m.Clear();
}

TEST_CASE(laid_flat_parts_stand_side_by_side_in_a_3mf) {
    CHECK_LOAD("box.slvs");
    LidOnTop();
    SS.exportLayFlat = true;
    Platform::Path path = helper->GetAssetPath(__FILE__, "flat.3mf", "out");
    CHECK_TRUE(SS.ExportParts(path).size() == 1);
    std::vector<Object> objects = Objects(Unzip(Read(path))["3D/3dmodel.model"]);
    SS.exportLayFlat = false;
    CHECK_TRUE(objects.size() == 2);
    CHECK_EQ_EPS(objects[0].zmin, 0);
    CHECK_EQ_EPS(objects[0].zmax, 10);
    CHECK_EQ_EPS(objects[1].zmin, 0);
    CHECK_EQ_EPS(objects[1].zmax, 4); // the 6 x 5 face down
    CHECK_TRUE(fabs(objects[1].volume - 120) < 1e-3);
    remove(path.raw.c_str());
}
