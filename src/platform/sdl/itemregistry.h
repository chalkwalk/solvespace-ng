//-----------------------------------------------------------------------------
// Records the label and rectangle of every ImGui item drawn in the last frame, through ImGui's
// test-engine hooks (IMGUI_ENABLE_TEST_ENGINE; the hooks are part of ImGui core, the separate
// Test Engine is not used). Only active while ImGuiContext::TestEngineHookItems is set, which
// the test driver does; tests click menus and buttons by name with it.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_SDL_ITEMREGISTRY_H
#define SOLVESPACE_SDL_ITEMREGISTRY_H

#include <string>
#include <vector>

namespace SolveSpace {
namespace Platform {

struct UiItem {
    std::string label;  // without any "##id" suffix
    std::string id;     // the "##id" suffix without the hashes, e.g. "shown:sketch"
    std::string window; // the ImGui window it was drawn in
    float       x0, y0, x1, y1;
    bool        checkable, checked, opened;
    bool        isWindow;
};

// Items of the most recent frame, in drawing order.
const std::vector<UiItem> &LastFrameItems();

}
}

#endif
