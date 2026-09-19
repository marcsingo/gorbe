cmake_minimum_required(VERSION 3.20)

# A szimuláció forró útja sok apró, beilleszthető függvényből áll; optimalizálás
# nélkül ez mérve 5.8-7.5x lassabb. Ezért ha a hívó nem adott build típust, akkor
# RelWithDebInfo-t választunk, hogy ne véletlenül Debugban méregessünk.
#
# FONTOS, hogy ez a project() ELŐTT álljon: MSVC-nél a platformmodul a project()
# hívásban CMAKE_BUILD_TYPE_INIT=Debug-ot alkalmaz, tehát utána már mindig van
# érték, és egy "ha üres" feltétel sosem sülne el. FORCE nélkül a -DCMAKE_BUILD_TYPE=...
# és a meglévő cache (pl. a CLion profiljai) továbbra is felülír minket.
if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build tipus")
    message(STATUS "CMAKE_BUILD_TYPE nem volt megadva -> RelWithDebInfo")
endif()

project(gorbe)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
# nlohmann/json (egyetlen fejléc, MIT) a projektfájlokhoz: #include "nlohmann/json.hpp"
include_directories(libraries)

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
# FIGYELEM: a fájlnevek kis-nagybetűje számít — Windowson elnézőbb a fájlrendszer,
# Linuxon/macOS-en viszont a rossz betűzéstől nem fordul le a projekt.
set(SOURCE_FILES main.cpp libraries/glad/src/glad.c
        utils/init.hpp
        utils/init.cpp
        model/Model.cpp
        model/Model.hpp
        model/Camera.cpp
        model/Camera.hpp
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
        matek/Fuggvenyek.hpp
        model/Window.cpp
        model/Window.hpp
        model/Shader.cpp
        model/Shader.hpp
        model/Gui.cpp
        model/Gui.hpp
        model/Axes.hpp
        model/CameraBasis.hpp
        model/Viewport.hpp
        model/Framebuffer.hpp
        scene/Scope.hpp
        scene/Time.hpp
        scene/Document.hpp
        scene/Names.hpp
        scene/Presets.hpp
        scene/Validate.hpp
        scene/ProjectFile.hpp
        scene/Build.hpp
        app/Scene.hpp
        app/Photo.hpp
        app/Controller.hpp
        ui/Requests.hpp
        ui/Layout.hpp
        ui/ShapesPanel.hpp
        ui/PropsPanel.hpp
        ui/HelpPanel.hpp
        ui/ParamsPanel.hpp
        ui/ViewportPanel.hpp
        ui/MenuBar.hpp
        raytrace/Raytracer.hpp
        raytrace/Palette.hpp
        raytrace/Material.hpp
        raytrace/Image.hpp
        App.hpp
        particle_sampling/ParticleSystem.hpp
        object/ObjectView.hpp
        object/ObjectController.hpp
        object/SpaceObject.hpp
        utils/Parallel.hpp
        particle_sampling/Surface.hpp
        particle_sampling/DomainConstraint.hpp
        particle_sampling/Transform.hpp
        particle_sampling/WarpPresets.hpp
        particle_sampling/SpatialGrid.hpp
        particle_sampling/ParticleView.hpp
        matek/Program.hpp
        matek/Kif.hpp
        matek/Parser.hpp
        model/Include.hpp
        particle_sampling/Particle.hpp)

# 4. Az executable létrehozása
add_executable(gorbe ${SOURCE_FILES})

# 5. Linkelés (GLFW, OpenGL és Dear ImGui)
find_package(OpenGL REQUIRED)
target_link_libraries(gorbe glfw OpenGL::GL imgui)

# 6. A bináris könyvtárának átadása a kódnak.
#    SHADER_DIR: innen tölti a shadereket (lásd a másolást lentebb).
#    BINARY_DIR: ide kerülnek a mentett fájlok (pl. a sugárkövetett fényképek a
#                `kepek/` almappába) — így "a futtatás mellé", nem a munkakönyvtárba.
#    Mindkettő ugyanaz az útvonal, de külön néven, hogy a jelentésük egyértelmű legyen.
target_compile_definitions(gorbe PRIVATE
        "SHADER_DIR=\"$<TARGET_FILE_DIR:gorbe>\""
        "BINARY_DIR=\"$<TARGET_FILE_DIR:gorbe>\"")

# 7. Tesztek (ctest). A matek/ és a részecske-kényszerek rétege GL nélkül,
#    fejlécből tesztelhető — ez a projekt legkockázatosabb, legnehezebben
#    debuggolható része, ezért itt van a legtöbb haszna.
#      cmake --build <dir> && ctest --test-dir <dir> --output-on-failure
option(GORBE_BUILD_TESTS "Tesztek forditasa" ON)
if(GORBE_BUILD_TESTS)
    enable_testing()
    foreach(t test_shapes test_csg test_domain test_program test_transform test_camera test_raytrace test_ui test_build test_parser test_particles test_project)
        add_executable(${t} tests/${t}.cpp)
        target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME ${t} COMMAND ${t})
    endforeach()
endif()

# 8. Shaderek másolása a bináris mellé.
#
# Ez SZÁNDÉKOSAN külön, mindig lefutó target — nem a gorbe POST_BUILD lépése.
# Utóbbi ugyanis csak akkor fut, ha a gorbe újralinkelődik: ha CSAK egy .glsl-t
# szerkesztesz, a build "no work to do"-t mond, a bináris mellett viszont a RÉGI
# shader marad, és a változás némán nem érvényesül.
# A copy_if_different olcsó, ezért nyugodtan futhat minden buildnél.
add_custom_target(shaders ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/particle_sampling/vertex.vert"
            "${CMAKE_CURRENT_SOURCE_DIR}/particle_sampling/disk.vert"
            "${CMAKE_CURRENT_SOURCE_DIR}/particle_sampling/fragment.glsl"
            "$<TARGET_FILE_DIR:gorbe>"
        COMMENT "Shaderek masolasa a binaris melle")
add_dependencies(gorbe shaders)
# 9. GL-kontextust igenylo tesztek — KULON kapcsolora, alapbol KI.
#
#      cmake -B <dir> -DGORBE_BUILD_GL_TESTS=ON && ctest --test-dir <dir>
#
# Valodi (rejtett) ablakot es GL-kontextust nyitnak, tehat fejgep nelkuli gepen es a
# szokasos CI-n nincs mivel futniuk. A 7. pont tesztjei ezert GL-mentesek.
#   test_gl_disks : az instancinges korong-rajzolas ugyanazt a kepet adja, mint a
#                   korabbi CPU-s (pixelenkenti osszevetes)
option(GORBE_BUILD_GL_TESTS "GL-kontextust igenylo tesztek forditasa" OFF)
if(GORBE_BUILD_GL_TESTS)
    enable_testing()
    foreach(t test_gl_disks)
        add_executable(${t} tests/gl/${t}.cpp
                libraries/glad/src/glad.c
                model/Window.cpp
                model/Camera.cpp
                model/Model.cpp
                model/Shader.cpp
                model/Gui.cpp
                utils/init.cpp)
        target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(${t} glfw OpenGL::GL imgui)
        target_compile_definitions(${t} PRIVATE "SHADER_DIR=\"$<TARGET_FILE_DIR:gorbe>\"")
        add_dependencies(${t} shaders)
        add_test(NAME ${t} COMMAND ${t})
    endforeach()
endif()
