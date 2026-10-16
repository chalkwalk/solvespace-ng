#include <set>
#include "solvespace.h"
#include "commandnames.h"

#include "harness.h"

TEST_CASE(round_trip_known_names) {
    CHECK_EQ_STR(CommandName(Command::LINE_SEGMENT), "line-segment");
    CHECK_TRUE(CommandFromName("line-segment") == Command::LINE_SEGMENT);
    CHECK_EQ_STR(CommandName(Command::SELECT_ALL), "select-all");
    CHECK_TRUE(CommandFromName("select-all") == Command::SELECT_ALL);
}

TEST_CASE(unknown_names_are_none) {
    CHECK_TRUE(CommandFromName("") == Command::NONE);
    CHECK_TRUE(CommandFromName("no-such-command") == Command::NONE);
    CHECK_TRUE(CommandFromName("SELECT-ALL") == Command::NONE);
    CHECK_TRUE(CommandName(Command::NONE) == nullptr);
}

TEST_CASE(every_menu_command_has_a_name) {
    for(Command cmd : GraphicsWindow::MenuCommands()) {
        const char *name = CommandName(cmd);
        CHECK_TRUE(name != nullptr);
        CHECK_TRUE(CommandFromName(name) == cmd);
    }
}

TEST_CASE(names_are_unique) {
    std::set<std::string> seen;
    for(Command cmd : GraphicsWindow::MenuCommands()) {
        const char *name = CommandName(cmd);
        CHECK_TRUE(name != nullptr);
        CHECK_TRUE(seen.insert(name).second);
    }
}

TEST_CASE(submenu_headers_are_not_invocable_commands) {
    // These menu rows only open submenus; ActivateCommand on them asserts.
    const Command headers[] = { Command::OPEN_RECENT, Command::GROUP_RECENT, Command::LOCALE };
    for(Command header : headers) {
        CHECK_TRUE(CommandName(header) == nullptr);
        for(Command cmd : GraphicsWindow::MenuCommands()) {
            CHECK_TRUE(cmd != header);
        }
    }
}
