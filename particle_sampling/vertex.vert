#version 330 core
layout (location = 0) in vec3 pos;

uniform mat4 u_MVP;
uniform vec3 color;
out vec3 o_color;


float f(float x, float y, float z);
vec3 fd(float x, float y, float z);

void main() {



    gl_Position = u_MVP * vec4(pos, 1.0f);

    o_color = color;
}