#include "shadowmap_render.h"

#include "../../render/render_gui.h"

void SimpleShadowmapRender::SetupGUIElements()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
//    ImGui::ShowDemoWindow();
    ImGui::Begin("Simple render settings");

    ImGui::ColorEdit3("Meshes base color", m_uniforms.baseColor.M, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat3("Light source position", m_uniforms.lightPos.M, -10.f, 10.f);

    ImGui::SliderFloat("Light intencity", &m_uniforms.lightIntencity, 0.1f, 10.f, nullptr, ImGuiSliderFlags_Logarithmic);
    ImGui::RadioButton("None", reinterpret_cast<int*>(&m_uniforms.toneMappingMode), 0);
    ImGui::SameLine();
    ImGui::RadioButton("Uncharted 2", reinterpret_cast<int*>(&m_uniforms.toneMappingMode), 1);
    ImGui::SameLine();
    ImGui::RadioButton("Reinhard", reinterpret_cast<int*>(&m_uniforms.toneMappingMode), 2);
    ImGui::SameLine();
    ImGui::RadioButton("Exponential", reinterpret_cast<int*>(&m_uniforms.toneMappingMode), 3);

    if (m_uniforms.toneMappingMode == 3) {
          ImGui::SliderFloat("White level", &m_uniforms.whiteLevel, 0.1f, 3.f);
    }
    if (m_uniforms.toneMappingMode == 1 || m_uniforms.toneMappingMode == 2) {
          ImGui::SliderFloat("Exposure", &m_uniforms.exposure, 1.f, 3.f); // I'm not sure if it makes sense to change, but why not
    }

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
