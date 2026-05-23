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
protected:
    glm::vec4 viewport;


    virtual glm::mat4 get_projection() const = 0;
    virtual glm::mat4 get_view() const = 0;;
public:
    Camera(glm::vec4 viewport) : viewport(viewport) {}
    glm::mat4 get_matrix() const;
    glm::vec3 get_mouse_pos_in_world() const;

    // Sugár–sík metszéspontja (3D drag). Alap implementáció: 2D fallback.
    virtual glm::vec3 get_mouse_pos_on_plane(glm::vec3 plane_point, glm::vec3 plane_normal) const;
    virtual glm::vec3 get_front() const { return glm::vec3(0.0f, 0.0f, -1.0f); }

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
    Camera2D(glm::vec4 viewport = glm::vec4{-10.0f, 10.0f, -10.0f, 10.0f})
    : Camera(viewport), offset(0), walls{glm::vec4{-10.0f, 10.0f, -10.0f, 10.0f}} {
        Window::add_mouse_scroll_event([this](auto p) {
            walls *= 1.0f - (0.1f * p.offsetY);
        });

        Window::add_key_event([this](auto p) {
            // Reagálunk a lenyomásra és a folyamatos nyomva tartásra is
            if (p.action == GLFW_PRESS || p.action == GLFW_REPEAT) {
                float speed = 0.2f;
                switch (p.key) {
                    case GLFW_KEY_W: offset.y += speed; break; // Fel
                    case GLFW_KEY_S: offset.y -= speed; break; // Le
                    case GLFW_KEY_A: offset.x -= speed; break; // Balra
                    case GLFW_KEY_D: offset.x += speed; break; // Jobbra
                }
            }
        });
    }

};

class Camera3D : public Camera {
private:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 world_up;

    // Euler-szögek a nézet irányításához
    float yaw;
    float pitch;

    // Kamera beállítások
    float movement_speed;
    float mouse_sensitivity;
    float fov;

    // Egér állapot
    float last_x;
    float last_y;
    bool first_mouse;
    bool right_mouse_down = false;
    bool left_mouse_down  = false;

    void update_camera_vectors();

protected:
    glm::mat4 get_projection() const override;
    glm::mat4 get_view() const override;

public:
    Camera3D(glm::vec4 viewport, glm::vec3 start_position = glm::vec3(0.0f, 0.0f, 3.0f),
             float initial_yaw = -90.0f, float initial_pitch = 0.0f);

    glm::vec3 get_front() const override { return front; }
    glm::vec3 get_mouse_pos_on_plane(glm::vec3 plane_point, glm::vec3 plane_normal) const override;

    void process_keyboard(int key);
    void process_mouse_movement(float xpos, float ypos);
    void process_mouse_scroll(float yoffset);
};


#endif //GORBE_CAMERA_HPP