#include "solvespace.h"
#include "ui/iconraster.h"
#include "harness.h"

static const char *Circle =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\""
    " fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><circle cx=\"12\" cy=\"12\""
    " r=\"10\"/></svg>";

static uint8_t Alpha(const SolveSpace::Ui::IconImage &im, int x, int y) {
    return im.rgba[(y * im.width + x) * 4 + 3];
}

TEST_CASE(icon_is_white_on_transparent) {
    auto im = SolveSpace::Ui::RasterizeIcon(Circle, 24);
    CHECK_TRUE(im && im->width == 24 && im->height == 24);
    if(!im) return;
    CHECK_TRUE(Alpha(*im, 12, 2) > 128);           // on the stroke, top of the circle
    CHECK_TRUE(im->rgba[(2 * 24 + 12) * 4] > 200); // white, not black
    CHECK_TRUE(Alpha(*im, 12, 12) == 0);           // the middle is empty
}

TEST_CASE(icon_scales_crisply) {
    auto im = SolveSpace::Ui::RasterizeIcon(Circle, 48); // a 2x display
    CHECK_TRUE(im && im->width == 48);
    if(!im) return;
    CHECK_TRUE(Alpha(*im, 24, 4) > 128);
    CHECK_TRUE(Alpha(*im, 24, 24) == 0);
}

TEST_CASE(broken_svg_gives_nothing) {
    CHECK_FALSE(SolveSpace::Ui::RasterizeIcon("not an svg", 24).has_value());
    CHECK_FALSE(SolveSpace::Ui::RasterizeIcon(Circle, 0).has_value());
}
