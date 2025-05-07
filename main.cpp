#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "lib/serialib.h"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <vector>

#if defined (__linux__) || defined(__APPLE__)
    #define SERIAL_PORT "/dev/cu.usbserial-1120"
#endif

void serialfunc(serialib& serial);
GLuint compileShader(GLenum type, const char* src);
GLuint linkProgram(GLuint vert, GLuint frag);
std::string loadShaderSource(const char* path);

// Global parameters read via serial
int p0, p1, p2, p3, p4, p5;

struct ProgramInfo {
    GLuint program;
    GLint loc_resolution;
    GLint loc_time;
    GLint loc_density;
    GLint loc_noise;
    GLint loc_swirl;
    GLint loc_xOffset;
};

// Vertex shader source
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

static int fbW, fbH;

int main() {
    serialib serial;
    bool serialDisponible = false;

    // Constants for emotion mapping
    const float BASE_DENSITY   = 0.1f;
    const float MAX_DENSITY    = 20.0f;
    const float BASE_NOISE     = 1.0f;
    const float MIN_NOISE      = -22.0f;
    const float MAX_NOISE      = 22.0f;
    const float MAX_SWIRL      = 200.0f;
    const float MAX_TIME_SCALE = 5.0f;
    const float MIN_TIME_SCALE = 0.1f;

    // Attempt serial connection
    char err = serial.openDevice(SERIAL_PORT, 115200);
    if (err == 1) {
        std::cout << "Connected to " << SERIAL_PORT << std::endl;
        serialDisponible = true;
    } else {
        std::cerr << "Error opening serial: code " << (int)err << std::endl;
    }

    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Sinestesia", glfwGetPrimaryMonitor(), NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    // Quad setup
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f
    };
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

    // Compile shaders
    GLuint vShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    std::vector<std::string> fragPaths = {"../shaders/leftFragment.frag", "../shaders/centerFragment.frag", "../shaders/rightFragment.frag"};
    std::vector<ProgramInfo> programs;
    programs.reserve(3);

    for (int i = 0; i < 3; ++i) {
        std::string src = loadShaderSource(fragPaths[i].c_str());
        GLuint fShader = compileShader(GL_FRAGMENT_SHADER, src.c_str());
        GLuint prog = linkProgram(vShader, fShader);
        glDeleteShader(fShader);

        ProgramInfo info;
        info.program        = prog;
        info.loc_resolution = glGetUniformLocation(prog, "u_resolution");
        info.loc_time       = glGetUniformLocation(prog, "u_time");
        info.loc_density    = glGetUniformLocation(prog, "u_flowerDensity");
        info.loc_noise      = glGetUniformLocation(prog, "u_noiseAmount");
        info.loc_swirl      = glGetUniformLocation(prog, "u_swirlIntensity");
        info.loc_xOffset    = glGetUniformLocation(prog, "u_xOffset");
        programs.push_back(info);
    }
    glDeleteShader(vShader);

    // Helper clamp
    auto clamp = [](float v, float lo, float hi) {
        return (v < lo ? lo : (v > hi ? hi : v));
    };

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        if (serialDisponible) serialfunc(serial);

        float densityArr[3], noiseArr[3], swirlArr[3], timeScaleArr[3];
        for (int i = 0; i < 3; ++i) {
            int rawLeft  = (i==0 ? p0 : i==1 ? p2 : p4);
            int rawRight = (i==0 ? p1 : i==1 ? p3 : p5);
            float leftN  = rawLeft  / 1023.0f;
            float rightN = rawRight / 1023.0f;

            // Default time scale
            timeScaleArr[i] = 1.0f;

            if (rightN > 0.0f && leftN <= 0.0f) {
                // Felicidad sola: acelerar tiempo hasta 1.8x
                densityArr[i]    = clamp(BASE_DENSITY + rightN * (MAX_DENSITY - BASE_DENSITY), BASE_DENSITY, MAX_DENSITY);
                noiseArr[i]      = BASE_NOISE;
                swirlArr[i]      = 0.0f;
                timeScaleArr[i] = clamp(1.0f + rightN * (MAX_TIME_SCALE - 1.0f), 1.0f, MAX_TIME_SCALE);

            } else if (leftN > 0.0f && rightN <= 0.0f) {
                // Melancolía sola: ralentizar tiempo
                densityArr[i]    = BASE_DENSITY;
                noiseArr[i]      = clamp(BASE_NOISE + leftN * (MIN_NOISE - BASE_NOISE), MIN_NOISE, MAX_NOISE);
                swirlArr[i]      = leftN * MAX_SWIRL;
                timeScaleArr[i] = clamp(1.0f - leftN, MIN_TIME_SCALE, 1.0f);

            } else if (leftN > 0.0f && rightN > 0.0f) {
                // Combinación: mix velocidad y lentitud
                float comb = (leftN + rightN) * 0.5f;
                densityArr[i]    = clamp(BASE_DENSITY - comb * BASE_DENSITY, BASE_DENSITY, MAX_DENSITY);
                noiseArr[i]      = clamp(BASE_NOISE + comb * (MAX_NOISE - BASE_NOISE), MIN_NOISE, MAX_NOISE);
                swirlArr[i]      = comb * MAX_SWIRL;
                timeScaleArr[i] = clamp((1.0f - leftN) + rightN * (MAX_TIME_SCALE - 1.0f), MIN_TIME_SCALE, MAX_TIME_SCALE);

            } else {
                // Ninguno
                densityArr[i]    = BASE_DENSITY;
                noiseArr[i]      = BASE_NOISE;
                swirlArr[i]      = 0.0f;
                timeScaleArr[i]  = 1.0f;
            }
        }

        glBindVertexArray(VAO);
        int third = fbW / 3;
        for (int i = 0; i < 3; ++i) {
            const auto &info = programs[i];
            int x = i * third;
            glViewport(x, 0, third, fbH);
            glUseProgram(info.program);
            glUniform2f(info.loc_resolution, (float)third, (float)fbH);
            glUniform1f(info.loc_time,       (float)glfwGetTime() * timeScaleArr[i]);
            glUniform1f(info.loc_density,    densityArr[i]);
            glUniform1f(info.loc_noise,      noiseArr[i]);
            glUniform1f(info.loc_swirl,      swirlArr[i]);
            if (info.loc_xOffset != -1)
                glUniform1f(info.loc_xOffset, (float)x);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    serial.closeDevice();
    for (const auto &info : programs)
        glDeleteProgram(info.program);
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