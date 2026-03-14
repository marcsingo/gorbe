
#include "shader.hpp"

#include <fstream>
#include <sstream>
#include <iostream>

std::string Utils::load_shader_source(const char* filepath) {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        file.open(Utils::source + filepath);
        std::stringstream stream;
        stream << file.rdbuf();
        file.close();
        return stream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "HIBA::SHADER::FAJL_NEM_OLVASHATO: " << filepath << std::endl;
        return "";
    }
}