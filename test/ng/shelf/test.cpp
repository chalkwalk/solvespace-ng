#include "solvespace.h"
#include "ui/keypresets.h"
#include "ui/iconraster.h"
#include "ui/shelfmodel.h"
#include "harness.h"

using namespace SolveSpace::Ui;

static std::vector<ShelfItem> AllItems() {
    std::string err;
    std::vector<ShelfItem> all;
    for(auto &s : BuildShelf(*ModernPreset(&err))) {
        all.insert(all.end(), s.items.begin(), s.items.end());
    }
    return all;
}

TEST_CASE(shelf_has_every_old_toolbar_tool) {
    // src/toolbar.cpp's buttons.
    const Command toolbar[] = {
        Command::LINE_SEGMENT, Command::RECTANGLE, Command::CIRCLE, Command::ARC,
        Command::TTF_TEXT, Command::IMAGE, Command::TANGENT_ARC, Command::CUBIC,
        Command::DATUM_POINT, Command::CONSTRUCTION, Command::SPLIT_CURVES,
        Command::DISTANCE_DIA, Command::ANGLE, Command::HORIZONTAL, Command::VERTICAL,
        Command::PARALLEL, Command::PERPENDICULAR, Command::ON_ENTITY, Command::SYMMETRIC,
        Command::EQUAL, Command::ORIENTED_SAME, Command::OTHER_ANGLE, Command::REFERENCE,
        Command::GROUP_EXTRUDE, Command::GROUP_LATHE, Command::GROUP_HELIX,
        Command::GROUP_REVOLVE, Command::GROUP_ROT, Command::GROUP_TRANS, Command::GROUP_WRKPL,
        Command::GROUP_3D, Command::GROUP_LINK, Command::NEAREST_ISO, Command::ONTO_WORKPLANE };
    std::vector<ShelfItem> all = AllItems();
    for(Command c : toolbar) {
        bool found = false;
        for(const ShelfItem &i : all) found |= i.command == c;
        CHECK_TRUE(found);
    }
}

TEST_CASE(shelf_items_have_labels_and_icons) {
    for(const ShelfItem &i : AllItems()) {
        CHECK_FALSE(i.label.empty());
        CHECK_TRUE(i.label.find('&') == std::string::npos);
        size_t      size = 0;
        const void *svg  = Platform::LoadResource("icons/ng/" + i.icon + ".svg", &size);
        auto im = RasterizeIcon(std::string_view((const char *)svg, size), 24);
        CHECK_TRUE(im.has_value());
        int ink = 0;
        for(size_t p = 3; im && p < im->rgba.size(); p += 4) ink += im->rgba[p] > 128;
        CHECK_TRUE(ink > 10); // the icon draws something
    }
}

TEST_CASE(shelf_items_show_their_keys) {
    for(const ShelfItem &i : AllItems()) {
        if(i.command == Command::LINE_SEGMENT) CHECK_EQ_STR(i.keys, "L");
        if(i.command == Command::GROUP_EXTRUDE) CHECK_EQ_STR(i.keys, "E");
        if(i.command == Command::GROUP_LINK) CHECK_EQ_STR(i.keys, "Ctrl+I");
    }
}
