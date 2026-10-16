#include "harness.h"
#include "keymap.h"

using keymap::Chord;
using keymap::Keymap;

TEST_CASE(parse_and_format_chords) {
    auto c = keymap::ParseChord("Ctrl+Shift+S");
    CHECK_TRUE(c.has_value());
    CHECK_EQ_STR(c->key, "s");
    CHECK_TRUE(c->ctrl && c->shift && !c->alt);
    CHECK_EQ_STR(keymap::FormatChord(*c), "Ctrl+Shift+S");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord("shift+ctrl+s")), "Ctrl+Shift+S");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord("Plus")), "Plus");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord(">")), ">");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord("Ctrl+,")), "Ctrl+,");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord("Num5")), "Num5");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord("alt+a")), "Alt+A");
}

TEST_CASE(reject_bad_chords) {
    CHECK_FALSE(keymap::ParseChord("").has_value());
    CHECK_FALSE(keymap::ParseChord("Ctrl+").has_value());
    CHECK_FALSE(keymap::ParseChord("Hyper+A").has_value());
    CHECK_FALSE(keymap::ParseChord("Ctrl+Ctrl+A").has_value());
    CHECK_FALSE(keymap::ParseChord("NotAKey").has_value());
}

TEST_CASE(bind_lookup_and_conflicts) {
    Keymap km;
    std::string err;
    CHECK_TRUE(km.Bind("save", *keymap::ParseChord("Ctrl+S"), &err));
    CHECK_FALSE(km.Bind("save-as", *keymap::ParseChord("Ctrl+S"), &err));
    CHECK_TRUE(err.find("Ctrl+S") != std::string::npos);
    CHECK_TRUE(err.find("save") != std::string::npos);
    const std::string *cmd = km.Lookup(*keymap::ParseChord("Ctrl+S"));
    CHECK_TRUE(cmd != nullptr && *cmd == "save");
    CHECK_TRUE(km.Lookup(*keymap::ParseChord("Ctrl+Q")) == nullptr);
    CHECK_TRUE(km.Bind("save", *keymap::ParseChord("F2"), &err));
    CHECK_TRUE(km.ChordsFor("save").size() == 2);
}

TEST_CASE(parse_file_round_trip) {
    std::string err;
    auto km = Keymap::Parse("# comment\n\nsave: Ctrl+S, F2\nundo: Ctrl+Z\n", &err);
    CHECK_TRUE(km.has_value());
    auto again = Keymap::Parse(km->Serialize(), &err);
    CHECK_TRUE(again.has_value());
    CHECK_EQ_STR(again->Serialize(), km->Serialize());
    CHECK_TRUE(again->ChordsFor("save").size() == 2);
}

TEST_CASE(crlf_and_comments_ok) {
    std::string err;
    auto km = Keymap::Parse("undo: Ctrl+Z   # trailing\r\nredo: Ctrl+Shift+Z\r\n", &err);
    CHECK_TRUE(km.has_value());
    CHECK_TRUE(*km->Lookup(*keymap::ParseChord("Ctrl+Shift+Z")) == "redo");
}

TEST_CASE(parse_errors_report_line) {
    std::string err;
    CHECK_FALSE(Keymap::Parse("undo: Ctrl+Z\nbroken line\n", &err).has_value());
    CHECK_TRUE(err.find("line 2") != std::string::npos);
    CHECK_FALSE(Keymap::Parse("a: Ctrl+Z\nb: Ctrl+Z\n", &err).has_value());
    CHECK_TRUE(err.find("line 2") != std::string::npos);
    CHECK_FALSE(Keymap::Parse("a: Bogus+Q\n", &err).has_value());
    CHECK_TRUE(err.find("line 1") != std::string::npos);
    auto empty = Keymap::Parse("", &err);
    CHECK_TRUE(empty.has_value() && empty->Commands().empty());
}

TEST_CASE(comma_key_in_file) {
    std::string err;
    auto km = Keymap::Parse("configuration: Ctrl+,\nother: F2, Alt+,\nplain: ,\n", &err);
    CHECK_TRUE(km.has_value());
    CHECK_TRUE(*km->Lookup(*keymap::ParseChord("Ctrl+,")) == "configuration");
    CHECK_TRUE(*km->Lookup(*keymap::ParseChord("Alt+,")) == "other");
    CHECK_TRUE(km->ChordsFor("other").size() == 2);
    CHECK_TRUE(*km->Lookup(*keymap::ParseChord(",")) == "plain");
    auto again = Keymap::Parse(km->Serialize(), &err);
    CHECK_TRUE(again.has_value());
}

TEST_CASE(parse_with_command_validator) {
    std::string err;
    auto known = [](const std::string &name) { return name == "undo" || name == "redo"; };
    CHECK_TRUE(Keymap::Parse("undo: Ctrl+Z\nredo: Ctrl+Y\n", &err, known).has_value());
    CHECK_FALSE(Keymap::Parse("undo: Ctrl+Z\nreddo: Ctrl+Y\n", &err, known).has_value());
    CHECK_TRUE(err.find("line 2") != std::string::npos);
    CHECK_TRUE(err.find("reddo") != std::string::npos);
}
