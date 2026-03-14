//
// Created by madam on 2026. 03. 07..
//

#ifndef GORBE_CAMERA_HPP
#define GORBE_CAMERA_HPP

#include <glm.hpp>
#include <iostream>
#include <ostream>

#include "Window.hpp"

class Camera {
    glm::vec4 viewport;

protected:
    virtual glm::mat4 get_projection() const = 0;
    virtual glm::mat4 get_view() const = 0;;
public:
    Camera(glm::vec4 viewport) : viewport(viewport) {}
    glm::mat4 get_matrix() const;
    virtual ~Camera() = default;
};

class Camera2D : public Camera {
protected:
    glm::mat4 get_projection() const;
    glm::mat4 get_view() const;
public:
    glm::vec2 offset;
    //left, right, bottom, top
    glm::vec4 walls;
    Camera2D(glm::vec4 viewport) : Camera(viewport), offset(0), walls(0) {
        Window::add_mouse_scroll_event([this](auto x, auto y) {
            walls *= 1.0f - (0.1f * y);
        });

        Window::add_key_event([this](int key, int scancode, int action, int mode) {
            // Reagálunk a lenyomásra és a folyamatos nyomva tartásra is
            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                float speed = 0.2f;
                switch (key) {
                    case GLFW_KEY_W: offset.y += speed; break; // Fel
                    case GLFW_KEY_S: offset.y -= speed; break; // Le
                    case GLFW_KEY_A: offset.x -= speed; break; // Balra
                    case GLFW_KEY_D: offset.x += speed; break; // Jobbra
                }
            }
        });
    }

};


#endif //GORBE_CAMERA_HPP