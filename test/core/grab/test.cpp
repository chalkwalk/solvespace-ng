#include "solvespace.h"

#include "harness.h"

static Vector At(double u, double v) {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector  o  = SK.GetEntity(wp->point[0])->PointGetNum();
    return o.Plus(wp->Normal()->NormalU().ScaledBy(u)).Plus(wp->Normal()->NormalV().ScaledBy(v));
}

// A fresh, unconstrained line in the active sketch; returns its request.
static hRequest FreeLine(double u = 200, double v = 200) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, false);
    SK.GetEntity(hr.entity(1))->PointForceTo(At(u, v));
    SK.GetEntity(hr.entity(2))->PointForceTo(At(u + 60, v + 30));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return hr;
}

static Vector P(hEntity h) { return SK.GetEntity(h)->PointGetNum(); }

TEST_CASE(grab_moves_selection_with_the_mouse) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    Vector a0 = P(hr.entity(1)), b0 = P(hr.entity(2));
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(SS.GW.StartGrab(Point2d::From(0, 0)));
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::GRABBING);
    SS.GW.GrabMouseMoved(Point2d::From(40, 0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector d = SS.GW.projRight.ScaledBy(40 / SS.GW.scale);
    CHECK_TRUE(P(hr.entity(1)).Minus(a0.Plus(d)).Magnitude() < 1e-6);
    CHECK_TRUE(P(hr.entity(2)).Minus(b0.Plus(d)).Magnitude() < 1e-6);
}

TEST_CASE(grab_axis_lock_and_typed_distance) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    Vector a0 = P(hr.entity(1));
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(SS.GW.StartGrab(Point2d::From(0, 0)));
    SS.GW.GrabSetAxis(0);
    Vector ax = SS.GW.GrabAxisVector(0);
    SS.GW.GrabMouseMoved(Point2d::From(30, 50));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector moved = P(hr.entity(1)).Minus(a0);
    CHECK_TRUE(moved.Magnitude() > 1e-3);
    CHECK_TRUE(moved.Minus(ax.ScaledBy(moved.Dot(ax))).Magnitude() < 1e-6); // only along X
    SS.GW.GrabSetTyped(12.0);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(P(hr.entity(1)).Minus(a0.Plus(ax.ScaledBy(12))).Magnitude() < 1e-6);
    SS.GW.GrabSetAxis(0); // same axis again: unlocked
    CHECK_TRUE(SS.GW.grab.axis == -1);
}

TEST_CASE(grab_cancel_restores_without_redo) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    // Tie another line to the dragged one so the solver moves it too.
    hRequest other = FreeLine(400, 400);
    Constraint::ConstrainCoincident(hr.entity(2), other.entity(1));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector a0 = P(hr.entity(1)), o0 = P(other.entity(2)), c0 = P(other.entity(1));
    int undo0 = SS.undo.cnt;
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(SS.GW.StartGrab(Point2d::From(0, 0)));
    SS.GW.GrabMouseMoved(Point2d::From(80, 40));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(P(other.entity(1)).Minus(c0).Magnitude() > 1e-3); // the solver moved it
    SS.GW.GrabCancel();
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(P(hr.entity(1)).Minus(a0).Magnitude() < 1e-9);
    CHECK_TRUE(P(other.entity(1)).Minus(c0).Magnitude() < 1e-9);
    CHECK_TRUE(P(other.entity(2)).Minus(o0).Magnitude() < 1e-9);
    CHECK_TRUE(SS.undo.cnt == undo0);
    CHECK_TRUE(SS.redo.cnt == 0);
}

TEST_CASE(grab_confirm_is_one_undo_step) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    Vector a0 = P(hr.entity(1));
    int undo0 = SS.undo.cnt;
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(SS.GW.StartGrab(Point2d::From(0, 0)));
    SS.GW.GrabMouseMoved(Point2d::From(50, 0));
    SS.GW.GrabConfirm();
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(SS.undo.cnt == undo0 + 1);
    RUN_COMMAND("undo");
    CHECK_TRUE(P(hr.entity(1)).Minus(a0).Magnitude() < 1e-9);
}

TEST_CASE(grab_with_nothing_does_nothing) {
    CHECK_LOAD("line.slvs");
    int undo0 = SS.undo.cnt;
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    CHECK_FALSE(SS.GW.StartGrab(Point2d::From(0, 0)));
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(SS.undo.cnt == undo0);
}

TEST_CASE(escape_cancels_a_grab) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    Vector a0 = P(hr.entity(1));
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(SS.GW.StartGrab(Point2d::From(0, 0)));
    SS.GW.GrabMouseMoved(Point2d::From(50, 0));
    RUN_COMMAND("unselect-all");
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(P(hr.entity(1)).Minus(a0).Magnitude() < 1e-9);
}

TEST_CASE(grab_cancel_keeps_the_file_unmodified) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    SS.unsaved = false;
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(SS.GW.StartGrab(Point2d::From(0, 0)));
    SS.GW.GrabMouseMoved(Point2d::From(50, 0));
    SS.GW.GrabCancel();
    CHECK_FALSE(SS.unsaved);
}
