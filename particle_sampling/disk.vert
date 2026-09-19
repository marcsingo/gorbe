#version 330 core
// A reszecske-korongok INSTANCINGGAL (object/ObjectView.hpp, ParticleDisks).
//
// Egyetlen kozos egysegkorong (16 haromszog a z=0 sikban, sugar 1), es
// reszecskenkent (instance) csak a kozeppont, a normalis es a sugar jon. A korong
// sikjat (T, B) itt szamoljuk a normalisbol — PONTOSAN ugyanugy, mint korabban a
// CPU-n, igy a kep valtozatlan (tests/gl/test_gl_disks.cpp veti ossze).
layout(location = 0) in vec3 corner;     // egysegkorong-csucs: (cos a, sin a, 0) vagy (0,0,0)
layout(location = 2) in vec3 i_center;   // a reszecske helye
layout(location = 3) in vec3 i_normal;   // a felulet gradiense (nem kell normalizalva)
layout(location = 4) in float i_radius;  // a korong sugara

uniform mat4 u_MVP;

out vec3 v_normal;

void main() {
    vec3 N = dot(i_normal, i_normal) > 1e-12 ? normalize(i_normal) : vec3(0.0, 1.0, 0.0);
    vec3 helper = abs(N.x) < 0.9 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 T = normalize(cross(N, helper));
    vec3 B = cross(N, T);
    vec3 p = i_center + i_radius * (corner.x * T + corner.y * B);
    v_normal = N;
    gl_Position = u_MVP * vec4(p, 1.0);
}
