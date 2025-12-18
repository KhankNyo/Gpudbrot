
#include "glad/glad.h"
#include "GL/gl.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
 
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
 

typedef struct 
{
    float A, B;
} vec2;
typedef struct 
{
    float A, B, C;
} vec3;
 
static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}
 
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static double DisplayFps(double Start)
{
    double Now = glfwGetTime();
    double ElapsedTime = Now - Start;
    double Fps = 1.0 / ElapsedTime;
    printf("\rfps: %f", Fps);
    fflush(stdout);
    return Now;
}
 
int main(void)
{
    glfwSetErrorCallback(error_callback);
 
    if (!glfwInit())
        exit(EXIT_FAILURE);
 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
 
    GLFWwindow* window = glfwCreateWindow(640, 480, "OpenGL Triangle", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
 
    glfwSetKeyCallback(window, key_callback);
 
    glfwMakeContextCurrent(window);
    gladLoadGLLoader(glfwGetProcAddress);
    glfwSwapInterval(1); // disable vsync
 
    // NOTE: OpenGL error checks have been omitted for brevity
    double Start = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
 
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        Start = DisplayFps(Start);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
 
    glfwDestroyWindow(window);
 
    glfwTerminate();
    exit(EXIT_SUCCESS);
}
 

