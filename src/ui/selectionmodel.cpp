//-----------------------------------------------------------------------------
// The selection's description, following TextWindow::DescribeSelection case by case.
//-----------------------------------------------------------------------------
#include "ui/selectionmodel.h"

namespace SolveSpace {
namespace Ui {

static std::string Coords(Vector p) {
    return "(" + SS.MmToString(p.x) + ", " + SS.MmToString(p.y) + ", " + SS.MmToString(p.z) +
           ")";
}

static std::string Numbers(Vector p) {
    return ssprintf("(%.3f, %.3f, %.3f)", p.x, p.y, p.z);
}

static SelectionRow Value(std::string label, std::string value) {
    SelectionRow r = {};
    r.label = std::move(label);
    r.value = std::move(value);
    return r;
}

static SelectionRow PointRow(std::string label, hEntity point) {
    SelectionRow r = Value(std::move(label), Coords(SK.GetEntity(point)->PointGetNum()));
    r.entity       = point;
    return r;
}

static SelectionRow EntityRow(std::string label, hEntity he, std::string value) {
    SelectionRow r = Value(std::move(label), std::move(value));
    r.entity       = he;
    return r;
}

static SelectionRow WorkplaneRow(hEntity workplane) {
    Entity *w = SK.GetEntity(workplane);
    if(w->h.isFromRequest()) return EntityRow("in workplane", w->h, w->DescriptionString());
    SelectionRow r = Value("in workplane", w->DescriptionString());
    r.group        = w->h.group();
    return r;
}

static void DescribeEntity(Entity *e, SelectionModel *m) {
    auto &rows = m->rows;
    switch(e->type) {
        case Entity::Type::POINT_IN_3D:
        case Entity::Type::POINT_IN_2D:
        case Entity::Type::POINT_N_TRANS:
        case Entity::Type::POINT_N_ROT_TRANS:
        case Entity::Type::POINT_N_COPY:
        case Entity::Type::POINT_N_ROT_AA:
        case Entity::Type::POINT_N_ROT_AXIS_TRANS:
            m->title = "POINT";
            rows.push_back(PointRow("at", e->h));
            break;

        case Entity::Type::NORMAL_IN_3D:
        case Entity::Type::NORMAL_IN_2D:
        case Entity::Type::NORMAL_N_COPY:
        case Entity::Type::NORMAL_N_ROT:
        case Entity::Type::NORMAL_N_ROT_AA: {
            Quaternion q = e->NormalGetNum();
            m->title = "NORMAL / COORDINATE SYSTEM";
            rows.push_back(Value("basis n", Numbers(q.RotationN())));
            rows.push_back(Value("u", Numbers(q.RotationU())));
            rows.push_back(Value("v", Numbers(q.RotationV())));
            break;
        }
        case Entity::Type::WORKPLANE:
            m->title = "WORKPLANE";
            rows.push_back(PointRow("origin", e->point[0]));
            rows.push_back(EntityRow("normal", e->Normal()->h,
                                     Numbers(e->Normal()->NormalGetNum().RotationN())));
            break;

        case Entity::Type::LINE_SEGMENT: {
            Vector p0 = SK.GetEntity(e->point[0])->PointGetNum();
            Vector p1 = SK.GetEntity(e->point[1])->PointGetNum();
            m->title  = "LINE SEGMENT";
            rows.push_back(PointRow("thru", e->point[0]));
            rows.push_back(PointRow("", e->point[1]));
            rows.push_back(Value("len", SS.MmToString(p1.Minus(p0).Magnitude())));
            break;
        }
        case Entity::Type::CUBIC_PERIODIC:
        case Entity::Type::CUBIC: {
            int pts;
            if(e->type == Entity::Type::CUBIC_PERIODIC) {
                m->title = "PERIODIC C2 CUBIC SPLINE";
                pts      = 3 + e->extraPoints;
            } else if(e->extraPoints > 0) {
                m->title = "INTERPOLATING C2 CUBIC SPLINE";
                pts      = 4 + e->extraPoints;
            } else {
                m->title = "CUBIC BEZIER CURVE";
                pts      = 4;
            }
            for(int i = 0; i < pts; i++) rows.push_back(PointRow(ssprintf("p%d", i), e->point[i]));
            break;
        }
        case Entity::Type::ARC_OF_CIRCLE: {
            double r = e->CircleGetRadiusNum();
            double thetas, thetaf, dtheta;
            e->ArcGetAngles(&thetas, &thetaf, &dtheta);
            m->title = "ARC OF A CIRCLE";
            rows.push_back(PointRow("center", e->point[0]));
            rows.push_back(PointRow("endpoints", e->point[1]));
            rows.push_back(PointRow("", e->point[2]));
            rows.push_back(Value("diameter", SS.MmToString(r * 2)));
            rows.push_back(Value("radius", SS.MmToString(r)));
            rows.push_back(Value("arc len", SS.MmToString(dtheta * r)));
            break;
        }
        case Entity::Type::CIRCLE: {
            double r = e->CircleGetRadiusNum();
            m->title = "CIRCLE";
            rows.push_back(PointRow("center", e->point[0]));
            rows.push_back(Value("diameter", SS.MmToString(r * 2)));
            rows.push_back(Value("radius", SS.MmToString(r)));
            rows.push_back(Value("circumference", SS.MmToString(2 * PI * r)));
            break;
        }
        case Entity::Type::FACE_NORMAL_PT:
        case Entity::Type::FACE_XPROD:
        case Entity::Type::FACE_N_ROT_TRANS:
        case Entity::Type::FACE_N_ROT_AA:
        case Entity::Type::FACE_N_TRANS:
        case Entity::Type::FACE_ROT_NORMAL_PT:
        case Entity::Type::FACE_N_ROT_AXIS_TRANS:
            m->title = "PLANE FACE";
            rows.push_back(Value("normal", Numbers(e->FaceGetNormalNum())));
            rows.push_back(EntityRow("thru", e->h, Coords(e->FaceGetPointNum())));
            break;

        case Entity::Type::TTF_TEXT:
            m->title = "TRUETYPE FONT TEXT";
            rows.push_back(Value("font", e->font));
            if(e->h.isFromRequest()) {
                m->ttf        = e->h.request();
                m->ttfText    = e->str;
                m->ttfFont    = e->font;
                m->ttfKerning = e->extraPoints != 0;
            } else {
                rows.push_back(Value("text", e->str));
            }
            break;

        case Entity::Type::IMAGE: {
            Platform::Path rel = e->file.RelativeTo(SS.saveFile.Parent());
            m->title = "IMAGE";
            rows.push_back(Value("file", (rel.IsEmpty() ? e->file : rel).raw));
            break;
        }
        default:
            m->title = "?? ENTITY";
            break;
    }

    if(e->h.isFromRequest()) {
        Request *r = SK.GetRequest(e->h.request());
        if(e->h == r->h.entity(0)) {
            rows.push_back(Value("from request", r->DescriptionString()));
        } else {
            rows.push_back(EntityRow("from request", r->h.entity(0), r->DescriptionString()));
        }
    }
    SelectionRow group = Value("in group", SK.GetGroup(e->group)->DescriptionString());
    group.group        = e->group;
    rows.push_back(group);
    if(e->workplane == Entity::FREE_IN_3D) {
        rows.push_back(Value("in workplane", "none (free in 3d)"));
    } else {
        rows.push_back(WorkplaneRow(e->workplane));
    }
    if(e->IsStylable()) {
        if(e->style.v) {
            SelectionRow style = Value("in style", Style::Get(e->style)->DescriptionString());
            style.style        = e->style;
            rows.push_back(style);
        } else {
            rows.push_back(Value("in style", "none"));
        }
    }
    if(e->construction) rows.push_back(Value("construction", "yes"));

    // The constraints on the entity or its points.
    std::vector<hConstraint> on;
    auto find = [&](hEntity he) {
        for(const Constraint &c : SK.constraint) {
            if(c.ptA == he || c.ptB == he || c.entityA == he || c.entityB == he ||
               c.entityC == he || c.entityD == he) {
                on.push_back(c.h);
            }
        }
    };
    find(e->h);
    if(!e->IsPoint()) {
        for(int i = 0; i < MAX_POINTS_IN_ENTITY && e->point[i].v; i++) find(e->point[i]);
    }
    std::sort(on.begin(), on.end());
    on.erase(std::unique(on.begin(), on.end()), on.end());
    for(bool reference : { false, true }) {
        SelectionSection s;
        s.title = reference ? "measured by" : "constrained by";
        for(hConstraint hc : on) {
            Constraint *c = SK.GetConstraint(hc);
            if(c->reference != reference) continue;
            SelectionRow row = Value("", c->DescriptionString());
            row.constraint   = hc;
            s.rows.push_back(row);
        }
        if(!s.rows.empty()) m->sections.push_back(s);
    }
}

static void DescribeFaces(SelectionModel *m) {
    const auto &gs = SS.GW.gs;
    char abc = 'A';
    for(hEntity fc : gs.face) {
        Entity *f = SK.GetEntity(fc);
        m->rows.push_back(Value(ssprintf("plane%c normal", abc), Numbers(f->FaceGetNormalNum())));
        m->rows.push_back(EntityRow(ssprintf("plane%c thru", abc), fc,
                                    Coords(f->FaceGetPointNum())));
        abc++;
    }
}

static void DescribeConstraint(Constraint *c, SelectionModel *m) {
    std::string desc = c->DescriptionString();
    m->constraint    = c->h;
    m->reference     = c->reference;
    if(c->type == Constraint::Type::COMMENT) {
        m->title     = "COMMENT";
        m->isComment = true;
        m->valueText = c->comment;
        if(c->ptA != Entity::NO_ENTITY) {
            m->rows.push_back(PointRow("attached to point", c->ptA));
            m->rows.push_back(Value("distance", SS.MmToString(c->disp.offset.Magnitude())));
            m->rows.push_back(Value("d(x, y, z)", Coords(c->disp.offset)));
        }
    } else if(c->HasLabel()) {
        m->title      = c->reference ? "REFERENCE" : "DIMENSION";
        m->hasValue   = true;
        m->valueText  = SS.ConstraintEditText(c->h);
        m->isDiameter = c->type == Constraint::Type::DIAMETER;
        m->showAsRadius = c->other;
    } else {
        m->title = "CONSTRAINT";
    }
    m->rows.push_back(Value("is", desc));

    if(c->IsProjectible()) {
        if(c->workplane == Entity::FREE_IN_3D) {
            m->rows.push_back(Value("in workplane", "none (not projected)"));
        } else {
            m->rows.push_back(WorkplaneRow(c->workplane));
        }
    }

    SelectionSection s;
    s.title = c->reference ? "measures" : "constrains";
    for(hEntity he : { c->ptA, c->ptB, c->entityA, c->entityB, c->entityC, c->entityD }) {
        if(he == Entity::NO_ENTITY || !he.isFromRequest()) continue;
        s.rows.push_back(EntityRow("", he, SK.GetEntity(he)->DescriptionString()));
    }
    if(!s.rows.empty()) m->sections.push_back(s);
}

SelectionModel BuildSelection() {
    SelectionModel m;
    SS.GW.GroupSelection();
    const auto &gs = SS.GW.gs;
    m.count = SS.GW.selection.n;
    m.empty = m.count == 0;
    if(m.empty) return m;

    if(gs.n == 1 && (gs.points == 1 || gs.entities == 1)) {
        DescribeEntity(SK.GetEntity(gs.points == 1 ? gs.point[0] : gs.entity[0]), &m);
    } else if(gs.n == 2 && gs.points == 2) {
        Vector p0 = SK.GetEntity(gs.point[0])->PointGetNum();
        Vector p1 = SK.GetEntity(gs.point[1])->PointGetNum();
        m.title   = "TWO POINTS";
        m.rows.push_back(PointRow("at", gs.point[0]));
        m.rows.push_back(PointRow("", gs.point[1]));
        m.rows.push_back(Value("d", SS.MmToString(p1.Minus(p0).Magnitude())));
        m.rows.push_back(Value("d(x, y, z)", Coords(p1.Minus(p0))));
    } else if(gs.n == 2 && gs.points == 1 && gs.circlesOrArcs == 1) {
        Entity *ec = SK.GetEntity(gs.entity[0]);
        Vector  p  = SK.GetEntity(gs.point[0])->PointGetNum();
        Vector  c  = SK.GetEntity(ec->point[0])->PointGetNum();
        double  r  = ec->CircleGetRadiusNum();
        m.title    = ec->type == Entity::Type::CIRCLE ? "POINT AND A CIRCLE" : "POINT AND AN ARC";
        m.rows.push_back(PointRow("pt at", gs.point[0]));
        m.rows.push_back(PointRow("center", ec->point[0]));
        m.rows.push_back(Value("diameter", SS.MmToString(r * 2)));
        m.rows.push_back(Value("radius", SS.MmToString(r)));
        m.rows.push_back(Value("distance", SS.MmToString(p.Minus(c).Magnitude() - r)));
    } else if(gs.n == 2 && gs.faces == 1 && gs.points == 1) {
        Vector pt = SK.GetEntity(gs.point[0])->PointGetNum();
        Vector n  = SK.GetEntity(gs.face[0])->FaceGetNormalNum();
        Vector pl = SK.GetEntity(gs.face[0])->FaceGetPointNum();
        m.title   = "A POINT AND A PLANE FACE";
        m.rows.push_back(PointRow("point", gs.point[0]));
        m.rows.push_back(Value("plane normal", Numbers(n)));
        m.rows.push_back(EntityRow("plane thru", gs.face[0], Coords(pl)));
        m.rows.push_back(Value("distance", SS.MmToString(n.Dot(pl) - n.Dot(pt))));
    } else if(gs.n == 3 && gs.points == 2 && gs.vectors == 1) {
        Vector p0 = SK.GetEntity(gs.point[0])->PointGetNum();
        Vector p1 = SK.GetEntity(gs.point[1])->PointGetNum();
        Vector v  = SK.GetEntity(gs.vector[0])->VectorGetNum().WithMagnitude(1);
        m.title   = "TWO POINTS AND A VECTOR";
        m.rows.push_back(PointRow("pointA", gs.point[0]));
        m.rows.push_back(PointRow("pointB", gs.point[1]));
        m.rows.push_back(EntityRow("vector", gs.vector[0], Numbers(v)));
        m.rows.push_back(Value("proj_d", SS.MmToString(p1.Minus(p0).Dot(v))));
    } else if(gs.n == 2 && gs.lineSegments == 1 && gs.points == 1) {
        Entity *ln  = SK.GetEntity(gs.entity[0]);
        Vector  lp0 = SK.GetEntity(ln->point[0])->PointGetNum();
        Vector  lp1 = SK.GetEntity(ln->point[1])->PointGetNum();
        Entity *p   = SK.GetEntity(gs.point[0]);
        Vector  pp  = p->PointGetNum();
        m.title     = "LINE SEGMENT AND POINT";
        m.rows.push_back(PointRow("ln thru", ln->point[0]));
        m.rows.push_back(PointRow("", ln->point[1]));
        m.rows.push_back(PointRow("point", gs.point[0]));
        m.rows.push_back(Value("pt-ln distance",
                               SS.MmToString(pp.DistanceToLine(lp0, lp1.Minus(lp0)))));
        hEntity wrkpl = SS.GW.ActiveWorkplane();
        if(wrkpl != Entity::FREE_IN_3D && !(p->workplane == wrkpl && ln->workplane == wrkpl)) {
            Vector ppw = pp.ProjectInto(wrkpl), lp0w = lp0.ProjectInto(wrkpl),
                   lp1w = lp1.ProjectInto(wrkpl);
            m.rows.push_back(Value("in workplane",
                                   SS.MmToString(ppw.DistanceToLine(lp0w, lp1w.Minus(lp0w)))));
        }
    } else if(gs.n == 2 && gs.vectors == 2) {
        Vector v0 = SK.GetEntity(gs.entity[0])->VectorGetNum().WithMagnitude(1);
        Vector v1 = SK.GetEntity(gs.entity[1])->VectorGetNum().WithMagnitude(1);
        double theta = acos(v0.Dot(v1));
        m.title      = "TWO VECTORS";
        m.rows.push_back(EntityRow("vectorA", gs.entity[0], Numbers(v0)));
        m.rows.push_back(EntityRow("vectorB", gs.entity[1], Numbers(v1)));
        m.rows.push_back(Value("angle", ssprintf("%.2f degrees", theta * 180 / PI)));
        while(theta < PI / 2) theta += PI;
        while(theta > PI / 2) theta -= PI;
        m.rows.push_back(Value("or angle", ssprintf("%.2f (mod 180)", theta * 180 / PI)));
    } else if(gs.n == 2 && gs.faces == 2) {
        m.title = "TWO PLANE FACES";
        DescribeFaces(&m);
        Vector n0 = SK.GetEntity(gs.face[0])->FaceGetNormalNum();
        Vector n1 = SK.GetEntity(gs.face[1])->FaceGetNormalNum();
        double theta = acos(n0.Dot(n1));
        m.rows.push_back(Value("angle", ssprintf("%.2f degrees", theta * 180 / PI)));
        while(theta < PI / 2) theta += PI;
        while(theta > PI / 2) theta -= PI;
        m.rows.push_back(Value("or angle", ssprintf("%.2f (mod 180)", theta * 180 / PI)));
        if(fabs(theta) < 0.01) {
            Vector p0 = SK.GetEntity(gs.face[0])->FaceGetPointNum();
            Vector p1 = SK.GetEntity(gs.face[1])->FaceGetPointNum();
            m.rows.push_back(Value("distance", SS.MmToString(p1.Minus(p0).Dot(n0))));
        }
    } else if(gs.n == 3 && gs.faces == 3) {
        m.title = "THREE PLANE FACES";
        DescribeFaces(&m);
    } else if(gs.n == 0 && gs.constraints == 1) {
        DescribeConstraint(SK.GetConstraint(gs.constraint[0]), &m);
    } else {
        m.title = ssprintf("%d ITEM%s", m.count, m.count == 1 ? "" : "S");
    }

    for(int i = 0; i < gs.entities; i++) {
        if(SK.GetEntity(gs.entity[i])->style.v) m.styleAssigned = true;
    }
    for(int i = 0; i < gs.constraints; i++) {
        Constraint *c = SK.GetConstraint(gs.constraint[i]);
        if(c->type == Constraint::Type::COMMENT && c->disp.style.v) m.styleAssigned = true;
    }
    return m;
}

}
}
