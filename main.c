#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include <assert.h>
#include <stdlib.h> /* malloc, free */
#include <math.h>
#include <stdio.h> /* file io, fprintf */

#define STATIC_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define IN_RANGE(lower, n, upper) ((lower) <= (n) && (n) <= (upper))
#define KEY_BASE GLFW_KEY_SPACE
#define KEY_COUNT (GLFW_KEY_MENU - KEY_BASE)

typedef enum
{
    false = 0, 
    true
} bool;

static float gScreenToWorldScaleFactor = 3.0f / 1080.0f;
static float gWorldLeft = -2.0f;
static float gWorldBottom = -1.0f;
static float gWorldWidth = 3.0f;
static float gWorldHeight = 2.0f;
static float gMouseX, gMouseY;
static GLint gIterationCount = 1024;
static int gLastKeyState[KEY_COUNT];
static int gCurrentKeyState[KEY_COUNT];



typedef struct 
{
    GLuint ID;
} shader_program;

static bool CompileShader(GLenum ShaderType, const char *ShaderProgram, GLuint *OutShaderID)
{
    *OutShaderID = glCreateShader(ShaderType);
    glShaderSource(*OutShaderID, 1, &ShaderProgram, NULL);
    glCompileShader(*OutShaderID);

    GLint Ok;
    glGetShaderiv(*OutShaderID, GL_COMPILE_STATUS, &Ok);
    return Ok;
}

static size_t GetFileSize(FILE *f)
{
    size_t Offset = ftell(f);
    fseek(f, 0, SEEK_END);
    size_t Size = ftell(f);
    fseek(f, Offset, SEEK_SET);
    return Size;
}

shader_program LoadShader(const char *FragmentShaderPath, const char *VertexShaderPath)
{
    shader_program ShaderProgram = {
        .ID = 0,
    };
    size_t ErrMsgCapacity = 2048;

    /* read both files into a single buffer */
    char *ShaderProgramSrc = NULL;
    FILE *FragmentShaderFile = NULL, *VertexShaderFile = NULL;
    FragmentShaderFile = fopen(FragmentShaderPath, "rb");
    if (!FragmentShaderFile)
    {
        fprintf(stderr, "Unable to open '%s'.\n", FragmentShaderPath);
        goto Error;
    }
    VertexShaderFile = fopen(VertexShaderPath, "rb");
    if (!VertexShaderFile)
    {
        fprintf(stderr, "Unable to open '%s'.\n", VertexShaderPath);
        goto Error;
    }

    /* allocate memory for shader programs and error logging */
    size_t FragmentShaderFileSize = GetFileSize(FragmentShaderFile);
    size_t VertexShaderFileSize = GetFileSize(VertexShaderFile);
    size_t BytesToAlloc = FragmentShaderFileSize + VertexShaderFileSize + ErrMsgCapacity + 3; /* null tereminators */
    ShaderProgramSrc = malloc(BytesToAlloc);
    if (NULL == ShaderProgramSrc)
    {
        fprintf(stderr, "Unable to allocate memory for shaders (%zu bytes).\n", BytesToAlloc);
        goto Error;
    }

    /* partition the memory buffer */
    char *VertexShaderProgram = ShaderProgramSrc;
    char *FragmentShaderProgram = VertexShaderProgram + VertexShaderFileSize + 1;
    char *ErrMsg = FragmentShaderProgram + FragmentShaderFileSize + 1;

    /* read into the buffer */
    fread(VertexShaderProgram, 1, VertexShaderFileSize, VertexShaderFile);
    VertexShaderProgram[VertexShaderFileSize] = '\0';
    fread(FragmentShaderProgram, 1, FragmentShaderFileSize, FragmentShaderFile);
    FragmentShaderProgram[FragmentShaderFileSize] = '\0';

    GLuint VertexShader, FragmentShader;
    if (!CompileShader(GL_VERTEX_SHADER, VertexShaderProgram, &VertexShader))
    {
        glGetShaderInfoLog(VertexShader, ErrMsgCapacity, NULL, ErrMsg);
        fprintf(stderr, "Vertex shader compilation error: \n%s\n", ErrMsg);
        goto Error;
    }

    if (CompileShader(GL_FRAGMENT_SHADER, FragmentShaderProgram, &FragmentShader))
    {
        ShaderProgram.ID = glCreateProgram();
        glAttachShader(ShaderProgram.ID, VertexShader);
        glAttachShader(ShaderProgram.ID, FragmentShader);
        glLinkProgram(ShaderProgram.ID);

        GLint IsShaderProgramUsable = false;
        glGetProgramiv(ShaderProgram.ID, GL_LINK_STATUS, &IsShaderProgramUsable);
        if (!IsShaderProgramUsable)
        {
            glGetProgramInfoLog(ShaderProgram.ID, ErrMsgCapacity, NULL, ErrMsg);
            fprintf(stderr, "Shader program link error: \n%s\n", ErrMsg);
        }
        glDeleteShader(FragmentShader);
    }
    else
    {
        glGetShaderInfoLog(FragmentShader, ErrMsgCapacity, NULL, ErrMsg);
        fprintf(stderr, "Fragment shader compilation error: \n%s\n", ErrMsg);
    }
    glDeleteShader(VertexShader);

Error:
    fclose(FragmentShaderFile);
    fclose(VertexShaderFile);
    free(ShaderProgramSrc);
    return ShaderProgram;
}

void UnloadShader(shader_program Program)
{
    glUseProgram(0);
    glDeleteProgram(Program.ID);
}

void UseShader(shader_program Program)
{
    glUseProgram(Program.ID);
}

void ShaderSetFloat(shader_program Program, const char *UniformName, const float *Values, int Count)
{
    GLint Location = glGetUniformLocation(Program.ID, UniformName);
    glUniform1fv(Location, Count, Values);
}

void ShaderSetInt(shader_program Program, const char *UniformName, const GLint *Values, int Count)
{
    GLint Location = glGetUniformLocation(Program.ID, UniformName);
    glUniform1iv(Location, Count, Values);
}

void ShaderSetVec3(shader_program Program, const char *UniformName, const void *Values, int Count)
{
    GLint Location = glGetUniformLocation(Program.ID, UniformName);
    glUniform3fv(Location, Count, Values);
}



static void OnFrameBufferResize(GLFWwindow *Window, int Width, int Height)
{
    (void)Window;
    gScreenToWorldScaleFactor = gWorldWidth / Width;
    glViewport(0, 0, Width, Height);
}

static void OnMouseScroll(GLFWwindow *Window, double OffsetX, double OffsetY)
{
    (void)OffsetX;
    int Width, Height;
    glfwGetWindowSize(Window, &Width, &Height);

    float Scale = 1.1;
    if (OffsetY > 0.0)
        Scale = 1.0 / Scale; 

    float MouseX = gMouseX * gWorldWidth / Width + gWorldLeft;
    float MouseY = gWorldHeight - gMouseY * gWorldHeight / Height + gWorldBottom;

    float ScaledLeft = (gWorldLeft - MouseX)*Scale + MouseX;
    float ScaledBottom = (gWorldBottom - MouseY)*Scale + MouseY;
    float ScaledWidth = gWorldWidth * Scale;
    float ScaledHeight = gWorldHeight * Scale;

    gWorldLeft = ScaledLeft;
    gWorldBottom = ScaledBottom;
    gWorldWidth = ScaledWidth;
    gWorldHeight = ScaledHeight;
    gScreenToWorldScaleFactor = gWorldWidth / Width;
}


static void OnKeyInput(GLFWwindow *Window, int Key, int Scancode, int Action, int Mod)
{
    (void)Window, (void)Mod, (void)Scancode;

    Key -= KEY_BASE;
    assert(IN_RANGE(0, Key, KEY_COUNT));
    gLastKeyState[Key] = gCurrentKeyState[Key];
    gCurrentKeyState[Key] = Action;
}

static bool IsKeyPress(int Key)
{
    Key -= KEY_BASE;
    assert(IN_RANGE(0, Key, KEY_COUNT));
    return gLastKeyState[Key] == GLFW_PRESS && gCurrentKeyState[Key] == GLFW_RELEASE;
}
static bool IsKeyDown(int Key)
{
    Key -= KEY_BASE;
    assert(IN_RANGE(0, Key, KEY_COUNT));
    return gCurrentKeyState[Key] == GLFW_PRESS || gCurrentKeyState[Key] == GLFW_REPEAT;
}


int main(void)
{
    printf("Hello, world\n");
    return 0;

    int WindowWidth = 1080, WindowHeight = 720;
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow *Window = glfwCreateWindow(WindowWidth, WindowHeight, "Hello, world", NULL, NULL);
    if (!Window)
    {
        printf("Unable to create window.\n");
        return 1;
    }
    glfwMakeContextCurrent(Window);
    glfwSwapInterval(0); /* disable 60fps cap */

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        printf("Unable to initialize GLAD.\n");
        return 1;
    }
    glfwSetFramebufferSizeCallback(Window, OnFrameBufferResize);
    glfwSetScrollCallback(Window, OnMouseScroll);
    glfwSetKeyCallback(Window, OnKeyInput);


    /* opengl stuff */
    glViewport(0, 0, WindowWidth, WindowHeight);

    /* shader(s) */
    shader_program ShaderProgram = LoadShader(
        "FragmentShader.glsl", 
        "VertexShader.glsl"
    );

    /* VAO, VBO, EBO */
    float VertexBuffer[] = {
        -1.0f, 1.0f, 0.0f, 
        1.0f, 1.0f, 0.0f, 
        1.0f, -1.0f, 0.0f, 
        -1.0f, -1.0f, 0.0f, 
    };
    unsigned Indices[] = {
        0, 1, 2, 
        2, 3, 0,
    };
    float ColorPalette[16][3] = {
    #define RGB(r, g, b) {r/255.0f, g/255.0f, b/255.0f}
        [0] = RGB(   66,  30,  15 /* brown 3 */),
        [1] = RGB(   25,   7,  26 /* dark violett */),
        [2] = RGB(   9,   1,  47 /* darkest blue */),
        [3] = RGB(   4,   4,  73 /* blue 5 */),
        [4] = RGB(   0,   7, 100 /* blue 4 */),
        [5] = RGB(   12,  44, 138 /* blue 3 */),
        [6] = RGB(   24,  82, 177 /* blue 2 */),
        [7] = RGB(   57, 125, 209 /* blue 1 */),
        [8] = RGB(   134, 181, 229 /* blue 0 */),
        [9] = RGB(   211, 236, 248 /* lightest blue */),
        [10] = RGB(   241, 233, 191 /* lightest yellow */),
        [11] = RGB(   248, 201,  95 /* light yellow */),
        [12] = RGB(   255, 170,   0 /* dirty yellow */),
        [13] = RGB(   204, 128,   0 /* brown 0 */),
        [14] = RGB(   153,  87,   0 /* brown 1 */),
        [15] = RGB(   106,  52,   3 /* brown 2 */),
    #undef RGB
    };

    /* vertex attrib */
    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    {
        /* vertex buffer object */
        GLuint VBO;
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof VertexBuffer, VertexBuffer, GL_STATIC_DRAW);

        /* element buffer object */
        GLuint EBO;
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof Indices, Indices, GL_STATIC_DRAW);

        /* data to the gpu */
        UseShader(ShaderProgram);
        {
            GLint VertexLocationInVertexShader = 0;
            glVertexAttribPointer(VertexLocationInVertexShader, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), NULL);
            glEnableVertexAttribArray(VertexLocationInVertexShader);

            ShaderSetVec3(ShaderProgram, "u_ColorPalette", ColorPalette, STATIC_ARRAY_SIZE(ColorPalette));
        }
    }
    /* TODO: is unbinding VAO expensive? (ie calling glBindVertexArray(0) to unbind current VAO) */
    glBindVertexArray(0);

    bool WireframeMode = false;
    if (WireframeMode)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    double Prev = glfwGetTime();
    double TimeSinceLastIterationCountChange = 0;
    while (!glfwWindowShouldClose(Window))
    {
        /* timer */
        double Now = glfwGetTime();
        double FrameTime = Now - Prev;
        Prev = Now;
        printf("\rFrame time: %fms, (%fFPS); Iterations: %d, up is down: %d            ", 
            FrameTime*1000.0, 
            1.0 / FrameTime, 
            gIterationCount,
            IsKeyDown(GLFW_KEY_UP)
        );

        /* inputs */
        {
            /* mosue inputs */
            double MouseX, MouseY;
            glfwGetCursorPos(Window, &MouseX, &MouseY);
            if (glfwGetMouseButton(Window, GLFW_MOUSE_BUTTON_LEFT))
            {
                float Dx = (MouseX - gMouseX) * gScreenToWorldScaleFactor;
                float Dy = -(MouseY - gMouseY) * gScreenToWorldScaleFactor;
                gWorldLeft -= Dx;
                gWorldBottom -= Dy;
            }
            gMouseX = MouseX;
            gMouseY = MouseY;
            /* reload shader when shift key is pressed */
            if (IsKeyPress(GLFW_KEY_LEFT_SHIFT))
            {
                UnloadShader(ShaderProgram);
                ShaderProgram = LoadShader(
                    "FragmentShader.glsl", 
                    "VertexShader.glsl"
                );
                UseShader(ShaderProgram);
                ShaderSetVec3(ShaderProgram, "u_ColorPalette", ColorPalette, STATIC_ARRAY_SIZE(ColorPalette));
            }

            int NewIterationCount = gIterationCount + IsKeyDown(GLFW_KEY_UP);
            NewIterationCount = NewIterationCount - IsKeyDown(GLFW_KEY_DOWN);
            /* can only modify iteration count every 10ms */
            bool CanModifyIterationCount = glfwGetTime() - TimeSinceLastIterationCountChange > 10.0/1000.0;
            if (CanModifyIterationCount && NewIterationCount != gIterationCount)
            {
                gIterationCount = NewIterationCount;
                TimeSinceLastIterationCountChange = glfwGetTime();
            }
        }


        /* rendering commands */
        UseShader(ShaderProgram);
        glBindVertexArray(VAO);
        glClearColor(.2f, .3f, .3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ShaderSetFloat(ShaderProgram, "u_ScreenToWorldScaleFactor", &gScreenToWorldScaleFactor, 1);
        ShaderSetFloat(ShaderProgram, "u_WorldBottom", &gWorldBottom, 1);
        ShaderSetFloat(ShaderProgram, "u_WorldLeft", &gWorldLeft, 1);
        ShaderSetInt(ShaderProgram, "u_IterationCount", &gIterationCount, 1);

        glDrawElements(GL_TRIANGLES, 2*3, GL_UNSIGNED_INT, NULL);

        /* glfw stuff */
        glfwSwapBuffers(Window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}

