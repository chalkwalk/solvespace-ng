// Theme, menu bar, keymap presets and files, and file / message dialogs.
#include "../uitest.h"
#include "ui/ngui.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static void WriteText(const Platform::Path &path, const std::string &text) {
    FILE *f = OpenFile(path, "wb");
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
}

TEST_CASE(the_palette_toggles_the_theme_and_remembers_it) {
    // No default key: Ctrl+Alt+T, the obvious one, opens a terminal on most Linux desktops.
    CHECK_LOAD("line.slvs");
    std::string before = Platform::GetSettings()->ThawString("NG_Theme", "dark");
    Driver::Key("Space");
    Driver::Type("toggle theme");
    Driver::Key("Return");
    std::string after = Platform::GetSettings()->ThawString("NG_Theme", "dark");
    CHECK_TRUE(before != after);
}

TEST_CASE(the_light_theme_lightens_the_drawing_area_too) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(SS.backgroundColor.Equals(RGBi(0, 0, 0)));
    Driver::Key("Ctrl+,");
    CHECK_TRUE(RevealIn("Preferences", "SolveSpace NG"));
    CHECK_TRUE(Driver::ClickItem("SolveSpace NG"));
    CHECK_TRUE(RevealIn("Preferences", "light theme"));
    CHECK_TRUE(Driver::ClickItem("light theme"));
    CHECK_TRUE(SS.backgroundColor.red > 200);
    // Saved, so a new file or the next start draws the same.
    Platform::SettingsRef settings = Platform::GetSettings();
    CHECK_TRUE(settings->ThawColor("BackgroundColor", RGBi(1, 2, 3)).Equals(SS.backgroundColor));
    CHECK_TRUE(settings->ThawColor(Style::CnfColor("ActiveGrp"), RGBi(1, 2, 3))
                   .Equals(Style::Color(hStyle{ Style::ACTIVE_GRP })));
    // And most of the window, which is mostly drawing area, is light.
    CHECK_TRUE(ClosePanel("Preferences"));
    Platform::Path shot = Driver::ConfigDir().Join("light.png");
    CHECK_TRUE(Driver::Screenshot(shot));
    std::shared_ptr<Pixmap> px = Pixmap::ReadPng(shot);
    CHECK_TRUE(px != nullptr);
    int light = 0, samples = 0;
    for(size_t y = 0; px && y < px->height; y += px->height / 10) {
        for(size_t x = 0; x < px->width; x += px->width / 10, samples++) {
            if(px->GetPixel(x, y).red > 200) light++;
        }
    }
    CHECK_TRUE(light * 10 > samples * 6);
    Driver::Key("Ctrl+,");
    CHECK_TRUE(RevealIn("Preferences", "dark theme"));
    CHECK_TRUE(Driver::ClickItem("dark theme"));
    CHECK_TRUE(SS.backgroundColor.Equals(RGBi(0, 0, 0)));
}

TEST_CASE(f10_hides_and_shows_the_menu_bar) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::FindItem("Sketch").has_value());
    Driver::Key("F10");
    CHECK_FALSE(Driver::FindItem("Sketch").has_value());
    Driver::Key("F10");
    CHECK_TRUE(Driver::FindItem("Sketch").has_value());
}

TEST_CASE(classic_preset_uses_solvespace_keys) {
    CHECK_LOAD("line.slvs");
    Platform::GetSettings()->FreezeString("NG_KeymapPreset", "classic");
    Driver::ReloadKeymap();
    Driver::Key("S"); // Classic: S is Line Segment
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
    Driver::Key("Escape");
    Driver::Key("Space");
    CHECK_FALSE(Ui::CapturesText());
    Driver::Key("Ctrl+K");
    CHECK_TRUE(Ui::CapturesText());
}

TEST_CASE(user_keymap_file_rebinds) {
    CHECK_LOAD("line.slvs");
    WriteText(Driver::ConfigDir().Join("keymap.txt"), "line-segment: K\n");
    Driver::ReloadKeymap();
    Driver::Key("K");
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
}

TEST_CASE(broken_keymap_file_reports_and_falls_back) {
    CHECK_LOAD("line.slvs");
    WriteText(Driver::ConfigDir().Join("keymap.txt"), "line-segment: Ctrl+Ctrl+Q\n");
    Driver::ReloadKeymap();
    CHECK_TRUE(Driver::Dialogs().size() == 1);
    CHECK_TRUE(Driver::Dialogs()[0].message.find("line 1") != std::string::npos);
    Driver::Key("L"); // the preset still works
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
}

TEST_CASE(ctrl_s_saves_through_the_file_dialog) {
    CHECK_LOAD("line.slvs");
    DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    Platform::Path out = Driver::ConfigDir().Join("saved.slvs");
    SS.saveFile.Clear();
    Driver::QueueFile(out);
    Driver::Key("Ctrl+S");
    CHECK_TRUE(Driver::Dialogs().size() == 1 && Driver::Dialogs()[0].kind == "save");
    FILE *f = OpenFile(out, "rb");
    CHECK_TRUE(f != nullptr);
    if(f) fclose(f);
    CHECK_FALSE(SS.unsaved);
}

TEST_CASE(ctrl_n_asks_to_save_changes) {
    CHECK_LOAD("line.slvs");
    DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    CHECK_TRUE(SS.unsaved);
    Driver::OnMessage([](const Driver::DialogRecord &) {
        return Platform::MessageDialog::Response::NO; // don't save
    });
    Driver::Key("Ctrl+N");
    CHECK_TRUE(Driver::Dialogs().size() == 1 && Driver::Dialogs()[0].kind == "message");
    CHECK_FALSE(SS.unsaved);
    CHECK_TRUE(SK.request.n < 4); // a fresh file
}

TEST_CASE(ctrl_o_opens_the_chosen_file) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    Platform::Path out = Driver::ConfigDir().Join("two-lines.slvs");
    CHECK_TRUE(SS.SaveToFile(out));
    int requests = SK.request.n;
    Driver::OnMessage([](const Driver::DialogRecord &) {
        return Platform::MessageDialog::Response::NO; // don't save
    });
    Driver::Key("Ctrl+N");
    Driver::Dialogs().clear();
    Driver::QueueFile(out);
    Driver::Key("Ctrl+O");
    CHECK_TRUE(Driver::Dialogs().size() == 1 && Driver::Dialogs()[0].kind == "open");
    CHECK_TRUE(SK.request.n == requests);
    CHECK_TRUE(SK.request.FindByIdNoOops(line) != nullptr);
}
