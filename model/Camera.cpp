//
// Created by madam on 2026. 03. 07..
//

#include "Camera.hpp"

glm::mat4 Camera::get_matrix() const {
    return this->get_projection() * this->get_view();
}

glm::vec3 Camera::get_mouse_pos_in_world() const {
    // 1. Ablak méreteinek lekérése (a Window osztályból)
    int width = Window::get_width();
    int height = Window::get_height();

    // 2. Normalizált Eszközkoordináták (NDC) kiszámítása [-1.0, 1.0] tartományra
    // Figyelem: Az Y-tengelyt invertáljuk, mert a GLFW 0-ja fent van, az OpenGL 0-ja lent!
    auto minfo = Window::get_mouse_info();
    float x_ndc = (2.0f * static_cast<float>(minfo.x)) / width - 1.0f;
    float y_ndc = 1.0f - (2.0f * static_cast<float>(minfo.y)) / height;

    // 3. Inverz kamera mátrix
    glm::mat4 inverse_mat = glm::inverse(this->get_matrix());

    // 4. Visszavetítés a világba (Z = 0.0f a 2D síkhoz)
    glm::vec4 world_pos = inverse_mat * glm::vec4(x_ndc, y_ndc, 0.0f, 1.0f);

    // 5. Homogén osztás (Bár 2D ortografikus vetítésnél a 'w' általában 1 marad,
    // a 3D-s perspektivikus kamerádnál ez az osztás kötelező lesz!)
    if (world_pos.w != 0.0f) {
        world_pos /= world_pos.w;
    }

    return glm::vec3(world_pos.x, world_pos.y, world_pos.z);
}

glm::vec3 Camera::get_mouse_pos_on_plane(glm::vec3 /*plane_point*/, glm::vec3 /*plane_normal*/) const {
    return get_mouse_pos_in_world();
}

#include "Camera.hpp"
#include <gtc/matrix_transform.hpp> // Ez kell a glm::translate és glm::ortho függvényekhez

glm::mat4 Camera2D::get_view() const {
    return glm::translate(glm::mat4(1.0f), glm::vec3(-offset.x, -offset.y, 0.0f));
}

glm::mat4 Camera2D::get_projection() const {
    return glm::ortho(walls.x, walls.y, walls.z, walls.w, -1.0f, 1.0f);
}

#include "Camera.hpp"
#include <gtc/matrix_transform.hpp>

// --- Meglévő kódod itt van ---

// --- Camera3D implementáció ---

Camera3D::Camera3D(glm::vec4 viewport, glm::vec3 start_position, float initial_yaw, float initial_pitch)
    : Camera(viewport), position(start_position), front(glm::vec3(0.0f, 0.0f, -1.0f)),
      world_up(glm::vec3(0.0f, 1.0f, 0.0f)), yaw(initial_yaw), pitch(initial_pitch),
      movement_speed(5.0f), mouse_sensitivity(0.1f), fov(45.0f),
      first_mouse(true), last_x(viewport.z / 2.0f), last_y(viewport.w / 2.0f) {

    update_camera_vectors();

    // 1. Egér mozgás (csak jobb gomb lenyomva esetén forgat)
    subs.push_back(Window::Subscription(Window::add_mouse_pos_event([this](auto p) {
        this->process_mouse_movement(static_cast<float>(p.x), static_cast<float>(p.y));
    })));

    // 2. Egérgomb figyelése: jobb gomb VAGY Alt+bal gomb = forgás mód
    subs.push_back(Window::Subscription(Window::add_mouse_button_event([this](auto p) {
        if (p.button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (p.action == GLFW_PRESS) {
                right_mouse_down = true;
                first_mouse = true;
            } else if (p.action == GLFW_RELEASE) {
                right_mouse_down = false;
            }
        }
        if (p.button == GLFW_MOUSE_BUTTON_LEFT) {
            if (p.action == GLFW_PRESS && (p.mods & GLFW_MOD_ALT)) {
                left_mouse_down = true;
                first_mouse = true;
            } else if (p.action == GLFW_RELEASE) {
                left_mouse_down = false;
            }
        }
    })));

    // 3. Görgő (FOV / Zoom)
    subs.push_back(Window::Subscription(Window::add_mouse_scroll_event([this](auto p) {
        this->process_mouse_scroll(static_cast<float>(p.offsetY));
    })));

    // 4. Billentyűzet: csak az állapotot (held / nem held) jegyezzük fel.
    // A REPEAT-et szándékosan figyelmen kívül hagyjuk: a folyamatos mozgást
    // a frame-enkénti update() adja, nem az OS billentyű-ismétlése.
    subs.push_back(Window::Subscription(Window::add_key_event([this](auto p) {
        if (p.action == GLFW_PRESS)   this->process_keyboard(p.key, true);
        if (p.action == GLFW_RELEASE) this->process_keyboard(p.key, false);
        if ((p.key == GLFW_KEY_LEFT_ALT || p.key == GLFW_KEY_RIGHT_ALT)
                && p.action == GLFW_RELEASE) {
            left_mouse_down = false;
        }
    })));

    // 5. Frame-enkénti mozgatás a held billentyűk alapján, dt-vel skálázva.
    subs.push_back(Window::Subscription(Window::add_time_passed_event([this](auto p) {
        this->update(static_cast<float>(p.dt));
    })));

    // FIGYELEM: Ehhez szükséged lesz egy kurzor pozíciót figyelő eseményre a Window osztályban!
    // Window::add_cursor_event([this](double xpos, double ypos) {
    //     this->process_mouse_movement(static_cast<float>(xpos), static_cast<float>(ypos));
    // });
}

glm::mat4 Camera3D::get_view() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera3D::get_projection() const {
    // Az aspect-et az AKTUÁLIS ablakméretből számoljuk, így resize-kor (a glViewport
    // frissítésével együtt) a kép arányhelyes marad, nem nyúlik szét. (h==0: minimalizálva.)
    int h = Window::get_height();
    float aspect_ratio = (h > 0)
        ? static_cast<float>(Window::get_width()) / static_cast<float>(h)
        : 1.0f;
    return glm::perspective(glm::radians(fov), aspect_ratio, 0.1f, 100.0f);
}

void Camera3D::update_camera_vectors() {
    // Új front vektor kiszámítása az Euler-szögekből
    glm::vec3 new_front;
    new_front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    new_front.y = sin(glm::radians(pitch));
    new_front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(new_front);

    // Új right és up vektorok (a normalizálás fontos, mert ha felfelé/lefelé nézünk, a vektorok hossza változhat)
    right = glm::normalize(glm::cross(front, world_up));
    up = glm::normalize(glm::cross(right, front));
}

void Camera3D::process_keyboard(int key, bool pressed) {
    if (key == GLFW_KEY_W) move_forward  = pressed;
    if (key == GLFW_KEY_S) move_backward = pressed;
    if (key == GLFW_KEY_A) move_left     = pressed;
    if (key == GLFW_KEY_D) move_right    = pressed;
}

void Camera3D::update(float dt) {
    // movement_speed egység/másodpercben; dt-vel skálázva framerate-független
    // és sima mozgást ad, akármilyen az ablak frissítési rátája.
    float velocity = movement_speed * dt;
    if (move_forward)  position += front * velocity;
    if (move_backward) position -= front * velocity;
    if (move_left)     position -= right * velocity;
    if (move_right)    position += right * velocity;
}

void Camera3D::process_mouse_movement(float xpos, float ypos) {
    if (first_mouse) {
        last_x = xpos;
        last_y = ypos;
        first_mouse = false;
        return;
    }

    float xoffset = xpos - last_x;
    float yoffset = last_y - ypos;
    last_x = xpos;
    last_y = ypos;

    if (!right_mouse_down && !left_mouse_down) return;

    xoffset *= mouse_sensitivity;
    yoffset *= mouse_sensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    if (pitch >  89.0f) pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    update_camera_vectors();
}

void Camera3D::process_mouse_scroll(float yoffset) {
    fov -= static_cast<float>(yoffset);
    if (fov < 1.0f)  fov = 1.0f;
    if (fov > 45.0f) fov = 45.0f;
}

glm::vec3 Camera3D::get_mouse_pos_on_plane(glm::vec3 plane_point, glm::vec3 plane_normal) const {
    int width  = Window::get_width();
    int height = Window::get_height();
    auto minfo = Window::get_mouse_info();

    float x_ndc = (2.0f * static_cast<float>(minfo.x)) / static_cast<float>(width)  - 1.0f;
    float y_ndc = 1.0f - (2.0f * static_cast<float>(minfo.y)) / static_cast<float>(height);

    // NDC → eye space irány, majd → world space irány
    glm::vec4 ray_eye = glm::inverse(get_projection()) * glm::vec4(x_ndc, y_ndc, -1.0f, 1.0f);
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);
    glm::vec3 ray_dir = glm::normalize(glm::vec3(glm::inverse(get_view()) * ray_eye));

    // Sugár–sík metszés: position + t*ray_dir a síkon
    float denom = glm::dot(ray_dir, plane_normal);
    if (std::abs(denom) < 1e-6f) return plane_point;
    float t = glm::dot(plane_point - position, plane_normal) / denom;
    if (t < 0.0f) return plane_point;
    return position + t * ray_dir;
}
