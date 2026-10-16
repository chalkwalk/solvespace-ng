// The Styles panel and assigning styles from the drawing.
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static bool StylesShown() {
    return Driver::FindItem("Styles").has_value();
}

static Style *Newest() {
    Style *newest = nullptr;
    for(Style &s : SK.style) {
        if(!newest || s.h.v > newest->h.v) newest = &s;
    }
    return newest;
}

TEST_CASE(line_styles_command_opens_the_panel) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "Edit", "Line Styles..." }));
    CHECK_TRUE(StylesShown());
    CHECK_FALSE(SS.GW.showTextWindow);
}

TEST_CASE(create_rename_and_set_a_custom_style) {
    CHECK_LOAD("line.slvs");
    Driver::ClickMenu({ "Edit", "Line Styles..." });
    int before = SK.style.n;
    CHECK_TRUE(Driver::ClickItem("create a new custom style"));
    CHECK_TRUE(SK.style.n == before + 1);
    hStyle hs = Newest()->h;
    CHECK_TRUE(RevealIn("Styles", "style-name"));
    CHECK_TRUE(EditField("style-name", "hidden"));
    CHECK_EQ_STR(Style::Get(hs)->name, "hidden");
    CHECK_TRUE(RevealIn("Styles", "width"));
    CHECK_TRUE(EditField("width", "4"));
    CHECK_TRUE(RevealIn("Styles", "contours are filled"));
    CHECK_TRUE(Driver::ClickItem("contours are filled"));
    CHECK_TRUE(Style::Get(hs)->filled);
    Driver::Key("Ctrl+Z");
    CHECK_FALSE(Style::Get(hs)->filled);
}

TEST_CASE(stipple_combo_sets_the_pattern) {
    CHECK_LOAD("line.slvs");
    Driver::ClickMenu({ "Edit", "Line Styles..." });
    Driver::ClickItem("create a new custom style");
    hStyle hs = Newest()->h;
    CHECK_TRUE(RevealIn("Styles", "dash dot"));
    CHECK_TRUE(Driver::ClickItem("dash dot"));
    CHECK_TRUE(Style::Get(hs)->stippleType == StipplePattern::DASH_DOT);
}

TEST_CASE(right_click_assigns_a_new_style_and_shows_it) {
    CHECK_LOAD("line.slvs");
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    Point2d  m    = AtMiddle(line);
    ClickAt(m);
    bool found = false;
    Driver::DuringModal([&] {
        found = Driver::ClickItem("Assign to Style") &&
                Driver::ClickItem("Newly Created Custom Style...");
    });
    Driver::Click(m.x, m.y, Driver::Button::RIGHT);
    CHECK_TRUE(found);
    CHECK_TRUE(SK.GetRequest(line)->style.v >= Style::FIRST_CUSTOM);
    CHECK_TRUE(StylesShown());
    CHECK_TRUE(Driver::FindItem("style-name").has_value() ||
               RevealIn("Styles", "style-name")); // that style's details
}

TEST_CASE(selection_style_link_opens_the_style) {
    CHECK_LOAD("line.slvs");
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    ClickAt(AtMiddle(line));
    // "in style: none" isn't a link; give it a style first.
    hStyle hs = { Style::CreateCustomStyle() };
    Style::AssignSelectionToStyle(hs.v);
    ClickAt(AtMiddle(line));
    std::optional<Driver::Item> link;
    for(const Driver::Item &i : Driver::Items()) {
        if(i.label == Style::Get(hs)->DescriptionString()) link = i;
    }
    CHECK_TRUE(link.has_value());
    Driver::Click(link->Center().x, link->Center().y);
    CHECK_TRUE(StylesShown());
}
