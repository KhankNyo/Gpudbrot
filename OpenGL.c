
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "Platform.h"
#include "Common.h"


static uint32_t sStackAllocatorMemory[4*MB / sizeof(uint32_t)]; 
static uint8_t *sStackAllocatorTop = (uint8_t *)sStackAllocatorMemory;
static double sFrameTimeTargetS = 1.0 / 165.0;
static double sStartTimeS;
static double sFrameTimeMs = 0; /* for the app */
static app_state sAppState;
static GLFWwindow *sWindow;
static bool8 sLastKeyState[256], sCurrentKeyState[256];

void *Platform_PushMemory(int *PlatformMemory, int SizeBytes)
{
    int32_t AlignedSize = 0;
    if (SizeBytes > 0)
    {
        /* align size to 4-byte boundary */
        AlignedSize = (SizeBytes + sizeof(AlignedSize)) & ~0x3;
    }

    /* allocate the memory */
    void *Memory = sStackAllocatorTop;
    sStackAllocatorTop += AlignedSize;
    uint8_t *StackEnd = (uint8_t *)(sStackAllocatorMemory + STATIC_ARRAY_SIZE(sStackAllocatorMemory));
    assert(sStackAllocatorTop <= StackEnd && "Out of memory");

    (*PlatformMemory) += AlignedSize;
    return Memory;
}

void Platform_PopMemory(int SizeBytes)
{
    sStackAllocatorTop -= SizeBytes;
    assert(sStackAllocatorTop >= (uint8_t *)sStackAllocatorMemory);
}

int Platform_BeginTempMemory(void)
{
    return 0;
}

char *Platform_PushNullTerminatedFileContentBlocking(int *PlatformMemory, const char *FileName)
{
    FILE *f = fopen(FileName, "rb");
    if (!f)
    {
        return NULL;
    }

    /* little maneuver to get the file's size */
    size_t FileSize = 0;
    fseek(f, 0, SEEK_END);
    FileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    int FileBufferSize = FileSize + 1; /* null terminator */
    char *FileBuffer = Platform_PushMemory(PlatformMemory, FileBufferSize); 
    if (FileSize != fread(FileBuffer, 1, FileSize, f))
    {
        *PlatformMemory -= FileBufferSize;
        Platform_PopMemory(FileBufferSize);
        return NULL;
    }

    FileBuffer[FileSize] = '\0';
    fclose(f);
    return FileBuffer;
}


void Platform_SetScreenBufferDimensions(int Width, int Height)
{
    glfwSetWindowSize(sWindow, Width, Height);
}

void Platform_SetFrameTimeTarget(double MillisecPerFrame)
{
    sFrameTimeTargetS = MillisecPerFrame * 0.001;
}

void Platform_SetVSync(bool8 Enable)
{
    glfwSwapInterval(Enable != false);
}


static void OnFrameBufferResize(GLFWwindow *Window, int Width, int Height)
{
    (void)Window;
    glViewport(0, 0, Width, Height);
}

static void OnMouseMove(GLFWwindow *Window, double X, double Y)
{
    mouse_data Mouse = {
        .Event = MOUSE_MOVE,
        .Status.Move = {
            .X = X, 
            .Y = Y,
            .IsLeftClicking = glfwGetMouseButton(Window, GLFW_MOUSE_BUTTON_LEFT),
            .IsRightClicking = glfwGetMouseButton(Window, GLFW_MOUSE_BUTTON_RIGHT),
        },
    }; 
    App_OnMouseEvent(&sAppState, &Mouse);
}

static void OnMouseWheel(GLFWwindow *Window, double OffsetX, double OffsetY)
{
    (void)Window, (void)OffsetX;
    mouse_data Mouse = {
        .Event = MOUSE_WHEEL,
        .Status.Wheel = {
            .ScrollTowardUser = OffsetY < 0,
        },
    };
    App_OnMouseEvent(&sAppState, &Mouse);
}


static void OnKeyInput(GLFWwindow *Window, int GlfwKey, int Scancode, int Action, int Mod)
{
    (void)Window, (void)Mod, (void)Scancode;
    platform_key Key = 0;
    switch (GlfwKey)
    {
    case GLFW_KEY_LEFT_SHIFT: Key = PLATFORM_KEY_LEFT_SHIFT; break;
    case GLFW_KEY_UP: Key = PLATFORM_KEY_UP_ARROW; break;
    case GLFW_KEY_DOWN: Key = PLATFORM_KEY_DOWN_ARROW; break;
    default: return;
    }

    sLastKeyState[Key] = sCurrentKeyState[Key];
    sCurrentKeyState[Key] = Action;
}

bool8 Platform_IsKeyPressed(platform_key Key)
{
    return sLastKeyState[Key] == GLFW_PRESS && sCurrentKeyState[Key] == GLFW_RELEASE;
}

bool8 Platform_IsKeyDown(platform_key Key)
{
    return sCurrentKeyState[Key] == GLFW_PRESS || sCurrentKeyState[Key] == GLFW_REPEAT;
}


platform_window_dimensions Platform_GetWindowDimensions(void)
{
    platform_window_dimensions Window;
    glfwGetWindowSize(sWindow, &Window.Width, &Window.Height);
    return Window;
}

double Platform_GetElapsedTimeMs(void)
{
    double Now = glfwGetTime();
    return (Now - sStartTimeS) * 0.001;
}

double Platform_GetFrameTimeMs(void)
{
    return sFrameTimeMs;
}

void Platform_RequestRedraw(void)
{
    platform_window_dimensions Window = Platform_GetWindowDimensions();
    App_OnRedrawRequest(&sAppState, Window.Width, Window.Height);
    glfwSwapBuffers(sWindow);
}



int main(void)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    int DefaultWindowWidth = 1080;
    int DefaultWindowHeight = 720;
    sWindow = glfwCreateWindow(
        DefaultWindowWidth, 
        DefaultWindowHeight, 
        App_GetName(&sAppState), 
        NULL, NULL
    );
    if (!sWindow)
    {
        fprintf(stderr, "Unable to create window.\n");
        return 1;
    }
    glfwMakeContextCurrent(sWindow);
    glfwSwapInterval(0); /* disable 60fps cap */

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "Unable to initialize OpenGL functions (GLAD).\n");
        return 1;
    }
    glfwSetFramebufferSizeCallback(sWindow, OnFrameBufferResize);
    glfwSetScrollCallback(sWindow, OnMouseWheel);
    glfwSetCursorPosCallback(sWindow, OnMouseMove);
    glfwSetKeyCallback(sWindow, OnKeyInput);
    glViewport(0, 0, DefaultWindowWidth, DefaultWindowHeight);


    sStartTimeS = glfwGetTime();
    sAppState = App_OnEntry();

    double FrameTimeStart = glfwGetTime();
    double IdleTimeMs = 0;
    while (!glfwWindowShouldClose(sWindow))
    {
        double LoopStart = glfwGetTime();
        App_OnLoop(&sAppState);
        double LoopTimeMs = (glfwGetTime() - LoopStart) * 1000.0;

        double Now = glfwGetTime();
        double FrameTimeNowS = Now - FrameTimeStart;
        if (FrameTimeNowS < sFrameTimeTargetS)
        {
            IdleTimeMs = (sFrameTimeTargetS - FrameTimeNowS) * 1000.0;
            usleep(IdleTimeMs * 1000.0);
        }
        else
        {
            sFrameTimeMs = FrameTimeNowS * 1000.0;
            FrameTimeStart = Now;
        }
        glfwPollEvents();

        printf("\rt_idle|t_loop|t_frame: %3.3f|%3.3f|%3.3f, fps: %3.3f", 
            IdleTimeMs, 
            LoopTimeMs, 
            sFrameTimeMs, 
            1000.0 / sFrameTimeMs
        );
    }

    App_OnExit(&sAppState);
    glfwTerminate();
    return 0;
}

