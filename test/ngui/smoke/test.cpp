#include "solvespace.h"
#include "platform/sdl/uidriver.h"
#include "ui/ngui.h"

#include "harness.h"

namespace Driver = SolveSpace::Platform::Driver;

static bool Near(Point2d a, Point2d b) {
    return a.Minus(b).Magnitude() < 1.0;
}

TEST_CASE(palette_runs_a_command_typed_by_name) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Space");
    CHECK_TRUE(Ui::CapturesText());
    Driver::Type("line segment");
    Driver::Key("Return");
    CHECK_FALSE(Ui::CapturesText());
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::COMMAND);
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
}

TEST_CASE(clicks_draw_a_line_where_the_mouse_is) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    Driver::Click(600, 420);
    Driver::Click(760, 300);
    Driver::Key("Escape");
    CHECK_TRUE(SK.request.n == before + 1);
    Request *r = &SK.request[SK.request.n - 1];
    CHECK_TRUE(r->type == Request::Type::LINE_SEGMENT);
    Vector a = SK.GetEntity(r->h.entity(1))->PointGetNum();
    Vector b = SK.GetEntity(r->h.entity(2))->PointGetNum();
    CHECK_TRUE(Near(Driver::ToWindow(a), Point2d::From(600, 420)));
    CHECK_TRUE(Near(Driver::ToWindow(b), Point2d::From(760, 300)));
}
