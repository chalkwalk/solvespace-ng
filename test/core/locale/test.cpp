#include "ui.h"

#include "harness.h"

TEST_CASE(parseable) {
    for(auto locale : Locales()) {
        SetLocale(locale.Culture());
    }
    // Back to English: the tests run in a random order, and later ones read English messages.
    CHECK_TRUE(SetLocale("en-US"));
    CHECK_TRUE(true); // didn't crash
}
