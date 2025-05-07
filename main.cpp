#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include "lib/serialib.h"

#if defined (_WIN32) || defined(_WIN64)

#endif
#if defined (__linux__) || defined(__APPLE__)
    #define SERIAL_PORT "/dev/cu.usbserial-120"
#endif
void serialfunc(serialib& serial);
GLuint compileShader(GLenum type, const char* src) ;
GLuint linkProgram(GLuint vert, GLuint frag);

int  p0, p1, p2, p3, p4, p5;
float quadVertices[] = {
    -1.0f,  1.0f,   0.0f, 0.0f,
    -1.0f, -1.0f,   0.0f, 1.0f,
     1.0f, -1.0f,   1.0f, 1.0f,

    -1.0f,  1.0f,   0.0f, 0.0f,
     1.0f, -1.0f,   1.0f, 1.0f,
     1.0f,  1.0f,   1.0f, 0.0f
};
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

const char* effectFragmentShader = R"frag(
#version 330 core
in vec2 TexCoords;
out vec4 FragColor;
uniform vec2 resolution;
uniform float time;
uniform float xpos;
uniform float ypos;
uniform float uSize;
void main() {
    vec2 I = gl_FragCoord.xy;
    vec4 O = vec4(0.0);
    vec2 v = resolution;
    vec2 p = (I * 2.0) / (v.y * uSize);
    p.x -= xpos;
    p.y -= ypos;
    for(float i = 0.2, l; i < 1.0; i+=0.05)
    {
        v = vec2(mod(atan(p.y,p.x) + i + i*time, 6.2831853) - 3.1415926, 1.0) * length(p) - i;
        v.x -= clamp(v.x += i, -i, i);
        O += (cos(i * 5.0 + vec4(0,1,2,3)) + 1.0) *
             (1.0 + v.y / (l = length(v) + 0.003)) / l;
    }
    O = tanh(O / 100.0);
    FragColor = O;
}
)frag";


int main() {
    serialib serial;
    char errorOpening = serial.openDevice(SERIAL_PORT, 9600);
    if (errorOpening!=1) return errorOpening;
    printf ("Successful connection to %s\n",SERIAL_PORT);
    if (!glfwInit());
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Sinestesia", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    //dddd


    // Setup fullscreen quad
    GLuint VAO,VBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(quadVertices),quadVertices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint v  = compileShader(GL_VERTEX_SHADER,   vertexShaderSource);
    GLuint fe = compileShader(GL_FRAGMENT_SHADER, effectFragmentShader);

    GLuint effectProgram = linkProgram(v, fe);

    glDeleteShader(v);
    glDeleteShader(fe);


    // Uniform locations
    GLint locResE  = glGetUniformLocation(effectProgram,"resolution");
    GLint locTimeE = glGetUniformLocation(effectProgram,"time");
    GLint locX     = glGetUniformLocation(effectProgram,"xpos");
    GLint locY     = glGetUniformLocation(effectProgram,"ypos");
    GLint locU     = glGetUniformLocation(effectProgram,"uSize");

    glViewport(0, 0, 800, 600);
    int esc = 0;
    while (!glfwWindowShouldClose(window)) {
        bool input = false;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if (!input) {
                esc+=1;
                input = true;
            }
        }
        if (esc>3) {
            //closed window
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
        glUseProgram(effectProgram);
        serialfunc(serial);
        glClearColor((float)p3/1023.f ,(float)p4/1023.f, (float)p5/1023.f, (float)p2/1023.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniform2f(locResE, 800.0f, 600.0f);
        glUniform1f(locTimeE, (float)glfwGetTime());
        glUniform1f(locX, (float)p0/300.f);
        glUniform1f(locY, (float)p1/300.f);
        glUniform1f(locU, 1.f);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);


        glfwSwapBuffers(window);
        glfwPollEvents();
        input = false;
    }

}

void serialfunc(serialib& serial) {
    unsigned char buffer[12];
    int readBytes = serial.readBytes(buffer, 12, 1000);

    if (readBytes == 12) {
         p0 = buffer[0] | (buffer[1] << 8);
         p1 = buffer[2] | (buffer[3] << 8);
         p2 = buffer[4] | (buffer[5] << 8);
         p3 = buffer[6] | (buffer[7] << 8);
         p4 = buffer[8] | (buffer[9] << 8);
         p5 = buffer[10] | (buffer[11] << 8);

        std::cout << "P0: " << p0 << " | P1: " << p1 << " | P2: " << p2
                  << " | P3: " << p3 << " | P4: " << p4 << " | P5: " << p5 << std::endl;
    } else {
        std::cerr << "Lectura incompleta: " << readBytes << " bytes." << std::endl;
    }
}

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Shader Compilation Error: " << infoLog << std::endl;
    }
    return shader;
}

// Helper: link program and log errors
GLuint linkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint success;
    char log[512];
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(prog, 512, NULL, log);
        std::cerr << "Program Linking Error: " << log << std::endl;
    }
    return prog;
}
