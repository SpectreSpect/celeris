// main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

// Window
const unsigned int SCR_W = 1280;
const unsigned int SCR_H = 720;

// Camera state (globals for simplicity)
glm::vec3 cameraPos   = glm::vec3(0.0f, 1.0f,  5.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

float yaw   = -90.0f; // facing -Z initially
float pitch =  0.0f;

float lastX = (float)SCR_W / 2.0f;
float lastY = (float)SCR_H / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Helper: compile shader
GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        std::cerr << "Shader compile error: " << buf << "\n";
    }
    return s;
}

// Simple shaders: position + color, MVP uniform
const char* vertex_shader_src = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
out vec3 vColor;
uniform mat4 uMVP;
void main() {
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* fragment_shader_src = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

// mouse callback
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }
    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos; // reversed
    lastX = (float)xpos;
    lastY = (float)ypos;

    float sensitivity = 0.15f; // tweak
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// framebuffer size
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// process keyboard
void processInput(GLFWwindow *window) {
    float speed = 5.0f * deltaTime;
    glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= right * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += right * speed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) cameraPos += cameraUp * speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) cameraPos -= cameraUp * speed;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
}

int main() {
    if (!glfwInit()) {
        std::cerr << "glfwInit failed\n";
        return -1;
    }

    // Request OpenGL 3.3 core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_W, SCR_H, "Flycam (click window and move mouse)", nullptr, nullptr);
    if (!window) {
        std::cerr << "glfwCreateWindow failed\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW AFTER context creation
    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        std::cerr << "glewInit failed: " << glewGetErrorString(glewErr) << "\n";
        return -1;
    }
    // clear potential GL_INVALID_ENUM from glewInit in core contexts
    glGetError();

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // capture mouse: hides cursor and provides motion
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    // try to enable raw mouse motion if available (better for high-dpi / smoothing)
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    // simple scene: a colored cube (positions + colors)
    float vertices[] = {
        // positions         // colors
        -1.0f,-1.0f,-1.0f,   0.8f,0.2f,0.2f,
         1.0f,-1.0f,-1.0f,   0.2f,0.8f,0.2f,
         1.0f, 1.0f,-1.0f,   0.2f,0.2f,0.8f,
        -1.0f, 1.0f,-1.0f,   0.8f,0.8f,0.2f,
        -1.0f,-1.0f, 1.0f,   0.2f,0.8f,0.8f,
         1.0f,-1.0f, 1.0f,   0.8f,0.2f,0.8f,
         1.0f, 1.0f, 1.0f,   0.6f,0.6f,0.6f,
        -1.0f, 1.0f, 1.0f,   0.3f,0.7f,0.4f
    };
    unsigned int indices[] = {
        0,1,2, 2,3,0,
        1,5,6, 6,2,1,
        5,4,7, 7,6,5,
        4,0,3, 3,7,4,
        3,2,6, 6,7,3,
        4,5,1, 1,0,4
    };

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
      glBindBuffer(GL_ARRAY_BUFFER, VBO);
      glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

      // pos
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
      glEnableVertexAttribArray(0);
      // color
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
      glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // compile shaders and program
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linkok; glGetProgramiv(prog, GL_LINK_STATUS, &linkok);
    if (!linkok) {
        char buf[1024];
        glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
        std::cerr << "Program link error: " << buf << "\n";
    }
    glDeleteShader(vs); glDeleteShader(fs);

    glEnable(GL_DEPTH_TEST);

    auto lastTitleUpdate = std::chrono::steady_clock::now();

    // main loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // MVP
        glm::mat4 projection = glm::perspective(glm::radians(60.0f), (float)SCR_W / (float)SCR_H, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 mvp = projection * view * model;

        glUseProgram(prog);
        GLint loc = glGetUniformLocation(prog, "uMVP");
        glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // update window title occasionally with camera info so it's obvious it's moving
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTitleUpdate).count() > 150) {
            lastTitleUpdate = now;
            char title[256];
            snprintf(title, sizeof(title), "Flycam | pos: %.2f, %.2f, %.2f | yaw: %.1f pitch: %.1f",
                     cameraPos.x, cameraPos.y, cameraPos.z, yaw, pitch);
            glfwSetWindowTitle(window, title);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup
    glDeleteProgram(prog);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &VAO);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
