//
// Created by madam on 2026. 03. 12..
//

#ifndef GORBE_POINT_HPP
#define GORBE_POINT_HPP

#include <glad/glad.h>

enum PosState {base, toCurve, toDist, fromDisttoCurve};

struct Point {
    glm::vec3 pos;
    float d = 1.0f;
    glm::vec3 vel{0};

    glm::vec3 grad{0};
    float f;
    PosState state = base;
};


#endif //GORBE_POINT_HPP