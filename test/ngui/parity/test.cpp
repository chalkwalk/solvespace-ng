// M1 parity (AGENTS.md section 8): every property-browser screen and every menu command has a
// home in the new UI. docs/m1-parity.md is the readable list; these tests keep it true.
#include <set>
#include "../uitest.h"
#include "platform/sdl/menumodel.h"
#include "harness.h"

using namespace SolveSpace::UiTest;
using Screen = TextWindow::Screen;

// Where each browser screen lives now: the panel's title.
static const std::pair<Screen, const char *> ScreenHomes[] = {
    { Screen::LIST_OF_GROUPS,    "Outliner" },
    { Screen::GROUP_INFO,        "Properties" },
    { Screen::GROUP_SOLVE_INFO,  "Problems" },
    { Screen::CONFIGURATION,     "Preferences" },
    { Screen::STEP_DIMENSION,    "Step Dimension" },
    { Screen::LIST_OF_STYLES,    "Styles" },
    { Screen::STYLE_INFO,        "Styles" },
    { Screen::PASTE_TRANSFORMED, "Paste Transformed" },
    { Screen::EDIT_VIEW,         "View" },
    { Screen::TANGENT_ARC,       "Tangent Arc" },
};

TEST_CASE(every_browser_screen_has_a_panel) {
    CHECK_LOAD("line.slvs");
    // A dimension, for the step-dimension screen.
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    ClickAt(AtMiddle(line));
    Driver::Key("D");
    Driver::Key("Return");
    hConstraint dim = *LastConstraint(Constraint::Type::PT_PT_DISTANCE);

    std::set<Screen> covered;
    for(auto [screen, title] : ScreenHomes) {
        covered.insert(screen);
        SS.TW.shown.group      = SS.GW.activeGroup;
        SS.TW.shown.style      = hStyle{ Style::CONSTRUCTION };
        SS.TW.shown.constraint = dim;
        SS.TW.GoToScreen(screen);
        SS.GW.ForceTextWindowShown(); // as the commands that pick a screen do
        Driver::Settle();
        if(!Driver::FindItem(title)) {
            fprintf(stderr, "no panel '%s' for screen %u\n", title, (unsigned)screen);
        }
        CHECK_TRUE(Driver::FindItem(title).has_value());
        CHECK_FALSE(SS.GW.showTextWindow); // never the old browser
    }
    // And the table covers every screen there is.
    for(uint32_t s = 0; s <= (uint32_t)Screen::TANGENT_ARC; s++) {
        CHECK_TRUE(covered.count((Screen)s) == 1);
    }
}

// Every label in the NG menu bar, opening each menu and submenu as a person would.
static std::set<std::string> MenuBarLabels() {
    std::set<std::string> labels;
    std::vector<std::string> top;
    for(const Driver::Item &i : Driver::Items()) {
        if(i.window == "##MainMenuBar" && !i.label.empty() && !i.isWindow) top.push_back(i.label);
    }
    auto menuOpen = [] {
        for(const Driver::Item &i : Driver::Items()) {
            if(i.isWindow && i.window.find("###Menu_") != std::string::npos) return true;
        }
        return false;
    };
    for(const std::string &menu : top) {
        // Close everything and leave the bar: an active menu bar opens menus on hover, and a
        // click would then close the one just opened.
        Driver::Key("Escape");
        Driver::Key("Escape");
        Driver::Move(640, 650);
        Driver::ClickItem(menu);
        if(!menuOpen()) Driver::ClickItem(menu);
        std::vector<Driver::Item> items = Driver::Items();
        for(const Driver::Item &i : items) {
            if(i.window.rfind("##MainMenuBar", 0) == 0 || i.isWindow) continue;
            labels.insert(i.label);
            if(i.openable) {
                Driver::Move(i.Center().x, i.Center().y); // hover opens a submenu
                for(const Driver::Item &sub : Driver::Items()) labels.insert(sub.label);
            }
        }
        Driver::Key("Escape");
    }
    return labels;
}

TEST_CASE(every_menu_command_is_in_the_menu_bar_and_palette) {
    CHECK_LOAD("line.slvs");
    std::set<std::string> labels = MenuBarLabels();
    CHECK_TRUE(labels.size() > 50);
    for(Command c : GraphicsWindow::MenuCommands()) {
        std::string label = Platform::StripMnemonic(GraphicsWindow::MenuLabel(c));
        if(!labels.count(label)) fprintf(stderr, "not in the menu bar: '%s'\n", label.c_str());
        CHECK_TRUE(labels.count(label) == 1);
    }
}
