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

    ImGui::SliderFloat3("Light source position", m_uniforms.lightPos.M, -10.f, 10.f);
    ImGui::Checkbox("Use RSM", reinterpret_cast<bool*>(&m_uniforms.enableRsm));
    if (m_uniforms.enableRsm) {
      int samples_count_int = m_uniforms.samplesCount;
      ImGui::SliderInt("Samples count", &samples_count_int, 0, m_maxSamples);
      m_uniforms.samplesCount = static_cast<shader_uint>(samples_count_int);
    }
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
