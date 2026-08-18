#include "Window.hpp"

#include "../utils/init.hpp"
#include "Gui.hpp"
#include "Viewport.hpp"

void Window::init(int width, int height, char const * text) {

        Window::width = width;
        Window::height = height;
        Window::text = text;
        window = Utils::create_window(width, height, text);


    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, mouse_scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
}

void Window::framebuffer_size_callback(GLFWwindow * /*window*/, int width, int height) {
    Window::width = width;
    Window::height = height;
    // A teljes (új) framebufferre rajzolunk; az aspect-et a kamera ebből számolja.
    glViewport(0, 0, width, height);
}


void Window::destroy_window() {
    glfwDestroyWindow(window);
    glfwTerminate();
}


// A JELENET akkor kap bevitelt, ha az egér a 3D nézet fölött van (Vp::scene_mouse),
// illetve amíg nem egy UI-mezőbe gépelünk (Vp::scene_keyboard). Ezeket a main állítja
// be frame-enként.
//
// FIGYELEM, a szabály MEGFORDULT: amíg a jelenet a teljes ablakra rajzolódott, "a UI
// fölött ne reagáljon" volt az elv. Most a nézet MAGA IS egy ImGui-ablakban van, tehát
// a Gui::wants_mouse() épp a képen állva IGAZ — arra szűrve a kamera megnémulna.
//
// Az ELENGEDÉST viszont mindig tovább kell adni, különben beragad: ha a húzást a képen
// kívül engeded el, a RELEASE elveszne, és a kontrollpont örökre követné az egeret.
// Ugyanez a kamera Alt+bal gombjával és a shift állapotával.
static bool is_release(int action) {
    return action == GLFW_RELEASE;
}

// Egy eseménylista bejárása. Indexeléssel megy (nem tartomány-ciklussal), mert egy
// kezelő regisztrálhat vagy leiratkoztathat újat, ami a vektort újrafoglalhatja.
template<class V, class Info>
static void dispatch(V& slots, Info const& info) {
    for (std::size_t i = 0; i < slots.size(); ++i) {
        auto fn = slots[i].fn;          // másolat: leiratkozás közben is érvényes marad
        if (fn) fn(info);
    }
}

void Window::key_callback(GLFWwindow *window, int key, int scancode, int action, int mode) {
    if (!Vp::scene_keyboard() && !is_release(action)) return; // a UI épp gépel
    dispatch(key_events, KeyEventInformation{key, scancode, action, mode});
}

EventId Window::add_key_event(KeyEvent&& f) {
    key_events.push_back({next_id, std::move(f)});
    return next_id++;
}

void Window::mouse_pos_callback(GLFWwindow *window, double x, double y) {
    if (!Vp::scene_mouse()) return; // nem a 3D nézet fölött vagyunk
    dispatch(mouse_pos_events, MousePosEventInformation{x, y});
}

EventId Window::add_mouse_pos_event(MousePosEvent&& f) {
    mouse_pos_events.push_back({next_id, std::move(f)});
    return next_id++;
}

void Window::mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    if (!Vp::scene_mouse() && !is_release(action)) return; // a UI kapja a kattintást
    dispatch(mouse_button_events, MouseButtonEventInformation{button, action, mods});
}

EventId Window::add_mouse_button_event(MouseButtonEvent&& f) {
    mouse_button_events.push_back({next_id, std::move(f)});
    return next_id++;
}

void Window::mouse_scroll_callback(GLFWwindow *window, double x, double y) {
    if (!Vp::scene_mouse()) return; // a UI fölött görgetünk
    dispatch(mouse_scroll_events, MouseScrollEventInformation{x, y});
}

EventId Window::add_mouse_scroll_event(MouseScrollEvent&& f) {
    mouse_scroll_events.push_back({next_id, std::move(f)});
    return next_id++;
}

void Window::remove_event(EventId id) {
    if (!id) return;
    if (erase_from(key_events, id))          return;
    if (erase_from(mouse_pos_events, id))    return;
    if (erase_from(mouse_button_events, id)) return;
    if (erase_from(mouse_scroll_events, id)) return;
    erase_from(time_passed_events, id);
}

void Window::event_handling() {
    static double t = glfwGetTime();
    static double dt = 0;
    glfwSwapBuffers(window);
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (dt != 0.0)
        dispatch(time_passed_events, TimePassedEventInformation{t, dt});
    glfwPollEvents();

    double now = glfwGetTime();
    dt = now - t;
    t = now;
}

EventId Window::add_time_passed_event(TimePassedEvent &&f) {
    time_passed_events.push_back({next_id, std::move(f)});
    return next_id++;
}

MousePosEventInformation Window::get_mouse_info() {
    double x, y;
    glfwGetCursorPos(window, &x, &y);

    return {x, y};
}





