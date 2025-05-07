#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "lib/serialib.h"
#include <fstream>
#include <sstream>
#include <string>

#if defined (__linux__) || defined(__APPLE__)
    #define SERIAL_PORT "/dev/cu.usbserial-120"
#endif

void serialfunc(serialib& serial);

GLuint compileShader(GLenum type, const char* src);
GLuint linkProgram(GLuint vert, GLuint frag);
std::string loadShaderSource(const char* path);

// Global parameters read via serial
int p0, p1, p2, p3, p4, p5;

// Fullscreen quad in NDC + UV coords
float quadVertices[] = {
    -1.0f,  1.0f,   0.0f, 0.0f,
    -1.0f, -1.0f,   0.0f, 1.0f,
     1.0f, -1.0f,   1.0f, 1.0f,
    -1.0f,  1.0f,   0.0f, 0.0f,
     1.0f, -1.0f,   1.0f, 1.0f,
     1.0f,  1.0f,   1.0f, 0.0f
};

// Simple passthrough vertex shader
const char* vertexShaderSource = R"vert(
    #version 330 core
    layout(location = 0) in vec2 aPos;
    layout(location = 1) in vec2 aTex;
    out vec2 TexCoords;
    void main() {
        TexCoords = aTex;
        gl_Position = vec4(aPos, 0.0, 1.0);
    }
)vert";




// Globals to track real framebuffer size
static int fbW, fbH;

// Callback for resized framebuffers


int main() {
    serialib serial;
    bool serialDisponible = false;

    // Attempt serial connection
    char err = serial.openDevice(SERIAL_PORT, 9600);
    if (err != 1) {
        std::cerr << "Error opening serial: code " << (int)err << std::endl;
        // handle or ignore; we won't crash if serial fails
    } else {
        std::cout << "Connected to " << SERIAL_PORT << std::endl;
        serialDisponible = true;
    }

    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow(
        mode->width, mode->height,
        "Sinestesia",
        glfwGetPrimaryMonitor(),
        NULL
    );
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // real size of framebuffer
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    // Setup fullscreen quad VAO/VBO
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Compile and link shaders
    GLuint vShader   = compileShader(GL_VERTEX_SHADER,   vertexShaderSource);
    GLuint fLeft     = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("../shaders/leftFragment.frag").c_str());
    GLuint fCenter   = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("../shaders/centerFragment.frag").c_str());
    GLuint fRight    = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("../shaders/rightFragment.frag").c_str());

    GLuint progLeft   = linkProgram(vShader, fLeft);
    GLuint progCenter = linkProgram(vShader, fCenter);
    GLuint progRight  = linkProgram(vShader, fRight);
    // Ejemplo en C++ con OpenGL
    glUniform2f(glGetUniformLocation(progLeft, "u_resolution"), fbW, fbH);
    glUniform1f(glGetUniformLocation(progLeft, "u_time"), glfwGetTime());

    glDeleteShader(vShader);
    glDeleteShader(fLeft);
    glDeleteShader(fCenter);
    glDeleteShader(fRight);
    glEnable(GL_BLEND);
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        if (serialDisponible) serialfunc(serial);

        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(VAO);

        int third = fbW / 3;


        // draw red on left third
        glViewport(0,        0, third, fbH);
        glUseProgram(progLeft);
        //set uniforms
        glUniform2f(glGetUniformLocation(progLeft, "u_resolution"), fbW/3, fbH);
        glUniform1f(glGetUniformLocation(progLeft, "u_time"), glfwGetTime());
        glUniform1f(glGetUniformLocation(progLeft, "u_flowerDensity"), 5.f);
        glUniform1f(glGetUniformLocation(progLeft, "u_noiseAmount"), 1.f);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        // draw green on center third
        glViewport(third,    0, third, fbH);
        glUseProgram(progCenter);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // draw blue on right third
        glViewport(third*2,  0, third, fbH);
        glUseProgram(progRight);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteProgram(progLeft);
    glDeleteProgram(progCenter);
    glDeleteProgram(progRight);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// Reads 6 16-bit values from serial, prints them
void serialfunc(serialib& serial) {
    unsigned char buf[12];
    int n = serial.readBytes(buf, 12, 1000);
    if (n == 12) {
        p0 = buf[0]  | (buf[1]  << 8);
        p1 = buf[2]  | (buf[3]  << 8);
        p2 = buf[4]  | (buf[5]  << 8);
        p3 = buf[6]  | (buf[7]  << 8);
        p4 = buf[8]  | (buf[9]  << 8);
        p5 = buf[10] | (buf[11] << 8);
        std::cout << "P0:" << p0 << " P1:" << p1 << " P2:" << p2
                  << " P3:" << p3 << " P4:" << p4 << " P5:" << p5
                  << std::endl;
    } else {
        std::cerr << "Serial read incomplete: " << n << " bytes\n";
    }
}

// Compile shader and log errors
GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, NULL, log);
        std::cerr << "Shader compile error: " << log << std::endl;
    }
    return s;
}

// Link program and log errors
GLuint linkProgram(GLuint vert, GLuint frag) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vert);
    glAttachShader(p, frag);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, 512, NULL, log);
        std::cerr << "Program link error: " << log << std::endl;
    }
    return p;
}
// loader {
std::string loadShaderSource(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "No se pudo abrir el archivo: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}