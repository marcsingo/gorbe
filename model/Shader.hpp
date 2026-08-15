//
// Created by madam on 2026. 03. 08..
//

#ifndef GORBE_SHADER_HPP
#define GORBE_SHADER_HPP

// A shaderek könyvtára. Normál esetben a CMake állítja be a bináris könyvtárára
// (lásd CMakeLists.txt); ez csak tartalék, ha valamiért nincs megadva.
#ifndef SHADER_DIR
#define SHADER_DIR "../particle_sampling"
#endif

#include <vector>
#include <string>

#include "glad/glad.h"

namespace Builder {
    class ShaderBuilder {
        std::vector<std::string> vertexShader;
        std::vector<std::string> geometryShader;
        std::vector<std::string> fragmentShader;

        std::string read_from_file(const char* fileName);

        GLuint compile_shader(GLenum type, std::vector<std::string>& datas);
    public:
        ShaderBuilder& add_vertex_shader(std::string  & text);
        ShaderBuilder& add_vertex_shader(const char* fileName);
        ShaderBuilder& add_geometry_shader(const char* fileName);
        ShaderBuilder& add_fragment_shader(std::string & text);
        ShaderBuilder& add_fragment_shader(const char* fileName);

        GLuint build();

    };

    // Fájlpárból épített shader program, gyorsítótárazva.
    //
    // Enélkül minden Model példány külön beolvasta és lefordította UGYANAZT a
    // vertex+fragment párt: a részecske-samplerekkel együtt ez több tucat egyforma
    // GL programot jelentett, amiket ráadásul senki nem szabadított fel. A cache
    // fájlútvonalakra kulcsol, így az azonos párok egyetlen programon osztoznak.
    GLuint get_or_build(char const* vertex_file, char const* fragment_file);

    // Az összes gyorsítótárazott program felszabadítása. A GL kontextus megszűnése
    // ELŐTT kell hívni (lásd App::run vége).
    void clear_shader_cache();
}




#endif //GORBE_SHADER_HPP