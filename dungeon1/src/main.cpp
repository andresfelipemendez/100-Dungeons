#include <flecs.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdio.h>

#include <string>
#include <fstream>
#include <sstream>
#include "model.h"


// Function to read a shader from a file
std::string ReadShaderFile(const char* filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open shader file: %s\n", filePath);
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Function to compile a shader from source code
unsigned int CompileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    // Error handling
    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        char* message = new char[length];
        glGetShaderInfoLog(id, length, &length, message);
        fprintf(stderr, "Failed to compile %s shader!\n", type == GL_VERTEX_SHADER ? "vertex" : "fragment");
        fprintf(stderr, "%s\n", message);
        delete[] message;
        glDeleteShader(id);
        return 0;
    }

    return id;
}

// Function to create a shader program
unsigned int CreateShader(const char* vertexShaderPath, const char* fragmentShaderPath) {
    std::string vertexShaderSource = ReadShaderFile(vertexShaderPath);
    std::string fragmentShaderSource = ReadShaderFile(fragmentShaderPath);

    unsigned int program = glCreateProgram();
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShaderSource.c_str());
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource.c_str());

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glValidateProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

int main() {

    flecs::world ecs;

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

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD");
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    unsigned int shaderProgram = CreateShader("assets/shaders/simple.vs", "assets/shaders/simple.fs");
    glUseProgram(shaderProgram);

    struct Transform {
        float position[3];
        float rotation[3];
        float scale[3] = {1.0f, 1.0f, 1.0f};
    };


    struct Animation {
        float currentTime;
        float duration;
    };

    ecs.component<Transform>();
    ecs.component<MeshID>();
    ecs.component<Animation>();

    auto dungeonEntity = ecs.entity("Dungeon").set<Transform>({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});

    auto model = loadModel();

    dungeonEntity.set<MeshID>(model);

     while (!glfwWindowShouldClose(window)) {
        // Clear the screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Update ECS world (optional: could handle animations or other systems here)
        ecs.progress();

        // Draw all entities that have a Mesh and Transform
        ecs.each([&](flecs::entity e, const Transform& transform, const MeshID& mesh) {
            // Apply transformations (e.g., model matrix)
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(transform.position[0], transform.position[1], transform.position[2]));
            model = glm::scale(model, glm::vec3(transform.scale[0], transform.scale[1], transform.scale[2]));
            model = glm::rotate(model, transform.rotation[0], glm::vec3(1, 0, 0));
            model = glm::rotate(model, transform.rotation[1], glm::vec3(0, 1, 0));
            model = glm::rotate(model, transform.rotation[2], glm::vec3(0, 0, 1));

            // Bind VAO and render (use shaders, bind textures, etc.)
            glBindVertexArray(mesh.VAO);
            glDrawElements(GL_TRIANGLES, mesh.vertexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        });

        // Swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean up and terminate GLFW
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;

}