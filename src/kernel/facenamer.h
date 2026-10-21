//-----------------------------------------------------------------------------
// Naming the faces a kernel makes, so constraints and sketches can refer to them (AGENTS.md
// section 6). A face's entity handle comes from its group's remap table, which the file saves,
// so the names must be made exactly as SolveSpace always has. The main thread names through the
// group itself (GroupFaceNamer, sketch.h); a worker thread through a SnapshotFaceNamer, a copy
// taken before the work started, which can't add names: a name it lacks is a miss, and that
// group is regenerated on the main thread instead.
//
// Included by sketch.h after the handle types and EntityMap.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_KERNEL_FACENAMER_H
#define SOLVESPACE_KERNEL_FACENAMER_H

#include <unordered_set>

class FaceNamer {
public:
    virtual ~FaceNamer() = default;
    // The face entity for (input, copy), made if need be (a cap, an extruded side).
    virtual hEntity Name(hEntity input, int copy) = 0;
    // The same, but NO_ENTITY unless that entity exists (a revolved side).
    virtual hEntity ExistingFace(hEntity input, int copy) = 0;
    bool missed = false; // a snapshot was asked for a name it doesn't have
};

class SnapshotFaceNamer final : public FaceNamer {
public:
    SnapshotFaceNamer(hGroup group, EntityMap remap, std::unordered_set<uint32_t> existing)
        : group(group), remap(std::move(remap)), existing(std::move(existing)) {}

    hEntity Name(hEntity input, int copy) override {
        auto it = remap.find({ input, copy });
        if(it == remap.end()) {
            missed = true;
            return {};
        }
        return group.entity(it->second.v);
    }
    hEntity ExistingFace(hEntity input, int copy) override {
        hEntity face = Name(input, copy);
        if(face.v == 0 || !existing.count(face.v)) return {};
        return face;
    }

private:
    hGroup                       group;
    EntityMap                    remap;
    std::unordered_set<uint32_t> existing;
};

#endif
