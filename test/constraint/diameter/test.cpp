#include "solvespace.h"
#include "harness.h"

TEST_CASE(normal_roundtrip) {
    CHECK_LOAD("normal.slvs");
    CHECK_RENDER("normal.png");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(normal_migrate_from_v20) {
    CHECK_LOAD("normal_v20.slvs");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(normal_migrate_from_v22) {
    CHECK_LOAD("normal_v22.slvs");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(reference_roundtrip) {
    CHECK_LOAD("reference.slvs");
    CHECK_RENDER("reference.png");
    CHECK_SAVE("reference.slvs");
}

TEST_CASE(reference_migrate_from_v20) {
    CHECK_LOAD("reference_v20.slvs");
    CHECK_SAVE("reference.slvs");
}

TEST_CASE(reference_migrate_from_v22) {
    CHECK_LOAD("reference_v22.slvs");
    CHECK_SAVE("reference.slvs");
}

TEST_CASE(label_at_centre_has_a_leader) {
    // A new diameter constraint's label starts at the centre (offset zero).
    CHECK_LOAD("normal.slvs");
    for(Constraint &c : SK.constraint) {
        if(c.type == Constraint::Type::DIAMETER) c.disp.offset = Vector::From(0, 0, 0);
    }
    CHECK_RENDER("label_at_centre.png");
}
