#version 330 core
layout(location = 0) in vec3 pos;
// Opcionalis normalis. Ha a Model nem tolt fel normalisokat, ez az attributum
// LETILTOTT, es akkor a GL konstans (0,0,0,1) erteket ad -> a fragment shader
// ebbol tudja, hogy arnyalas nelkul, egyszinuen kell rajzolnia (tengelyek, racs).
layout(location = 1) in vec3 normal;

uniform mat4 u_MVP;

out vec3 v_normal;

void main() {
    v_normal = normal;
    gl_Position = u_MVP * vec4(pos, 1.0);
}
