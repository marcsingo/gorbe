//
// Created by madam on 2026. 03. 12..
//

#ifndef GORBE_POINT_HPP
#define GORBE_POINT_HPP

#include <glad/glad.h>

enum PosState {base, toCurve, toDist, fromDisttoCurve};

struct Point {
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec3 grad;
    float d;
    float f;
    PosState state;
};


#endif //GORBE_POINT_HPP