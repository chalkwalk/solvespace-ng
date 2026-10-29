//-----------------------------------------------------------------------------
// The selection's description, following TextWindow::DescribeSelection case by case.
//-----------------------------------------------------------------------------
#include "ui/selectionmodel.h"

namespace SolveSpace {
namespace Ui {

namespace {
struct ConstraintKind {
    Constraint::Type type;
    const char      *name, *icon;
};
using T = Constraint::Type;
const ConstraintKind ConstraintKinds[] = {
    { T::POINTS_COINCIDENT, N_("Coincident"), "point-on" },
    { T::PT_PT_DISTANCE, N_("Distance"), "dimension" },
    { T::PT_LINE_DISTANCE, N_("Point to line distance"), "dimension" },
    { T::PT_PLANE_DISTANCE, N_("Point to plane distance"), "dimension" },
    { T::PT_FACE_DISTANCE, N_("Point to face distance"), "dimension" },
    { T::PROJ_PT_DISTANCE, N_("Projected distance"), "dimension" },
    { T::PT_IN_PLANE, N_("Point in plane"), "point-on" },
    { T::PT_ON_LINE, N_("Point on line"), "point-on" },
    { T::PT_ON_FACE, N_("Point on face"), "point-on" },
    { T::EQUAL_LENGTH_LINES, N_("Equal length"), "equal" },
    { T::EQ_LEN_PT_LINE_D, N_("Equal length and distance"), "equal" },
    { T::EQ_PT_LN_DISTANCES, N_("Equal distances"), "equal" },
    { T::LENGTH_RATIO, N_("Length ratio"), "dimension" },
    { T::ARC_ARC_LEN_RATIO, N_("Arc length ratio"), "dimension" },
    { T::ARC_LINE_LEN_RATIO, N_("Arc to line length ratio"), "dimension" },
    { T::LENGTH_DIFFERENCE, N_("Length difference"), "dimension" },
    { T::ARC_ARC_DIFFERENCE, N_("Arc length difference"), "dimension" },
    { T::ARC_LINE_DIFFERENCE, N_("Arc to line length difference"), "dimension" },
    { T::SYMMETRIC, N_("Symmetric"), "symmetric" },
    { T::SYMMETRIC_HORIZ, N_("Symmetric horizontally"), "symmetric" },
    { T::SYMMETRIC_VERT, N_("Symmetric vertically"), "symmetric" },
    { T::SYMMETRIC_LINE, N_("Symmetric about line"), "symmetric" },
    { T::AT_MIDPOINT, N_("Midpoint"), "point-on" },
    { T::HORIZONTAL, N_("Horizontal"), "horizontal" },
    { T::VERTICAL, N_("Vertical"), "vertical" },
    { T::DIAMETER, N_("Diameter"), "dimension" },
    { T::PT_ON_CIRCLE, N_("Point on circle"), "point-on" },
    { T::SAME_ORIENTATION, N_("Same orientation"), "same-orientation" },
    { T::ANGLE, N_("Angle"), "angle" },
    { T::PARALLEL, N_("Parallel"), "parallel" },
    { T::ARC_LINE_TANGENT, N_("Tangent"), "parallel" },
    { T::CUBIC_LINE_TANGENT, N_("Tangent"), "parallel" },
    { T::CURVE_CURVE_TANGENT, N_("Tangent"), "parallel" },
    { T::PERPENDICULAR, N_("Perpendicular"), "perpendicular" },
    { T::EQUAL_RADIUS, N_("Equal radius"), "equal" },
    { T::EQUAL_ANGLE, N_("Equal angle"), "equal" },
    { T::EQUAL_LINE_ARC_LEN, N_("Equal line and arc length"), "equal" },
    { T::WHERE_DRAGGED, N_("Lock"), "" },
    { T::COMMENT, N_("Comment"), "" },
};
const ConstraintKind *FindKind(Constraint::Type type) {
    for(const ConstraintKind &k : ConstraintKinds) {
        if(k.type == type) return &k;
    }
    return nullptr;
}
}

std::string ConstraintTypeName(Constraint::Type type) {
    const ConstraintKind *k = FindKind(type);
    return k ? _(k->name) : _("Constraint");
}

std::string ConstraintIcon(Constraint::Type type) {
    const ConstraintKind *k = FindKind(type);
    return k ? k->icon : "";
}

std::string ConstraintLabel(const Constraint &c) {
    return ssprintf("%s (c%03x)", ConstraintTypeName(c.type).c_str(), c.h.v);
}

static std::string Coords(Vector p) {
    return "(" + SS.MmToString(p.x) + ", " + SS.MmToString(p.y) + ", " + SS.MmToString(p.z) +
           ")";
}

static std::string Numbers(Vector p) {
    return ssprintf("(%.3f, %.3f, %.3f)", p.x, p.y, p.z);
}

// A length in the current unit, with the unit (owner review 2026-10-17, P8).
static std::string Len(double mm) {
    return SS.MmToString(mm) + " " + SS.UnitName();
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
    if(w->h.isFromRequest()) return EntityRow(_("workplane"), w->h, w->DescriptionString());
    SelectionRow r = Value(_("workplane"), w->DescriptionString());
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
        case Entity::Type::POINT_ALONG_PARAM:
        case Entity::Type::POINT_N_ROT_AA:
        case Entity::Type::POINT_N_ROT_AXIS_TRANS:
            m->title = _("Point");
            rows.push_back(PointRow(_("at"), e->h));
            break;

        case Entity::Type::NORMAL_IN_3D:
        case Entity::Type::NORMAL_IN_2D:
        case Entity::Type::NORMAL_N_COPY:
        case Entity::Type::NORMAL_N_ROT:
        case Entity::Type::NORMAL_N_ROT_AA: {
            Quaternion q = e->NormalGetNum();
            m->title = _("Normal / coordinate system");
            rows.push_back(Value(_("normal"), Numbers(q.RotationN())));
            rows.push_back(Value(_("u"), Numbers(q.RotationU())));
            rows.push_back(Value(_("v"), Numbers(q.RotationV())));
            break;
        }
        case Entity::Type::WORKPLANE:
            m->title = _("Workplane");
            rows.push_back(PointRow(_("origin"), e->point[0]));
            rows.push_back(EntityRow(_("normal"), e->Normal()->h,
                                     Numbers(e->Normal()->NormalGetNum().RotationN())));
            break;

        case Entity::Type::LINE_SEGMENT: {
            Vector p0 = SK.GetEntity(e->point[0])->PointGetNum();
            Vector p1 = SK.GetEntity(e->point[1])->PointGetNum();
            m->title  = _("Line segment");
            rows.push_back(PointRow(_("from"), e->point[0]));
            rows.push_back(PointRow(_("to"), e->point[1]));
            rows.push_back(Value(_("length"), Len(p1.Minus(p0).Magnitude())));
            break;
        }
        case Entity::Type::CUBIC_PERIODIC:
        case Entity::Type::CUBIC: {
            int pts;
            if(e->type == Entity::Type::CUBIC_PERIODIC) {
                m->title = _("Periodic C2 cubic spline");
                pts      = 3 + e->extraPoints;
            } else if(e->extraPoints > 0) {
                m->title = _("Interpolating C2 cubic spline");
                pts      = 4 + e->extraPoints;
            } else {
                m->title = _("Cubic B\xC3\xA9zier curve");
                pts      = 4;
            }
            for(int i = 0; i < pts; i++) rows.push_back(PointRow(ssprintf("p%d", i), e->point[i]));
            break;
        }
        case Entity::Type::ARC_OF_CIRCLE: {
            double r = e->CircleGetRadiusNum();
            double thetas, thetaf, dtheta;
            e->ArcGetAngles(&thetas, &thetaf, &dtheta);
            m->title = _("Arc of a circle");
            rows.push_back(PointRow(_("center"), e->point[0]));
            rows.push_back(PointRow(_("endpoints"), e->point[1]));
            rows.push_back(PointRow("", e->point[2]));
            rows.push_back(Value(_("diameter"), Len(r * 2)));
            rows.push_back(Value(_("radius"), Len(r)));
            rows.push_back(Value(_("arc length"), Len(dtheta * r)));
            break;
        }
        case Entity::Type::CIRCLE: {
            double r = e->CircleGetRadiusNum();
            m->title = _("Circle");
            rows.push_back(PointRow(_("center"), e->point[0]));
            rows.push_back(Value(_("diameter"), Len(r * 2)));
            rows.push_back(Value(_("radius"), Len(r)));
            rows.push_back(Value(_("circumference"), Len(2 * PI * r)));
            break;
        }
        case Entity::Type::FACE_NORMAL_PT:
        case Entity::Type::FACE_XPROD:
        case Entity::Type::FACE_N_ROT_TRANS:
        case Entity::Type::FACE_N_ROT_AA:
        case Entity::Type::FACE_N_TRANS:
        case Entity::Type::FACE_ROT_NORMAL_PT:
        case Entity::Type::FACE_N_ROT_AXIS_TRANS:
            m->title = _("Plane face");
            rows.push_back(Value(_("normal"), Numbers(e->FaceGetNormalNum())));
            rows.push_back(EntityRow(_("through"), e->h, Coords(e->FaceGetPointNum())));
            break;

        case Entity::Type::TTF_TEXT:
            m->title = _("TrueType font text");
            rows.push_back(Value(_("font"), e->font));
            if(e->h.isFromRequest()) {
                m->ttf        = e->h.request();
                m->ttfText    = e->str;
                m->ttfFont    = e->font;
                m->ttfKerning = e->extraPoints != 0;
            } else {
                rows.push_back(Value(_("text"), e->str));
            }
            break;

        case Entity::Type::IMAGE: {
            Platform::Path rel = e->file.RelativeTo(SS.saveFile.Parent());
            m->title = _("Image");
            rows.push_back(Value(_("file"), (rel.IsEmpty() ? e->file : rel).raw));
            break;
        }
        default:
            m->title = _("Entity");
            break;
    }

    if(e->h.isFromRequest()) {
        Request *r = SK.GetRequest(e->h.request());
        if(e->h == r->h.entity(0)) {
            rows.push_back(Value(_("sketch entity"), r->DescriptionString()));
        } else {
            rows.push_back(EntityRow(_("sketch entity"), r->h.entity(0), r->DescriptionString()));
        }
    }
    SelectionRow group = Value(_("group"), SK.GetGroup(e->group)->DescriptionString());
    group.group        = e->group;
    rows.push_back(group);
    if(e->workplane == Entity::FREE_IN_3D) {
        rows.push_back(Value(_("workplane"), _("none (free in 3d)")));
    } else {
        rows.push_back(WorkplaneRow(e->workplane));
    }
    if(e->IsStylable()) {
        if(e->style.v) {
            SelectionRow style = Value(_("style"), Style::DisplayName(e->style));
            style.style        = e->style;
            rows.push_back(style);
        } else {
            rows.push_back(Value(_("style"), _("none")));
        }
    }
    if(e->construction) rows.push_back(Value(_("construction"), _("yes")));

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
        s.title = reference ? _("measured by") : _("constrained by");
        for(hConstraint hc : on) {
            Constraint *c = SK.GetConstraint(hc);
            if(c->reference != reference) continue;
            SelectionRow row = Value("", ConstraintLabel(*c));
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
        m->rows.push_back(
            Value(ssprintf(_("plane%c normal"), abc), Numbers(f->FaceGetNormalNum())));
        m->rows.push_back(EntityRow(ssprintf(_("plane%c thru"), abc), fc,
                                    Coords(f->FaceGetPointNum())));
        abc++;
    }
}

static void DescribeConstraint(Constraint *c, SelectionModel *m) {
    std::string desc = ConstraintTypeName(c->type);
    m->constraint    = c->h;
    m->reference     = c->reference;
    if(c->type == Constraint::Type::COMMENT) {
        m->title     = _("Comment");
        m->isComment = true;
        m->valueText = c->comment;
        if(c->ptA != Entity::NO_ENTITY) {
            m->rows.push_back(PointRow(_("attached to"), c->ptA));
            m->rows.push_back(Value(_("distance"), Len(c->disp.offset.Magnitude())));
            m->rows.push_back(Value(_("offset (x, y, z)"), Coords(c->disp.offset)));
        }
    } else if(c->HasLabel()) {
        m->title      = c->reference ? _("Reference") : _("Dimension");
        m->hasValue   = true;
        // At the display precision, as the canvas shows it (the unit beside the field).
        double value = fabs(c->valA);
        if(c->type == Constraint::Type::DIAMETER && c->other) value /= 2;
        if(c->type == Constraint::Type::LENGTH_RATIO ||
           c->type == Constraint::Type::ARC_ARC_LEN_RATIO ||
           c->type == Constraint::Type::ARC_LINE_LEN_RATIO) {
            m->valueText = ssprintf("%.3f", value);
        } else if(c->type == Constraint::Type::ANGLE) {
            m->valueText = SS.DegreeToString(value);
            m->unit      = "\xC2\xB0";
        } else {
            m->valueText = SS.MmToString(value, true);
            m->unit      = SS.UnitName();
        }
        m->isDiameter = c->type == Constraint::Type::DIAMETER;
        m->showAsRadius = c->other;
    } else {
        m->title = _("Constraint");
    }
    m->rows.push_back(Value(_("type"), desc));

    if(c->IsProjectible()) {
        if(c->workplane == Entity::FREE_IN_3D) {
            m->rows.push_back(Value(_("workplane"), _("none (not projected)")));
        } else {
            m->rows.push_back(WorkplaneRow(c->workplane));
        }
    }

    SelectionSection s;
    s.title = c->reference ? _("measures") : _("constrains");
    std::set<std::string> listed; // both end points of one line: the line once
    for(hEntity he : { c->ptA, c->ptB, c->entityA, c->entityB, c->entityC, c->entityD }) {
        if(he == Entity::NO_ENTITY || !he.isFromRequest()) continue;
        std::string name = SK.GetEntity(he)->DescriptionString();
        if(!listed.insert(name).second) continue;
        s.rows.push_back(EntityRow("", he, name));
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

    const GraphicsWindow::Selection &first = SS.GW.selection[0];
    if(m.count == 1 && first.solidEdge != 0) {
        // An edge no entity describes (#6): its length, from the outlines drawn.
        double length = 0;
        if(Group *g = SK.group.FindByIdNoOops(first.solidEdgeGroup)) {
            for(const SOutline &so : g->ShownOutlines().l) {
                if(so.edge == first.solidEdge) length += so.b.Minus(so.a).Magnitude();
            }
        }
        m.title = _("Edge of the solid");
        m.rows.push_back(Value(_("length"), Len(length)));
    } else if(gs.n == 1 && (gs.points == 1 || gs.entities == 1)) {
        DescribeEntity(SK.GetEntity(gs.points == 1 ? gs.point[0] : gs.entity[0]), &m);
    } else if(gs.n == 2 && gs.points == 2) {
        Vector p0 = SK.GetEntity(gs.point[0])->PointGetNum();
        Vector p1 = SK.GetEntity(gs.point[1])->PointGetNum();
        m.title   = _("Two points");
        m.rows.push_back(PointRow(_("from"), gs.point[0]));
        m.rows.push_back(PointRow(_("to"), gs.point[1]));
        m.rows.push_back(Value(_("distance"), Len(p1.Minus(p0).Magnitude())));
        m.rows.push_back(Value(_("offset (x, y, z)"), Coords(p1.Minus(p0))));
    } else if(gs.n == 2 && gs.points == 1 && gs.circlesOrArcs == 1) {
        Entity *ec = SK.GetEntity(gs.entity[0]);
        Vector  p  = SK.GetEntity(gs.point[0])->PointGetNum();
        Vector  c  = SK.GetEntity(ec->point[0])->PointGetNum();
        double  r  = ec->CircleGetRadiusNum();
        m.title    = ec->type == Entity::Type::CIRCLE ? _("Point and a circle")
                                                      : _("Point and an arc");
        m.rows.push_back(PointRow(_("point"), gs.point[0]));
        m.rows.push_back(PointRow(_("center"), ec->point[0]));
        m.rows.push_back(Value(_("diameter"), Len(r * 2)));
        m.rows.push_back(Value(_("radius"), Len(r)));
        m.rows.push_back(Value(_("distance"), Len(p.Minus(c).Magnitude() - r)));
    } else if(gs.n == 2 && gs.faces == 1 && gs.points == 1) {
        Vector pt = SK.GetEntity(gs.point[0])->PointGetNum();
        Vector n  = SK.GetEntity(gs.face[0])->FaceGetNormalNum();
        Vector pl = SK.GetEntity(gs.face[0])->FaceGetPointNum();
        m.title   = _("A point and a plane face");
        m.rows.push_back(PointRow(_("point"), gs.point[0]));
        m.rows.push_back(Value(_("plane normal"), Numbers(n)));
        m.rows.push_back(EntityRow(_("plane through"), gs.face[0], Coords(pl)));
        m.rows.push_back(Value(_("distance"), Len(n.Dot(pl) - n.Dot(pt))));
    } else if(gs.n == 3 && gs.points == 2 && gs.vectors == 1) {
        Vector p0 = SK.GetEntity(gs.point[0])->PointGetNum();
        Vector p1 = SK.GetEntity(gs.point[1])->PointGetNum();
        Vector v  = SK.GetEntity(gs.vector[0])->VectorGetNum().WithMagnitude(1);
        m.title   = _("Two points and a vector");
        m.rows.push_back(PointRow(_("point A"), gs.point[0]));
        m.rows.push_back(PointRow(_("point B"), gs.point[1]));
        m.rows.push_back(EntityRow(_("vector"), gs.vector[0], Numbers(v)));
        m.rows.push_back(Value(_("projected distance"), Len(p1.Minus(p0).Dot(v))));
    } else if(gs.n == 2 && gs.lineSegments == 1 && gs.points == 1) {
        Entity *ln  = SK.GetEntity(gs.entity[0]);
        Vector  lp0 = SK.GetEntity(ln->point[0])->PointGetNum();
        Vector  lp1 = SK.GetEntity(ln->point[1])->PointGetNum();
        Entity *p   = SK.GetEntity(gs.point[0]);
        Vector  pp  = p->PointGetNum();
        m.title     = _("Line segment and point");
        m.rows.push_back(PointRow(_("line through"), ln->point[0]));
        m.rows.push_back(PointRow("", ln->point[1]));
        m.rows.push_back(PointRow(_("point"), gs.point[0]));
        m.rows.push_back(Value(_("distance to line"),
                               SS.MmToString(pp.DistanceToLine(lp0, lp1.Minus(lp0)))));
        hEntity wrkpl = SS.GW.ActiveWorkplane();
        if(wrkpl != Entity::FREE_IN_3D && !(p->workplane == wrkpl && ln->workplane == wrkpl)) {
            Vector ppw = pp.ProjectInto(wrkpl), lp0w = lp0.ProjectInto(wrkpl),
                   lp1w = lp1.ProjectInto(wrkpl);
            m.rows.push_back(Value(_("workplane"),
                                   SS.MmToString(ppw.DistanceToLine(lp0w, lp1w.Minus(lp0w)))));
        }
    } else if(gs.n == 2 && gs.vectors == 2) {
        Vector v0 = SK.GetEntity(gs.entity[0])->VectorGetNum().WithMagnitude(1);
        Vector v1 = SK.GetEntity(gs.entity[1])->VectorGetNum().WithMagnitude(1);
        double theta = acos(v0.Dot(v1));
        m.title      = _("Two vectors");
        m.rows.push_back(EntityRow(_("vector A"), gs.entity[0], Numbers(v0)));
        m.rows.push_back(EntityRow(_("vector B"), gs.entity[1], Numbers(v1)));
        m.rows.push_back(Value(_("angle"), ssprintf(_("%.2f degrees"), theta * 180 / PI)));
        while(theta < PI / 2) theta += PI;
        while(theta > PI / 2) theta -= PI;
        m.rows.push_back(Value(_("or"), ssprintf(_("%.2f (mod 180)"), theta * 180 / PI)));
    } else if(gs.n == 2 && gs.faces == 2) {
        m.title = _("Two plane faces");
        DescribeFaces(&m);
        Vector n0 = SK.GetEntity(gs.face[0])->FaceGetNormalNum();
        Vector n1 = SK.GetEntity(gs.face[1])->FaceGetNormalNum();
        double theta = acos(n0.Dot(n1));
        m.rows.push_back(Value(_("angle"), ssprintf(_("%.2f degrees"), theta * 180 / PI)));
        while(theta < PI / 2) theta += PI;
        while(theta > PI / 2) theta -= PI;
        m.rows.push_back(Value(_("or"), ssprintf(_("%.2f (mod 180)"), theta * 180 / PI)));
        if(fabs(theta) < 0.01) {
            Vector p0 = SK.GetEntity(gs.face[0])->FaceGetPointNum();
            Vector p1 = SK.GetEntity(gs.face[1])->FaceGetPointNum();
            m.rows.push_back(Value(_("distance"), Len(p1.Minus(p0).Dot(n0))));
        }
    } else if(gs.n == 3 && gs.faces == 3) {
        m.title = _("Three plane faces");
        DescribeFaces(&m);
    } else if(gs.n == 0 && gs.constraints == 1) {
        DescribeConstraint(SK.GetConstraint(gs.constraint[0]), &m);
    } else {
        m.title = ssprintf(m.count == 1 ? _("%d item") : _("%d items"), m.count);
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
