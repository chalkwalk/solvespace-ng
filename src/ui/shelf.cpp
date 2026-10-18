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
        // Rows wrap at this width, so a narrow window still shows every button.
        float maxWidth = std::min(560.0f, io.DisplaySize.x - 24) - st.WindowPadding.x * 2;
        float rowWidth = 0;
        int   fallbacks = 0;
        const float spacing = 6;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
        std::vector<ShelfSection> sections = BuildShelf(keymap);
        auto buttonWidth = [&](const ShelfItem &item, ImTextureID tex) {
            return tex ? icon + st.FramePadding.x * 2
                       : ImGui::CalcTextSize(item.label.c_str()).x + st.FramePadding.x * 2;
        };
        for(size_t si = 0; si < sections.size(); si++) {
            // A section that doesn't fit whole on this row starts the next one.
            float sectionWidth = 0;
            for(const ShelfItem &item : sections[si].items) {
                if(sectionWidth > 0) sectionWidth += spacing;
                sectionWidth += buttonWidth(item, Platform::IconTexture(
                                                      item.icon, (int)(icon * scale + 0.5f), palette));
            }
            bool newRow = rowWidth > 0 && rowWidth + spacing + 12 + sectionWidth > maxWidth;
            for(size_t ii = 0; ii < sections[si].items.size(); ii++) {
                const ShelfItem &item = sections[si].items[ii];
                std::string      id    = std::string("##shelf-") + CommandName(item.command);
                std::string      label = item.label + id;
                ImTextureID tex =
                    Platform::IconTexture(item.icon, (int)(icon * scale + 0.5f), palette);
                float width = buttonWidth(item, tex);
                // A wider gap between sections.
                float gap = spacing + (ii == 0 && si > 0 ? 12 : 0);
                if(ii == 0 && newRow) {
                    rowWidth = 0;
                } else if(rowWidth > 0 && rowWidth + gap + width <= maxWidth) {
                    ImGui::SameLine(0, gap);
                    rowWidth += gap;
                } else {
                    rowWidth = 0;
                }
                rowWidth += width;
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
