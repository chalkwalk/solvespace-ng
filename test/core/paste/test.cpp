#include "solvespace.h"

#include "harness.h"

TEST_CASE(bad_paste_scale_is_refused) {
    SS.TW.shown.paste.scale = 2;
    SS.TW.edit.meaning      = TextWindow::Edit::PASTE_SCALE;
    SS.TW.EditControlDone("2*"); // not an expression: refused (it used to crash)
    CHECK_EQ_EPS(SS.TW.shown.paste.scale, 2);
    SS.TW.edit.meaning = TextWindow::Edit::PASTE_SCALE;
    SS.TW.EditControlDone("3");
    CHECK_EQ_EPS(SS.TW.shown.paste.scale, 3);
}
