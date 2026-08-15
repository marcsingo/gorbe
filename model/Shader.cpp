#include "Shader.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

std::string Builder::ShaderBuilder::read_from_file(const char* fileName) {
    std::ifstream file(fileName);
    if (!file) {
        std::cerr << "Can't open file " << fileName << std::endl;
        exit(EXIT_FAILURE);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

Builder::ShaderBuilder &Builder::ShaderBuilder::add_vertex_shader(const char *fileName) {
    this->vertexShader.push_back(read_from_file(fileName));
    return *this;
}

Builder::ShaderBuilder &Builder::ShaderBuilder::add_fragment_shader(const char *fileName) {
    this->fragmentShader.push_back(read_from_file(fileName));
    return *this;
}

Builder::ShaderBuilder &Builder::ShaderBuilder::add_vertex_shader(std::string & text) {
    this->vertexShader.push_back(text);
    return *this;
}

Builder::ShaderBuilder &Builder::ShaderBuilder::add_fragment_shader(std::string & text) {
    this->fragmentShader.push_back(text);
    return *this;
}

GLuint Builder::ShaderBuilder::compile_shader(GLenum type, std::vector<std::string> &datas) {
    if (datas.empty()) return 0;
    std::vector<char const *> c_strings{};
    for (auto& str: datas) c_strings.push_back(str.c_str());

    GLuint shader_id = glCreateShader(type);
    glShaderSource(shader_id, c_strings.size(), c_strings.data(), NULL);
    glCompileShader(shader_id);

    int success;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader_id, 512, nullptr, infoLog);
        std::string typeStr = (type == GL_VERTEX_SHADER)   ? "VERTEX"
                            : (type == GL_GEOMETRY_SHADER) ? "GEOMETRY"
                                                           : "FRAGMENT";
        std::cerr << "HIBA::SHADER::" << typeStr << "::FORDITASI_HIBA\n" << infoLog << std::endl;
    }

    return shader_id;
}

Builder::ShaderBuilder& Builder::ShaderBuilder::add_geometry_shader(const char* fileName) {
    this->geometryShader.push_back(read_from_file(fileName));
    return *this;
}

GLuint Builder::ShaderBuilder::build() {
    GLuint vertexShader   = compile_shader(GL_VERTEX_SHADER,   this->vertexShader);
    GLuint geometryShader = compile_shader(GL_GEOMETRY_SHADER, this->geometryShader);
    GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, this->fragmentShader);

    GLuint shaderProgram = glCreateProgram();

    if (vertexShader)   glAttachShader(shaderProgram, vertexShader);
    if (geometryShader) glAttachShader(shaderProgram, geometryShader);
    if (fragmentShader) glAttachShader(shaderProgram, fragmentShader);

    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "HIBA::SHADER::PROGRAM::LINKELESI_HIBA\n" << infoLog << std::endl;
    }

    if (vertexShader)   glDeleteShader(vertexShader);
    if (geometryShader) glDeleteShader(geometryShader);
    if (fragmentShader) glDeleteShader(fragmentShader);

    return shaderProgram;
}


// --- Shader-cache ---------------------------------------------------------
// Ugyanahhoz a fájlpárhoz csak egyszer olvasunk, fordítunk és linkelünk.

#include <map>
#include <utility>

namespace {
    std::map<std::pair<std::string, std::string>, GLuint>& cache() {
        static std::map<std::pair<std::string, std::string>, GLuint> c;
        return c;
    }
}

GLuint Builder::get_or_build(char const* vertex_file, char const* fragment_file) {
    auto key = std::make_pair(std::string(vertex_file), std::string(fragment_file));
    auto it = cache().find(key);
    if (it != cache().end()) return it->second;

    Builder::ShaderBuilder b;
    GLuint prog = b.add_vertex_shader(vertex_file)
                   .add_fragment_shader(fragment_file)
                   .build();
    cache().emplace(std::move(key), prog);
    return prog;
}

void Builder::clear_shader_cache() {
    for (auto& kv : cache()) glDeleteProgram(kv.second);
    cache().clear();
}
