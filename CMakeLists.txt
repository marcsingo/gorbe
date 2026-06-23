cmake_minimum_required(VERSION 3.20)
project(gorbe)

set(CMAKE_CXX_STANDARD 20)

# 1. GLFW hozzáadása
# Kikapcsoljuk a felesleges dolgokat a GLFW-ben, hogy gyorsabb legyen a build
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(libraries/glfw)

# 2. GLM és GLAD include útvonalak
include_directories(libraries/glm)
include_directories(libraries/glad/include)

# 2b. Dear ImGui (GLFW + OpenGL3 backend) statikus könyvtárként
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/imgui)
add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

# 3. Forrásfájlok meghatározása (glad.c-t bele kell venni!)
set(SOURCE_FILES main.cpp libraries/glad/src/glad.c
        utils/init.hpp
        utils/init.cpp
        utils/shader.cpp
        utils/shader.hpp
        model/model.cpp
        model/Model.hpp
        model/camera.cpp
        model/camera.hpp
        model/Scene.cpp
        model/Scene.hpp
        matek/Analizis.hpp
        matek/Kifejezes.hpp
        matek/levelek/Konstans.hpp
        matek/levelek/Valtozo.hpp
        matek/levelek/Parameter.hpp
        matek/muveletek/KetOperandus.hpp
        matek/muveletek/Osszeg.hpp
        matek/muveletek/Kulonbseg.hpp
        matek/muveletek/Szorzat.hpp
        matek/muveletek/Hanyados.hpp
        matek/muveletek/Hatvany.hpp
        matek/fuggvenyek/Fuggveny.hpp
        matek/fuggvenyek/Ln.hpp
        matek/fuggvenyek/Log.hpp
        matek/fuggvenyek/Sin.hpp
        matek/fuggvenyek/Cos.hpp
        matek/fuggvenyek/Tan.hpp
        matek/fuggvenyek/Ctg.hpp
        model/Window.cpp
        model/Window.hpp
        model/Window.hpp
        model/Shader.cpp
        model/Shader.hpp
        model/Gui.cpp
        model/Gui.hpp
        objects/Gorbe.hpp
        objects/Vector.hpp
        objects/Point.hpp
        matek/Egyenlet.hpp
        App.hpp
        particle_sampling/ImplicitSurface.hpp
        particle_sampling/Surface.hpp
        particle_sampling/Occluders.hpp
        matek/Kif.hpp
        model/Include.hpp
        particle_sampling/Particle.hpp)

# 4. Az executable létrehozása
add_executable(gorbe ${SOURCE_FILES}
        utils/init.hpp)

# 5. Linkelés (GLFW, OpenGL és Dear ImGui)
find_package(OpenGL REQUIRED)
target_link_libraries(gorbe glfw OpenGL::GL imgui)

# 6. Shaderek a binárís mellé. A SHADER_DIR makró a bináris könyvtárára mutat, a kód
#    onnan tölti a shadereket -> a program tetszőleges munkakönyvtárból indítható.
target_compile_definitions(gorbe PRIVATE "SHADER_DIR=\"$<TARGET_FILE_DIR:gorbe>\"")

add_custom_command(TARGET gorbe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/particle_sampling/vertex.vert"
            "${CMAKE_CURRENT_SOURCE_DIR}/particle_sampling/fragment.glsl"
            "$<TARGET_FILE_DIR:gorbe>"
        COMMENT "Shaderek masolasa a binaris melle")