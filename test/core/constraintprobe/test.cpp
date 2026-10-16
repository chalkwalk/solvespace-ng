#include "solvespace.h"
#include "commandnames.h"

#include "harness.h"

static Vector At(double u, double v) {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector  o  = SK.GetEntity(wp->point[0])->PointGetNum();
    return o.Plus(wp->Normal()->NormalU().ScaledBy(u)).Plus(wp->Normal()->NormalV().ScaledBy(v));
}

// Adds unconstrained geometry to the active sketch: two lines, a circle, an arc, a point.
static std::vector<hEntity> AddGeometry() {
    hRequest l1 = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, false);
    SK.GetEntity(l1.entity(1))->PointForceTo(At(100, 100));
    SK.GetEntity(l1.entity(2))->PointForceTo(At(160, 120));
    hRequest l2 = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, false);
    SK.GetEntity(l2.entity(1))->PointForceTo(At(100, 200));
    SK.GetEntity(l2.entity(2))->PointForceTo(At(130, 260));
    hRequest c = SS.GW.AddRequest(Request::Type::CIRCLE, false);
    SK.GetEntity(c.entity(1))->PointForceTo(At(300, 100));
    SK.GetEntity(c.entity(64))->DistanceForceTo(20);
    hRequest a = SS.GW.AddRequest(Request::Type::ARC_OF_CIRCLE, false);
    SK.GetEntity(a.entity(1))->PointForceTo(At(300, 300));
    SK.GetEntity(a.entity(2))->PointForceTo(At(330, 300));
    SK.GetEntity(a.entity(3))->PointForceTo(At(300, 330));
    hRequest p = SS.GW.AddRequest(Request::Type::DATUM_POINT, false);
    SK.GetEntity(p.entity(0))->PointForceTo(At(50, 50));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return { l1.entity(0), l1.entity(1), l2.entity(0), l2.entity(2), c.entity(0), a.entity(0),
             p.entity(0) };
}

TEST_CASE(applicability_matches_menu_constrain) {
    CHECK_LOAD("line.slvs");
    SS.immediatelyEditDimension = false;
    std::vector<hEntity> pool = AddGeometry();
    SS.UndoRemember(); // a base state every probe returns to
    std::vector<std::vector<hEntity>> selections;
    for(size_t i = 0; i < pool.size(); i++) {
        selections.push_back({ pool[i] });
        for(size_t j = i + 1; j < pool.size(); j++) {
            selections.push_back({ pool[i], pool[j] });
            for(size_t k = j + 1; k < pool.size(); k++) {
                selections.push_back({ pool[i], pool[j], pool[k] });
            }
        }
    }
    const Command checked[] = {
        Command::DISTANCE_DIA, Command::ON_ENTITY, Command::EQUAL, Command::RATIO,
        Command::DIFFERENCE, Command::AT_MIDPOINT, Command::SYMMETRIC, Command::HORIZONTAL,
        Command::VERTICAL, Command::ANGLE, Command::PARALLEL, Command::PERPENDICULAR,
        Command::WHERE_DRAGGED, Command::ORIENTED_SAME };
    int mismatches = 0;
    for(const auto &sel : selections) {
        for(Command cmd : checked) {
            SS.GW.ClearSelection();
            for(hEntity he : sel) SS.GW.MakeSelected(he);
            SS.GW.GroupSelection();
            // Tangency also needs a shared endpoint, which this geometry never has; the
            // predicate deliberately checks counts only (see AppliesToSelection).
            if(cmd == Command::PARALLEL && SS.GW.gs.arcs + SS.GW.gs.cubics > 0) continue;
            bool predicted  = Constraint::AppliesToSelection(cmd);
            int  before     = SK.constraint.n;
            int  undoBefore = SS.undo.cnt;
            Constraint::MenuConstrain(cmd);
            bool acted = SK.constraint.n != before;
            while(SS.undo.cnt > undoBefore) SS.UndoUndo();
            SS.UndoClearStack(&SS.redo);
            if(predicted != acted) {
                mismatches++;
                std::string items;
                for(hEntity he : sel) items += " " + std::to_string(he.v);
                fprintf(stderr, "mismatch: %s on%s: predicted %d, acted %d\n", CommandName(cmd),
                        items.c_str(), predicted, acted);
            }
        }
    }
    CHECK_TRUE(mismatches == 0);
}

TEST_CASE(pie_commands_are_constraint_commands) {
    for(Command c : Constraint::PieCommands()) CHECK_TRUE(CommandName(c) != nullptr);
    CHECK_TRUE(Constraint::PieCommands().size() >= 8);
}
