//
// Created by madam on 2026. 03. 07..
//

#include "Camera.hpp"

glm::mat4 Camera::get_matrix() const {
    return this->get_projection() * this->get_view();
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

Camera3D::Camera3D(glm::vec4 viewport, glm::vec3 start_position)
    : Camera(viewport), position(start_position), front(glm::vec3(0.0f, 0.0f, -1.0f)),
      world_up(glm::vec3(0.0f, 1.0f, 0.0f)), yaw(-90.0f), pitch(0.0f),
      movement_speed(0.1f), mouse_sensitivity(0.1f), fov(45.0f),
      first_mouse(true), last_x(viewport.z / 2.0f), last_y(viewport.w / 2.0f) {

    update_camera_vectors();

    // 1. Egér mozgás (Nézelődés)
    Window::add_mouse_pos_event([this](double xpos, double ypos) {
        this->process_mouse_movement(static_cast<float>(xpos), static_cast<float>(ypos));
    });

    // 2. Görgő (FOV / Zoom)
    Window::add_mouse_scroll_event([this](double xoffset, double yoffset) {
        this->process_mouse_scroll(static_cast<float>(yoffset));
    });

    // 3. Billentyűzet (Mozgás)
    Window::add_key_event([this](int key, int scancode, int action, int mode) {
        // A GLFW_REPEAT az operációs rendszer ismétlési sebességétől függ
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            this->process_keyboard(key);
        }
    });

    Window::disable_cursor();

    // FIGYELEM: Ehhez szükséged lesz egy kurzor pozíciót figyelő eseményre a Window osztályban!
    // Window::add_cursor_event([this](double xpos, double ypos) {
    //     this->process_mouse_movement(static_cast<float>(xpos), static_cast<float>(ypos));
    // });
}

glm::mat4 Camera3D::get_view() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera3D::get_projection() const {
    float aspect_ratio = (viewport.z - viewport.x) / (viewport.w - viewport.y);
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

void Camera3D::process_keyboard(int key) {
    if (key == GLFW_KEY_W) position += front * movement_speed;
    if (key == GLFW_KEY_S) position -= front * movement_speed;
    if (key == GLFW_KEY_A) position -= right * movement_speed;
    if (key == GLFW_KEY_D) position += right * movement_speed;
}

void Camera3D::process_mouse_movement(float xpos, float ypos) {
    if (first_mouse) {
        last_x = xpos;
        last_y = ypos;
        first_mouse = false;
    }

    float xoffset = xpos - last_x;
    float yoffset = last_y - ypos; // Fordítva van, mert az Y koordináták lentről felfelé nőnek 3D-ben
    last_x = xpos;
    last_y = ypos;

    xoffset *= mouse_sensitivity;
    yoffset *= mouse_sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Pitch korlátozása, hogy ne "forduljon át" a kamera (Gimbal lock elkerülése)
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    update_camera_vectors();
}

void Camera3D::process_mouse_scroll(float yoffset) {
    fov -= (float)yoffset;
    if (fov < 1.0f) fov = 1.0f;
    if (fov > 45.0f) fov = 45.0f;
}
