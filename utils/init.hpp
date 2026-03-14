
#ifndef GORBE_INIT_HPP
#define GORBE_INIT_HPP

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Utils {
    void init_glfw();
    GLFWwindow* create_window(int, int, const char*);
}



#endif //GORBE_INIT_HPP