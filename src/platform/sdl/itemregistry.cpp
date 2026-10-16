//-----------------------------------------------------------------------------
// ImGui test-engine hook implementations; see itemregistry.h.
//-----------------------------------------------------------------------------
#include <unordered_map>
#include "imgui.h"
#include "imgui_internal.h"
#include "platform/sdl/itemregistry.h"

namespace SolveSpace {
namespace Platform {

static int                                 gFrame = -1;
static std::vector<UiItem>                 gItems;
static std::unordered_map<ImGuiID, ImRect> gRects;

// Items are recorded while a frame is built; a newer frame starts a new list.
static void StartFrameIfNew(ImGuiContext *ctx) {
    if(ctx->FrameCount == gFrame) return;
    gFrame = ctx->FrameCount;
    gItems.clear();
    gRects.clear();
}

const std::vector<UiItem> &LastFrameItems() {
    return gItems;
}

}
}

using namespace SolveSpace::Platform;

void ImGuiTestEngineHook_ItemAdd(ImGuiContext *ctx, ImGuiID id, const ImRect &bb,
                                 const ImGuiLastItemData *) {
    StartFrameIfNew(ctx);
    gRects[id] = bb;
}

void ImGuiTestEngineHook_ItemInfo(ImGuiContext *ctx, ImGuiID id, const char *label,
                                  ImGuiItemStatusFlags flags) {
    StartFrameIfNew(ctx);
    auto it = gRects.find(id);
    if(it == gRects.end() || label == nullptr) return;
    std::string text = label, itemId;
    size_t hash = text.find("##");
    if(hash != std::string::npos) {
        size_t start = text.find_first_not_of('#', hash);
        if(start != std::string::npos) itemId = text.substr(start);
        text.erase(hash);
    }
    UiItem item;
    item.label     = text;
    item.id        = itemId;
    item.window    = ctx->CurrentWindow ? ctx->CurrentWindow->Name : "";
    item.x0        = it->second.Min.x;
    item.y0        = it->second.Min.y;
    item.x1        = it->second.Max.x;
    item.y1        = it->second.Max.y;
    item.checkable = (flags & ImGuiItemStatusFlags_Checkable) != 0;
    item.checked   = (flags & ImGuiItemStatusFlags_Checked) != 0;
    item.opened    = (flags & ImGuiItemStatusFlags_Opened) != 0;
    item.isWindow  = ImGui::FindWindowByID(id) != nullptr;
    gItems.push_back(item);
}

void ImGuiTestEngineHook_Log(ImGuiContext *, const char *, ...) {}

const char *ImGuiTestEngine_FindItemDebugLabel(ImGuiContext *, ImGuiID) {
    return nullptr;
}
