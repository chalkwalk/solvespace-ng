#include "uitest.h"
#include "pie.h"
#include "ui/piemenu.h"

namespace SolveSpace {
namespace UiTest {

bool Near(Point2d a, Point2d b) {
    return a.Minus(b).Magnitude() < 1.0;
}

Point2d At(Vector modelPoint) {
    return Driver::ToWindow(modelPoint);
}

Point2d AtPoint(hEntity point) {
    return At(SK.GetEntity(point)->PointGetNum());
}

Point2d AtMiddle(hRequest line) {
    Vector a = SK.GetEntity(line.entity(1))->PointGetNum();
    Vector b = SK.GetEntity(line.entity(2))->PointGetNum();
    return At(a.Plus(b).ScaledBy(0.5));
}

Point2d AtLabel(hConstraint c) {
    return At(SK.GetConstraint(c)->GetLabelPos(SS.GW.GetCamera()));
}

void ClickAt(Point2d p) {
    Driver::Click(p.x, p.y);
}

void ShiftClickAt(Point2d p) {
    Driver::KeyDown("Shift");
    Driver::Click(p.x, p.y);
    Driver::KeyUp("Shift");
}

void DoubleClickAt(Point2d p) {
    Driver::DoubleClick(p.x, p.y);
}

bool RevealIn(const std::string &panelTitle, const std::string &labelOrId) {
    // Down to the bottom, then back up to the top.
    for(int i = 0; i < 80; i++) {
        std::optional<Driver::Item> item = Driver::FindItem(labelOrId);
        if(item && item->fullyVisible) return true;
        std::optional<Driver::Item> panel = Driver::FindItem(panelTitle);
        if(!panel) return false;
        Driver::Wheel((panel->x0 + panel->x1) / 2, (panel->y0 + panel->y1) / 2, i < 40 ? -1 : 1);
    }
    return false;
}

bool ClosePanel(const std::string &panelTitle) {
    std::optional<Driver::Item> panel = Driver::FindItem(panelTitle);
    if(!panel || !panel->isWindow) return false;
    // ImGui's close button sits at the right end of the title bar.
    Driver::Click(panel->x1 - 14, panel->y0 + 12);
    return !Driver::FindItem(panelTitle);
}

bool EditField(const std::string &labelOrId, const std::string &text) {
    std::optional<Driver::Item> field = Driver::FindItem(labelOrId);
    if(!field) return false;
    Point2d c = field->Center();
    Driver::Click(c.x, c.y);
    Driver::Key("Ctrl+A");
    Driver::Type(text);
    Driver::Key("Return");
    return true;
}

hRequest DrawLine(Point2d a, Point2d b) {
    Driver::Key("L");
    ClickAt(a);
    ClickAt(b);
    Driver::Key("Escape");
    return *LastRequest(Request::Type::LINE_SEGMENT);
}

std::optional<hRequest> LastRequest(Request::Type type) {
    for(int i = SK.request.n - 1; i >= 0; i--) {
        if(SK.request[i].type == type) return SK.request[i].h;
    }
    return std::nullopt;
}

std::optional<hConstraint> LastConstraint(Constraint::Type type) {
    for(int i = SK.constraint.n - 1; i >= 0; i--) {
        if(SK.constraint[i].type == type) return SK.constraint[i].h;
    }
    return std::nullopt;
}

int CountConstraints(Constraint::Type type) {
    int n = 0;
    for(const Constraint &c : SK.constraint) {
        if(c.type == type) n++;
    }
    return n;
}

std::vector<std::string> PieLabels() {
    std::vector<std::string> labels;
    if(!Ui::Pie().IsOpen()) return labels;
    for(const Ui::PieItem &item : Ui::Pie().Items()) labels.push_back(item.label);
    return labels;
}

bool PickPie(const std::string &label) {
    const std::vector<Ui::PieItem> &items = Ui::Pie().Items();
    for(int i = 0; i < (int)items.size(); i++) {
        if(items[i].label != label) continue;
        float dx, dy;
        pie::ItemDirection(i, (int)items.size(), &dx, &dy);
        Driver::Click(Ui::Pie().CenterX() + dx * 80, Ui::Pie().CenterY() + dy * 80);
        return true;
    }
    return false;
}

bool ContextMenu(Point2d p, const std::string &item) {
    bool found = false;
    Driver::DuringModal([&] { found = Driver::ClickItem(item); });
    Driver::Click(p.x, p.y, Driver::Button::RIGHT);
    return found;
}

}
}
