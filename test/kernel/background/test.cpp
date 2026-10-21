// Background regeneration while dragging (src/regen.cpp): the solids made on the worker are
// the solids a full regeneration makes; a stale result is dropped; a face the worker can't
// name sends the work back to the main thread.
#include "solvespace.h"

#include "harness.h"

static Group *LastGroup() {
    return SK.GetGroup(SK.groupOrder[SK.groupOrder.n - 1]);
}

static std::string Describe(Group *g) {
    SMesh m = {};
    m.MakeFromCopyOf(&g->runningMesh);
    g->runningShell.TriangulateInto(&m);
    std::string s = ssprintf("surfaces %d volume %.4f", g->runningShell.surface.n,
                             m.CalculateVolume());
    m.Clear();
    for(const SSurface &ss : g->thisShell.surface) s += ssprintf(" %x", ss.face);
    return s;
}

// As a drag does: everything from the first sketch on is dirty, the mouse is down.
static void DragFrame() {
    SS.GW.pending.operation = GraphicsWindow::Pending::DRAGGING_POINTS;
    SS.MarkGroupDirty(SK.groupOrder[1]);
    SS.GenerateAll(SolveSpaceUI::Generate::DIRTY);
}

static void EndDrag() {
    SS.GW.pending.operation = GraphicsWindow::Pending::NONE;
}

TEST_CASE(a_drag_makes_the_solids_on_the_worker) {
    CHECK_LOAD("extrudes.slvs");
    SS.GW.activeGroup = LastGroup()->h;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    std::string want = Describe(LastGroup());
    SS.backgroundRegenEnabled = true;
    int applied = SS.regenChainsApplied;
    DragFrame();
    CHECK_TRUE(LastGroup()->regenPending); // a ghost until the worker is done
    EndDrag();
    SS.FlushRegeneration();
    CHECK_FALSE(SS.RegenerationPending());
    CHECK_FALSE(LastGroup()->regenPending);
    CHECK_TRUE(SS.regenChainsApplied == applied + 1);
    CHECK_EQ_STR(Describe(LastGroup()), want);
}

TEST_CASE(a_stale_result_is_dropped) {
    CHECK_LOAD("extrudes.slvs");
    SS.GW.activeGroup = LastGroup()->h;
    SS.backgroundRegenEnabled = true;
    int applied = SS.regenChainsApplied;
    DragFrame();
    EndDrag();
    // Undo, say: a synchronous regeneration while the worker still has the drag's solids.
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SS.FlushRegeneration();
    CHECK_TRUE(SS.regenChainsApplied == applied);
    CHECK_FALSE(LastGroup()->regenPending);
}

static hRequest Line(Vector a, Vector b, bool construction = false) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SK.GetRequest(hr)->construction = construction;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

// A square lathed about the Y axis (as kernel/revolved).
static Group *Lathe() {
    for(Request &r : SK.request) {
        if(r.type == Request::Type::LINE_SEGMENT) r.construction = true;
    }
    Vector   p[4] = { { 10, 0, 0 }, { 20, 0, 0 }, { 20, 10, 0 }, { 10, 10, 0 } };
    hRequest side[4];
    for(int i = 0; i < 4; i++) side[i] = Line(p[i], p[(i + 1) % 4]);
    for(int i = 0; i < 4; i++) {
        Constraint::ConstrainCoincident(side[i].entity(2), side[(i + 1) % 4].entity(1));
    }
    hRequest axis = Line({ 0, 0, 0 }, { 0, 10, 0 }, /*construction=*/true);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SS.GW.ClearSelection();
    GraphicsWindow::Selection sel = {};
    sel.entity = axis.entity(0);
    SS.GW.selection.Add(&sel);
    Group::MenuGroup(Command::GROUP_LATHE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return SK.GetGroup(SS.GW.activeGroup);
}

TEST_CASE(a_face_the_worker_cannot_name_is_made_on_the_main_thread) {
    CHECK_LOAD("sketch.slvs");
    Group *lathe = Lathe();
    CHECK_TRUE(lathe->type == Group::Type::LATHE);
    std::string want = Describe(lathe);
    // Forget the newest name, one only the kernel gives (made after the group's entities),
    // as for a face that has never existed: the worker can't name it.
    auto newest = lathe->remap.begin();
    for(auto it = lathe->remap.begin(); it != lathe->remap.end(); ++it) {
        if(it->second.v > newest->second.v) newest = it;
    }
    EntityKey key = newest->first;
    lathe->remap.erase(newest);
    SS.backgroundRegenEnabled = true;
    int misses = SS.regenMisses;
    SS.GW.pending.operation = GraphicsWindow::Pending::DRAGGING_POINTS;
    SS.MarkGroupDirty(lathe->h);
    SS.GenerateAll(SolveSpaceUI::Generate::DIRTY);
    EndDrag();
    SS.FlushRegeneration();
    CHECK_TRUE(SS.regenMisses == misses + 1);
    CHECK_TRUE(lathe->remap.count(key) == 1); // named again, on the main thread
    CHECK_FALSE(SS.RegenerationPending());
    CHECK_EQ_STR(Describe(lathe), want);
}

TEST_CASE(stock_solvespace_never_regenerates_in_the_background) {
    CHECK_LOAD("extrudes.slvs");
    CHECK_FALSE(SS.backgroundRegenEnabled);
    DragFrame();
    EndDrag();
    CHECK_FALSE(SS.RegenerationPending());
    CHECK_FALSE(LastGroup()->regenPending);
}

TEST_CASE(many_frames_of_dragging_end_in_the_newest_solid) {
    // The main thread solves and submits while the worker works (run under ThreadSanitizer:
    // build with -fsanitize=thread); the solid that stays is the newest one.
    CHECK_LOAD("extrudes.slvs");
    SS.GW.activeGroup = LastGroup()->h;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    std::string want = Describe(LastGroup());
    SS.backgroundRegenEnabled = true;
    for(int frame = 0; frame < 30; frame++) {
        DragFrame();
        SS.PollRegeneration(); // as the event loop does between frames
    }
    EndDrag();
    SS.FlushRegeneration();
    CHECK_FALSE(SS.RegenerationPending());
    CHECK_EQ_STR(Describe(LastGroup()), want);
}

#if defined(HAVE_OCCT)
TEST_CASE(occt_solids_made_on_the_worker_match_a_full_regeneration) {
    // The worker and the main thread share B-reps (a synchronous pass may run while the
    // worker still has a stale chain); run under ThreadSanitizer as well.
    CHECK_LOAD("extrudes.slvs");
    SS.SetKernel(KernelKind::OCCT);
    SS.GW.activeGroup = LastGroup()->h;
    std::string want = Describe(LastGroup());
    SS.backgroundRegenEnabled = true;
    for(int frame = 0; frame < 30; frame++) {
        DragFrame();
        if(frame % 7 == 3) SS.GenerateAll(SolveSpaceUI::Generate::ALL); // an undo, say
        SS.PollRegeneration();
    }
    EndDrag();
    SS.FlushRegeneration();
    CHECK_FALSE(SS.RegenerationPending());
    CHECK_TRUE(LastGroup()->runningShape != nullptr);
    CHECK_EQ_STR(Describe(LastGroup()), want);
}
#endif
