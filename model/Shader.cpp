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
        glGetShaderInfoLog(type, 512, nullptr, infoLog);
        std::string typeStr = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        std::cerr << "HIBA::SHADER::" << typeStr << "::FORDITASI_HIBA\n" << infoLog << std::endl;
    }

    return shader_id;
}

GLuint Builder::ShaderBuilder::build() {
    GLuint vertexShader = compile_shader(GL_VERTEX_SHADER, this->vertexShader);
    GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, this->fragmentShader);

    GLuint shaderProgram = glCreateProgram();

    if (vertexShader) glAttachShader(shaderProgram, vertexShader);
    if (fragmentShader) glAttachShader(shaderProgram, fragmentShader);

    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "HIBA::SHADER::PROGRAM::LINKELESI_HIBA\n" << infoLog << std::endl;
    }

    if (vertexShader) glDeleteShader(vertexShader);
    if (fragmentShader) glDeleteShader(fragmentShader);

    return shaderProgram;
}

