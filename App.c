#include <stdio.h>
#include "Platform.h"

static bool CompileShader(GLenum ShaderType, const char *ShaderProgram, GLuint *OutShaderID)
{
    *OutShaderID = glCreateShader(ShaderType);
    glShaderSource(*OutShaderID, 1, &ShaderProgram, NULL);
    glCompileShader(*OutShaderID);

    GLint Ok;
    glGetShaderiv(*OutShaderID, GL_COMPILE_STATUS, &Ok);
    return Ok;
}

static void ShaderSetVec3(GLint Program, const char *UniformName, const void *Values, int Count)
{
    GLint Location = glGetUniformLocation(Program, UniformName);
    glUniform3fv(Location, Count, Values);
}

static void ShaderSetFloat(GLint Program, const char *UniformName, const float *Values, int Count)
{
    GLint Location = glGetUniformLocation(Program, UniformName);
    glUniform1fv(Location, Count, Values);
}

static void ShaderSetInt(GLint Program, const char *UniformName, const GLint *Values, int Count)
{
    GLint Location = glGetUniformLocation(Program, UniformName);
    glUniform1iv(Location, Count, Values);
}

static GLint LoadShader(const char *FragmentShaderFileName, const char *VertexShaderFileName)
{
    GLuint ShaderProgramID = 0;
    int PlatformMemory = Platform_BeginTempMemory();
    const char *VertexShaderSource = Platform_PushNullTerminatedFileContentBlocking(&PlatformMemory, VertexShaderFileName);
    if (!VertexShaderSource)
    {
        fprintf(stderr, "Unable to open '%s'\n", VertexShaderFileName);
        goto Out;
    }
    const char *FragmentShaderSource = Platform_PushNullTerminatedFileContentBlocking(&PlatformMemory, FragmentShaderFileName);
    if (!FragmentShaderSource)
    {
        fprintf(stderr, "Unable to open '%s'\n", FragmentShaderFileName);
        goto Out;
    }

    GLuint VertexShaderID = 0;
    GLuint FragmentShaderID = 0;
    GLint ErrMsgCapacity = 0;
    if (CompileShader(GL_VERTEX_SHADER, VertexShaderSource, &VertexShaderID))
    {
        if (CompileShader(GL_FRAGMENT_SHADER, FragmentShaderSource, &FragmentShaderID))
        {
            ShaderProgramID = glCreateProgram();
            glAttachShader(ShaderProgramID, VertexShaderID);
            glAttachShader(ShaderProgramID, FragmentShaderID);
            glLinkProgram(ShaderProgramID);

            GLint LinkOk = false;
            glGetProgramiv(ShaderProgramID, GL_LINK_STATUS, &LinkOk);
            if (!LinkOk)
            {
                glGetProgramiv(ShaderProgramID, GL_INFO_LOG_LENGTH, &ErrMsgCapacity);
                char *ErrMsgBuffer = Platform_PushMemory(&PlatformMemory, ErrMsgCapacity);
                glGetProgramInfoLog(ShaderProgramID, ErrMsgCapacity, NULL, ErrMsgBuffer);
                fprintf(stderr, "\nShader program link error: \n%s\n", ErrMsgBuffer);
            }
            glDeleteShader(FragmentShaderID);
        }
        else
        {
            glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &ErrMsgCapacity);
            char *ErrMsgBuffer = Platform_PushMemory(&PlatformMemory, ErrMsgCapacity);
            glGetShaderInfoLog(FragmentShaderID, ErrMsgCapacity, NULL, ErrMsgBuffer);
            fprintf(stderr, "\nFragment shader compilation error: \n%s\n", ErrMsgBuffer);
        }
        glDeleteShader(VertexShaderID);
    }
    else
    {
        glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &ErrMsgCapacity);
        char *ErrMsgBuffer = Platform_PushMemory(&PlatformMemory, ErrMsgCapacity);
        glGetShaderInfoLog(VertexShaderID, ErrMsgCapacity, NULL, ErrMsgBuffer);
        fprintf(stderr, "\nVertex shader compilation error: \n%s\n", ErrMsgBuffer);
    }
Out:
    Platform_PopMemory(PlatformMemory);
    return ShaderProgramID;
}

app_state App_OnEntry(void)
{
    static float ColorPalette[16][3] = {
    #define FLOAT_ARRAY_RGB(r, g, b) {r/255.0f, g/255.0f, b/255.0f}
        [0] = FLOAT_ARRAY_RGB(   66,  30,  15 /* brown 3 */),
        [1] = FLOAT_ARRAY_RGB(   25,   7,  26 /* dark violett */),
        [2] = FLOAT_ARRAY_RGB(   9,   1,  47 /* darkest blue */),
        [3] = FLOAT_ARRAY_RGB(   4,   4,  73 /* blue 5 */),
        [4] = FLOAT_ARRAY_RGB(   0,   7, 100 /* blue 4 */),
        [5] = FLOAT_ARRAY_RGB(   12,  44, 138 /* blue 3 */),
        [6] = FLOAT_ARRAY_RGB(   24,  82, 177 /* blue 2 */),
        [7] = FLOAT_ARRAY_RGB(   57, 125, 209 /* blue 1 */),
        [8] = FLOAT_ARRAY_RGB(   134, 181, 229 /* blue 0 */),
        [9] = FLOAT_ARRAY_RGB(   211, 236, 248 /* lightest blue */),
        [10] = FLOAT_ARRAY_RGB(   241, 233, 191 /* lightest yellow */),
        [11] = FLOAT_ARRAY_RGB(   248, 201,  95 /* light yellow */),
        [12] = FLOAT_ARRAY_RGB(   255, 170,   0 /* dirty yellow */),
        [13] = FLOAT_ARRAY_RGB(   204, 128,   0 /* brown 0 */),
        [14] = FLOAT_ARRAY_RGB(   153,  87,   0 /* brown 1 */),
        [15] = FLOAT_ARRAY_RGB(   106,  52,   3 /* brown 2 */),
    #undef FLOAT_ARRAY_RGB
    };

    int Width = 1280, 
        Height = 720;
    Platform_SetScreenBufferDimensions(Width, Height);
    Platform_SetVSync(false);
    app_state App = { 
        .WorldLeft = -2.0f,
        .WorldBottom = -1.0f, 
        .WorldHeight = 2.0f,
        .WorldWidth = 3.0f,
        .IterationCount = 1024,

        .VertexShaderFileName = "VertexShader.glsl",
        .FragmentShaderFileName = "FragmentShader.glsl",
        .ColorPalette = (float *)ColorPalette,
        /* TODO: do this dynamically */
        .ColorPaletteCount = STATIC_ARRAY_SIZE(ColorPalette)*3,
    };
    App.ScreenToWorldScaleFactor = App.WorldWidth / Width;
    App.ShaderProgramID = LoadShader(App.FragmentShaderFileName, App.VertexShaderFileName);

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

    /* vertex attrib */
    glGenVertexArrays(1, &App.VAO);
    glBindVertexArray(App.VAO);
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
        glUseProgram(App.ShaderProgramID);
        {
            GLint VertexLocationInVertexShader = 0;
            glVertexAttribPointer(VertexLocationInVertexShader, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), NULL);
            glEnableVertexAttribArray(VertexLocationInVertexShader);
            ShaderSetVec3(App.ShaderProgramID, "u_ColorPalette", ColorPalette, STATIC_ARRAY_SIZE(ColorPalette));
        }
    }
    glBindVertexArray(0);

    return App;
}

void App_OnExit(app_state *State)
{
}


const char *App_GetName(app_state *State)
{
    (void)State;
    return "GPUdbrot";
}


void App_OnLoop(app_state *State)
{
    if (Platform_IsKeyPressed(PLATFORM_KEY_LEFT_SHIFT))
    /* reload shader */
    {
        glUseProgram(0);
        glDeleteProgram(State->ShaderProgramID);

        State->ShaderProgramID = LoadShader(State->FragmentShaderFileName, State->VertexShaderFileName);
        glUseProgram(State->ShaderProgramID);
        /* TODO: do this dynamically */
        ShaderSetVec3(State->ShaderProgramID, "u_ColorPalette", State->ColorPalette, State->ColorPaletteCount/3);
    }

    int NewIterationCount = State->IterationCount + Platform_IsKeyDown(PLATFORM_KEY_UP_ARROW);
    NewIterationCount -= (Platform_IsKeyDown(PLATFORM_KEY_DOWN_ARROW) && State->IterationCount > 0);
    /* can only modify iteration count every 20ms */
    bool8 CanModifyIterationCount = Platform_GetElapsedTimeMs() - State->TimeSinceLastIterationCountChange > 20.0/1000.0;
    if (CanModifyIterationCount 
    && NewIterationCount != State->IterationCount)
    {
        State->IterationCount = NewIterationCount;
        State->TimeSinceLastIterationCountChange = Platform_GetElapsedTimeMs();
    }

    Platform_RequestRedraw();
}


/* event handlers */
void App_OnMouseEvent(app_state *State, const mouse_data *Mouse)
{
    switch (Mouse->Event)
    {
    case MOUSE_MOVE:
    {
        float MouseX = Mouse->Status.Move.X;
        float MouseY = Mouse->Status.Move.Y;
        if (Mouse->Status.Move.IsLeftClicking)
        {
            float Dx = (MouseX - State->MouseX) * State->ScreenToWorldScaleFactor;
            float Dy = -(MouseY - State->MouseY) * State->ScreenToWorldScaleFactor;
            State->WorldLeft -= Dx;
            State->WorldBottom -= Dy;
        }
        State->MouseX = MouseX;
        State->MouseY = MouseY;
    } break;
    case MOUSE_WHEEL:
    {
        platform_window_dimensions Window = Platform_GetWindowDimensions();
        float WindowWidth = Window.Width;
        float WindowHeight = Window.Height;

        float Scale = 1.1;
        if (!Mouse->Status.Wheel.ScrollTowardUser)
            Scale = 1.0 / Scale; 

        float MouseX = State->MouseX * State->WorldWidth / WindowWidth + State->WorldLeft;
        float MouseY = State->WorldHeight - State->MouseY * State->WorldHeight / WindowHeight + State->WorldBottom;

        float ScaledLeft = (State->WorldLeft - MouseX)*Scale + MouseX;
        float ScaledBottom = (State->WorldBottom - MouseY)*Scale + MouseY;
        float ScaledWidth = State->WorldWidth * Scale;
        float ScaledHeight = State->WorldHeight * Scale;

        State->WorldLeft = ScaledLeft;
        State->WorldBottom = ScaledBottom;
        State->WorldWidth = ScaledWidth;
        State->WorldHeight = ScaledHeight;
        State->ScreenToWorldScaleFactor = State->WorldWidth / WindowWidth;
    } break;
    }
}

void App_OnRedrawRequest(app_state *State, int Width, int Height)
{
    glUseProgram(State->ShaderProgramID);
    glViewport(0, 0, Width, Height);
    glBindVertexArray(State->VAO);

    ShaderSetFloat(State->ShaderProgramID, "u_ScreenToWorldScaleFactor", &State->ScreenToWorldScaleFactor, 1);
    ShaderSetFloat(State->ShaderProgramID, "u_WorldBottom", &State->WorldBottom, 1);
    ShaderSetFloat(State->ShaderProgramID, "u_WorldLeft", &State->WorldLeft, 1);
    ShaderSetInt(State->ShaderProgramID, "u_IterationCount", &State->IterationCount, 1);

    glDrawElements(GL_TRIANGLES, 2*3, GL_UNSIGNED_INT, NULL);
}


