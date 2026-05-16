#include "Model.hpp"
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <iostream>

Model::Model()
    : shaderProgram(0), position(0.0f), scale(1.0f), rotationAngle(0.0f), rotationAxis(0.0f, 0.0f, 1.0f)
{
    // A konstruktorban CSAK az azonosítókat foglaljuk le!
    // Adatot nem töltünk fel, hiszen a 'vertices' vektor itt még teljesen üres.
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
}

Model::~Model() {
    // RAII elv: amikor a C++ objektum megszűnik, takarítunk a GPU-n is
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void Model::update_buffers() const {
    if (vertices.empty()) return;

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // sizeof(glm::vec3) pontosan 3 db float mérete (12 bájt), így biztonságos.
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);

    // 0. index: pozíció (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // VAO intentionally left bound so render() can safely add more attributes after this call
}

void Model::set_shader(GLuint shader) {
    shaderProgram = shader;
}

void Model::set_position(const glm::vec3& pos) { position = pos; }
void Model::set_scale(const glm::vec3& scl) { scale = scl; }
void Model::set_rotation(float angle_degrees, const glm::vec3& axis) {
    rotationAngle = angle_degrees;
    rotationAxis = axis;
}

glm::mat4 Model::get_model_matrix() const {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotationAngle), rotationAxis);
    model = glm::scale(model, scale);
    return model;
}

// Figyelem: A 'const' jelölést eltávolítottuk, hogy egyezzen a Model.hpp deklarációval!
void Model::draw(const Camera& camera)  {
    if (this->update_buffers_on_draw) update_buffers();
    if (shaderProgram == 0 ) return;
    // 1. Állapotok beállítása (Shader + Mátrixok)
    glUseProgram(shaderProgram);

    glm::mat4 MVP = camera.get_matrix() * get_model_matrix();
    int mvpLocation = glGetUniformLocation(shaderProgram, "u_MVP");
    if (mvpLocation != -1) {
        glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, glm::value_ptr(MVP));
    } else {
        // Opcionális figyelmeztetés fejlesztés közben
        // std::cerr << "Figyelmeztetes: 'u_MVP' uniform nem talalhato a shaderben!" << std::endl;
    }

    // 2. Geometria aktiválása
    glBindVertexArray(VAO);

    // if (update_buffers_on_draw) update_buffers();
    // 3. A tényleges kirajzolás delegálása a leszármazottnak!
    this->render(camera);

    // 4. Takarítás
    glBindVertexArray(0);
}

// --- Segédfüggvény a hely lekérdezésére és a hibaüzenetre ---
GLint Model::get_uniform_location(const std::string& name) const {
    if (shaderProgram == 0) {
        std::cerr << "Hiba: Nincs shader beallitva a modellhez!" << std::endl;
        return -1;
    }

    GLint location = glGetUniformLocation(shaderProgram, name.c_str());
    if (location == -1) {
        // Pontosan a kért hibaüzenet formátum
        std::cerr << "Hiba: nem talalhato ez a uniform: " << name << std::endl;
    }
    return location;
}

// --- Uniform Setters ---

void Model::set_uniform(const std::string& name, int value) const {
    glUseProgram(shaderProgram); // Kötelező, mielőtt uniformot állítasz!
    GLint location = get_uniform_location(name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void Model::set_uniform(const std::string& name, float value) const {
    glUseProgram(shaderProgram);
    GLint location = get_uniform_location(name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void Model::set_uniform(const std::string& name, const glm::vec2& value) const {
    glUseProgram(shaderProgram);
    GLint location = get_uniform_location(name);
    if (location != -1) {
        glUniform2fv(location, 1, glm::value_ptr(value));
    }
}

void Model::set_uniform(const std::string& name, const glm::vec3& value) const {
    glUseProgram(shaderProgram);
    GLint location = get_uniform_location(name);
    if (location != -1) {
        // A value_ptr-hez kell a <gtc/type_ptr.hpp>, ami nálad már be van húzva
        glUniform3fv(location, 1, glm::value_ptr(value));
    }
}

void Model::set_uniform(const std::string& name, const glm::vec4& value) const {
    glUseProgram(shaderProgram);
    GLint location = get_uniform_location(name);
    if (location != -1) {
        glUniform4fv(location, 1, glm::value_ptr(value));
    }
}

void Model::set_uniform(const std::string& name, const glm::mat4& value) const {
    glUseProgram(shaderProgram);
    GLint location = get_uniform_location(name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
    }
}