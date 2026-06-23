#include "Gui.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

void Gui::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // ne írjon imgui.ini-t a munkakönyvtárba
    ImGui::StyleColorsDark();

    // install_callbacks = true: az ImGui rálicncolódik a Window meglévő GLFW
    // callbackjeire (a sajátja után meghívja azokat is).
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330"); // a projekt GL 3.3 core-t használ
}

void Gui::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Gui::end_frame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Gui::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Gui::demo_panel() {
    ImGui::Begin("Dear ImGui");
    ImGui::Text("Az ImGui be van kotve es mukodik.");
    ImGui::Text("Sajat panelt az App::set_gui(...)-val adhatsz hozza.");
    static bool show_demo = false;
    ImGui::Checkbox("ImGui demo ablak", &show_demo);
    if (show_demo) ImGui::ShowDemoWindow(&show_demo);
    ImGui::End();
}

bool Gui::wants_mouse()    { return ImGui::GetIO().WantCaptureMouse; }
bool Gui::wants_keyboard() { return ImGui::GetIO().WantCaptureKeyboard; }