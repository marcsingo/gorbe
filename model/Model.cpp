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

void Model::update_buffers() {
    if (vertices.empty()) return;

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // sizeof(glm::vec3) pontosan 3 db float mérete (12 bájt), így biztonságos.
    GLsizeiptr const bytes = static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));

    // A részecske-modellek MINDEN frame-ben újratöltik a csúcsokat, ezért
    // GL_DYNAMIC_DRAW (a GL_STATIC_DRAW ennek pont az ellenkezőjét ígérte a
    // drivernek). Amíg belefér a már lefoglalt bufferbe, csak felülírjuk —
    // így nincs frame-enkénti újrafoglalás.
    if (bytes > vbo_capacity) {
        glBufferData(GL_ARRAY_BUFFER, bytes, vertices.data(), GL_DYNAMIC_DRAW);
        vbo_capacity = bytes;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, vertices.data());
    }

    // 0. index: pozíció (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // VAO intentionally left bound so render() can safely add more attributes after this call
}

void Model::set_shader(GLuint shader) {
    shaderProgram = shader;
    uniform_cache.clear();   // a helyek programonként mások
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
// A talált (és a hiányzó) helyeket is gyorsítótárazzuk, így a string szerinti
// keresés és az esetleges hibaüzenet is legfeljebb EGYSZER fut le nevenként —
// nem frame-enként, ahogy korábban.
GLint Model::get_uniform_location(const std::string& name) const {
    if (shaderProgram == 0) {
        std::cerr << "Hiba: Nincs shader beallitva a modellhez!" << std::endl;
        return -1;
    }

    auto it = uniform_cache.find(name);
    if (it != uniform_cache.end()) return it->second;

    GLint location = glGetUniformLocation(shaderProgram, name.c_str());
    if (location == -1) {
        std::cerr << "Hiba: nem talalhato ez a uniform: " << name << std::endl;
    }
    uniform_cache.emplace(name, location);
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