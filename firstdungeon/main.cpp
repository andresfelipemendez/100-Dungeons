

#include <GLFW/glfw3.h>
#include <stdio.h>


int main() {

    if (!glfwInit()) {
         fprintf(stderr, "Failed to initialize GLFW");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "3D Dungeon Engine", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);

    // if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    //     fprintf(stderr, "Failed to initialize GLAD");
    //     return -1;
    // }

    glEnable(GL_DEPTH_TEST);

}