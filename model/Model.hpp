#ifndef GORBE_MODEL_HPP
#define GORBE_MODEL_HPP

#include <glad/glad.h>
#include <glm.hpp>
#include <vector>
#include "Camera.hpp"

class Model {
private:
    GLuint VAO, VBO;
    GLuint shaderProgram;


protected:
    bool update_buffers_on_draw = true;
    // Ebben tároljuk a csúcspontokat. A leszármazottak tudják módosítani.
    std::vector<glm::vec3> vertices;

    // Transzformációs adatok
    glm::vec3 position;
    glm::vec3 scale;
    float rotationAngle;
    glm::vec3 rotationAxis;

    GLint get_uniform_location(const std::string& name) const;

    // Ezt kötelező megírni minden leszármazottnak! (Ettől absztrakt az osztály)
    virtual void render(Camera const &camera) = 0;

    // Ezt a függvényt hívja a leszármazott, ha feltöltötte vagy módosította a vektort
    void update_buffers() const;

public:
    Model();
    virtual ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    void set_shader(GLuint shader);

    // Transzformációs Setretek
    void set_position(const glm::vec3& pos);
    void set_scale(const glm::vec3& scl);
    void set_rotation(float angle_degrees, const glm::vec3& axis = glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 get_model_matrix() const;

    // A fő rajzoló függvény (Template Method)
    void draw(const Camera& camera);

    void set_uniform(const std::string& name, int value) const;
    void set_uniform(const std::string& name, float value) const;
    void set_uniform(const std::string& name, const glm::vec2& value) const;
    void set_uniform(const std::string& name, const glm::vec3& value) const;
    void set_uniform(const std::string& name, const glm::vec4& value) const;
    void set_uniform(const std::string& name, const glm::mat4& value) const;
};

#endif //GORBE_MODEL_HPP