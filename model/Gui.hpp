//
// Dear ImGui wrapper. A teljes ImGui-életciklust elrejti, hogy a render loopban és
// a Window-ban ne kelljen közvetlenül az ImGui fejléceket includolni.
//

#ifndef GORBE_GUI_HPP
#define GORBE_GUI_HPP

struct GLFWwindow;

class Gui {
public:
    // ImGui kontextus + GLFW/OpenGL3 backend. A Window callbackjei UTÁN kell hívni,
    // hogy az ImGui rájuk tudjon láncolódni (install_callbacks).
    static void init(GLFWwindow* window);

    static void begin_frame();   // új ImGui frame (NewFrame)
    static void end_frame();     // ImGui::Render + kirajzolás a jelenet fölé
    static void shutdown();

    // Egy egyszerű alap-panel, ami bizonyítja, hogy az ImGui be van kötve.
    // Saját UI-hoz: App::set_gui(...).
    static void demo_panel();

    // Igaz, ha épp az ImGui használja az egeret/billentyűzetet (UI fölött vagyunk).
    // A Window ezzel szűri, hogy a kamera/kontrollpontok ne reagáljanak a UI-ra.
    static bool wants_mouse();
    static bool wants_keyboard();
};

#endif //GORBE_GUI_HPP