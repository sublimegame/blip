// VXImGuiTest.cpp

#include "VXImGuiTest.hpp"

#ifndef P3S_CLIENT_HEADLESS

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
// imgui+bgfx
#include <imgui/bgfx-imgui.h>
#include <dear-imgui/imgui.h>
#pragma GCC diagnostic pop

#endif

using namespace vx;

void vx::ShowWindowLayoutTest() {
#ifndef P3S_CLIENT_HEADLESS
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Window layout test")) {
        static float f = 0.0f;
        float wWidth = ImGui::GetWindowWidth();
        float wHeight = ImGui::GetWindowHeight();

        ImGui::TextWrapped("Long text using TextWrapped will automatically be wrapped to a new line if the window is not large enough");
        ImGui::NewLine();
        ImGui::Text("Half of window's width");
        ImGui::SetNextItemWidth(wWidth * 0.5f);
        ImGui::DragFloat("", &f);
        ImGui::Text("Horizontal spacing"); ImGui::SameLine(0, 50); ImGui::TextColored(ImVec4(1,1,0,1), "Colored");
        ImGui::NewLine();
        ImGui::Text("Aligned"); ImGui::SameLine(150); ImGui::Text("x=150");
        ImGui::Spacing(); ImGui::SameLine(wWidth * 0.1f); ImGui::Text("0.1f from left with no item");

        ImGui::SetCursorPos(ImVec2(wWidth * .4f, wHeight * .6f));
        ImGui::BeginGroup();
        ImGui::TextWrapped("Arbitrary relative cursor pos");
        ImGui::TextWrapped("Continue layout within group");
        ImGui::EndGroup();

        ImGui::NewLine();
        ImGui::Text("Text inputs (static buffers)");
        static char buff[64] = "";
        ImGui::InputText("text", buff, 64);
        static char bufpass[64] = "password123";
        ImGui::InputText("password", bufpass, 64, ImGuiInputTextFlags_Password | ImGuiInputTextFlags_CharsNoBlank);
    }
    ImGui::End();
#endif
}
