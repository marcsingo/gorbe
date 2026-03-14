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
