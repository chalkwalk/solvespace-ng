//-----------------------------------------------------------------------------
// The tool shelf panel.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "commandnames.h"
#include "platform/sdl/icontextures.h"
#include "ui/icons.h"
#include "ui/panels.h"
#include "ui/shelf.h"
#include "ui/shelfmodel.h"

namespace SolveSpace {
namespace Ui {

static int gTextFallbacks = 0;

int ShelfTextFallbacks() {
    return gTextFallbacks;
}

std::function<void()> DrawShelf(const keymap::Keymap &keymap) {
    Panel &panel = ShelfPanel();
    panel.SetShown(SS.showToolbar);
    if(!panel.Shown()) return nullptr;
    std::function<void()> run;
    if(panel.Begin()) {
        ImGuiIO    &io    = ImGui::GetIO();
        ImGuiStyle &st    = ImGui::GetStyle();
        // Drawn at the framebuffer's resolution, so icons stay crisp on HiDPI screens.
        float       scale = std::max(1.0f, io.DisplayFramebufferScale.x);
        const float icon  = 22;
        IconPalette palette = CurrentIconPalette();
        int   fallbacks = 0;
        const float spacing = 6;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
        std::vector<ShelfSection> sections = BuildShelf(keymap);
        // A column on the left (owner review 4, layout c), two buttons wide, each section
        // starting a row of its own; a third column when the window is too short for two.
        float rowHeight = icon + st.FramePadding.y * 2 + spacing;
        float room = io.DisplaySize.y - (ImGui::GetFrameHeight() + 16) - 52 -
                     ImGui::GetFrameHeight() - st.WindowPadding.y * 2;
        int columns = 2;
        for(;; columns++) {
            int rows = 0;
            for(const ShelfSection &sec : sections) {
                rows += ((int)sec.items.size() + columns - 1) / columns;
            }
            float height = rows * rowHeight + (sections.size() - 1) * spacing;
            if(height <= room || columns >= 6) break;
        }
        for(size_t si = 0; si < sections.size(); si++) {
            if(si > 0) ImGui::Dummy(ImVec2(0, 0)); // a little more space between sections
            for(size_t ii = 0; ii < sections[si].items.size(); ii++) {
                const ShelfItem &item = sections[si].items[ii];
                std::string      id    = std::string("##shelf-") + CommandName(item.command);
                std::string      label = item.label + id;
                ImTextureID tex =
                    Platform::IconTexture(item.icon, (int)(icon * scale + 0.5f), palette);
                if(ii % columns != 0) ImGui::SameLine(0, spacing);
                bool clicked;
                if(tex) {
                    clicked = ImGui::ImageButton(id.c_str(), tex, ImVec2(icon, icon), ImVec2(0, 0),
                                                 ImVec2(1, 1));
                    // Image buttons report no label; give the UI tests one to find.
                    ImGuiContext &g = *GImGui;
                    (void)g;
                    IMGUI_TEST_ENGINE_ITEM_INFO(ImGui::GetItemID(), label.c_str(),
                                                ImGui::GetItemStatusFlags());
                } else {
                    clicked = ImGui::Button(label.c_str());
                    fallbacks++;
                }
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                    std::string tip = item.label;
                    if(!item.keys.empty()) tip += "    " + item.keys;
                    ImGui::SetTooltip("%s", tip.c_str());
                }
                if(clicked) {
                    Command c = item.command;
                    run = [c] { SS.GW.ActivateCommand(c); };
                }
            }
        }
        ImGui::PopStyleVar();
        gTextFallbacks = fallbacks;
        panel.End();
    }
    if(!panel.Shown()) {
        // Closed with its title-bar button: as View > Show Toolbar would.
        run = [] {
            SS.showToolbar = false;
            SS.GW.EnsureValidActives();
        };
    }
    return run;
}

}
}
