#include "solvespace.h"
#include "ui/typedfields.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static keymap::Chord C(const char *s) { return *keymap::ParseChord(s); }

TEST_CASE(typed_fields_collect_digits_and_cycle) {
    TypedFields f({"length", "angle"});
    CHECK_FALSE(f.Any());
    for(const char *k : {"2", "5", "Tab", "3", "Num0"}) CHECK_TRUE(f.HandleChord(C(k)));
    CHECK_EQ_STR(f.Text(0), "25");
    CHECK_EQ_STR(f.Text(1), "30");
    CHECK_TRUE(f.Active() == 1);
    CHECK_TRUE(f.HandleChord(C("Backspace")));
    CHECK_EQ_STR(f.Text(1), "3");
    CHECK_TRUE(f.HandleChord(C("Tab")));       // wraps
    CHECK_TRUE(f.Active() == 0);
    CHECK_TRUE(f.HandleChord(C("Shift+Tab"))); // back
    CHECK_TRUE(f.Active() == 1);
    CHECK_FALSE(f.HandleChord(C("L")));        // not ours
    CHECK_FALSE(f.HandleChord(C("Ctrl+2")));
    f.Clear();
    CHECK_FALSE(f.Any());
    CHECK_TRUE(f.Active() == 0);
}

TEST_CASE(typed_fields_parse_values) {
    TypedFields f({"d"});
    for(const char *k : {"-", "1", ".", "5"}) f.HandleChord(C(k));
    CHECK_TRUE(f.ValueRaw(0).has_value());
    CHECK_EQ_EPS(*f.ValueRaw(0), -1.5);
    f.Clear();
    for(const char *k : {"1", ".", ".", "2"}) f.HandleChord(C(k));
    CHECK_FALSE(f.ValueRaw(0).has_value()); // "1..2" is not a number
    f.Clear();
    CHECK_FALSE(f.ValueMm(0).has_value());  // empty
}

TEST_CASE(typed_fields_use_display_units) {
    Unit saved = SS.viewUnits;
    TypedFields f({"d"});
    f.HandleChord(C("1"));
    SS.viewUnits = Unit::MM;
    CHECK_EQ_EPS(*f.ValueMm(0), 1.0);
    SS.viewUnits = Unit::INCHES;
    CHECK_EQ_EPS(*f.ValueMm(0), 25.4);
    SS.viewUnits = saved;
}
