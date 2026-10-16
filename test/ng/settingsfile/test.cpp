#include <filesystem>
#include "solvespace.h"
#include "platform/sdl/settingsfile.h"

#include "harness.h"

using namespace SolveSpace::Platform;

static Path TempSettingsPath(const char *name) {
    return Path::From(std::filesystem::temp_directory_path().string())
               .Join(std::string("ssng-test-") + name + ".txt");
}

TEST_CASE(round_trip_all_types) {
    Path path = TempSettingsPath("roundtrip");
    remove(path.raw.c_str());
    {
        SettingsFile s(path);
        s.FreezeInt("Int", 42);
        s.FreezeFloat("Float", 0.1);
        s.FreezeString("Str", "a=b\nc\\d");
        s.FreezeBool("Bool", true);
    }
    SettingsFile s(path);
    CHECK_TRUE(s.ThawInt("Int") == 42);
    CHECK_EQ_EPS(s.ThawFloat("Float"), 0.1);
    CHECK_EQ_STR(s.ThawString("Str"), "a=b\nc\\d");
    CHECK_TRUE(s.ThawBool("Bool"));
    CHECK_TRUE(s.ThawInt("Missing", 7) == 7);
    remove(path.raw.c_str());
}

TEST_CASE(garbage_lines_are_ignored) {
    Path path = TempSettingsPath("garbage");
    CHECK_TRUE(WriteFile(path, "no equals sign\nInt=notanumber\nGood=5\n"));
    SettingsFile s(path);
    CHECK_TRUE(s.ThawInt("Int", 3) == 3);
    CHECK_TRUE(s.ThawInt("Good") == 5);
    remove(path.raw.c_str());
}
