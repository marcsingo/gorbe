// //
// // Created by madam on 2026. 03. 12..
// //
//
// #ifndef GORBE_NORMALS_HPP
// #define GORBE_NORMALS_HPP
//
// #include "../model/Model.hpp"
// #include "Point.hpp"
// #include "../model/Shader.hpp"
//
// class Vector : public Model {
// protected:
//     glm::vec3 color;
//     void render(Camera const &camera) const override {
//         glLineWidth(1);
//         this->set_uniform("color", color);
//         glDrawArrays(GL_LINES, 0, vertices.size());
//     }
// public:
//     Vector(glm::vec3 color): Model(), color(color) {
//         Builder::ShaderBuilder builder;
//         set_shader(builder
//             .add_vertex_shader("../objects/vertex.vert")
//             .add_fragment_shader("../objects/fragment.glsl")
//             .build());
//     }
//
//     void reset() {
//         vertices.clear();
//     }
//
//     void update() {
//         update_buffers();
//     }
//
//     void add_vector(glm::vec3 start, glm::vec3 vec) {
//         vertices.push_back(start);
//         vertices.push_back(start+vec);
//     }
//
// };
//
// #endif //GORBE_NORMALS_HPP