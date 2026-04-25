// //
// // Created by madam on 2026. 03. 12..
// //
//
// #ifndef GORBE_POINT_HPP
// #define GORBE_POINT_HPP
//
// #include <glad/glad.h>
// #include <map>
//
// enum PosState {base, toCurve, toDist, fromDisttoCurve};
//
// struct Point {
//
//     //fizika:
//     glm::vec3 p;
//     std::map<std::string, glm::vec3> forces;
//     glm::vec3 v{0, 0, 0};
//     float nu = 0.5f; // lassítási tényező
//     float m;
//
//     // függvény adatok:
//     glm::vec3 F{0};
//     glm::vec3 g{0};
//     float delta = 1.0f;
//     float gamma = 0.3f;
//     float f;
//     float K;
//
//     //állapot:
//     PosState state = base;
// };
//
//
// #endif //GORBE_POINT_HPP