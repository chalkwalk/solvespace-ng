// Naming the faces a kernel makes (src/kernel/facenamer.h): on the main thread through the
// group's remap table, as SolveSpace always has; on a worker through a snapshot of it.
#include "solvespace.h"

#include "harness.h"

static Group *FirstExtrude() {
    for(hGroup hg : SK.groupOrder) {
        Group *g = SK.GetGroup(hg);
        if(g->type == Group::Type::EXTRUDE) return g;
    }
    return nullptr;
}

TEST_CASE(the_snapshot_names_faces_as_the_group_does) {
    CHECK_LOAD("extrudes.slvs");
    Group *g = FirstExtrude();
    CHECK_TRUE(g != nullptr);
    if(!g) return;
    GroupFaceNamer    live(g);
    SnapshotFaceNamer snap = g->FaceSnapshot();
    CHECK_TRUE(g->remap.size() > 0);
    for(const auto &entry : g->remap) {
        hEntity in = entry.first.input;
        int     copy = entry.first.copyNumber;
        CHECK_TRUE(snap.Name(in, copy) == live.Name(in, copy));
        CHECK_TRUE(snap.ExistingFace(in, copy) == live.ExistingFace(in, copy));
    }
    CHECK_FALSE(snap.missed);
}

TEST_CASE(an_unknown_face_is_a_miss_for_the_snapshot) {
    CHECK_LOAD("extrudes.slvs");
    Group *g = FirstExtrude();
    CHECK_TRUE(g != nullptr);
    if(!g) return;
    size_t            before = g->remap.size();
    SnapshotFaceNamer snap = g->FaceSnapshot();
    hEntity           unknown = { 0x7ff00001 };
    CHECK_TRUE(snap.Name(unknown, Group::REMAP_LINE_TO_FACE) == Entity::NO_ENTITY);
    CHECK_TRUE(snap.missed);
    CHECK_TRUE(g->remap.size() == before); // a snapshot never changes the group
}
