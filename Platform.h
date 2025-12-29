#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include "Common.h"
#include "glad/glad.h"


typedef enum 
{
    PLATFORM_KEY_LEFT_SHIFT,
    PLATFORM_KEY_DOWN_ARROW,
    PLATFORM_KEY_UP_ARROW,
    PLATFORM_KEY_COUNT,
} platform_key;

typedef enum
{
    MOUSE_MOVE,
    MOUSE_WHEEL,
} mouse_flag;

typedef struct
{
    mouse_flag Event;
    union {
        struct {
            int X, Y;
            bool8 IsLeftClicking;
            bool8 IsRightClicking;
        } Move;
        struct {
            bool8 ScrollTowardUser;
        } Wheel;
    } Status;
} mouse_data;

typedef struct 
{
    i32 Width, Height;
} platform_window_dimensions;


typedef struct 
{
    float ScreenToWorldScaleFactor;
    float WorldBottom, WorldHeight;
    float WorldLeft, WorldWidth;
    int IterationCount;
    float TimeSinceLastIterationCountChange;
    float MouseX, MouseY;

    const char *FragmentShaderFileName;
    const char *VertexShaderFileName;
    float *ColorPalette;
    int ColorPaletteCount;
    GLuint ShaderProgramID;
    GLuint VAO;
} app_state;



// ==========================================
// Functions that the application must define
// ==========================================
/* main loop */
app_state App_OnEntry(void);
void App_OnLoop(app_state *State);
void App_OnExit(app_state *State);
/* event handlers */
void App_OnMouseEvent(app_state *State, const mouse_data *Mouse);
void App_OnRedrawRequest(app_state *State, int Width, int Height);
/* getter */
const char *App_GetName(app_state *State);


// ==========================================
// Functions that the platform must define
// ==========================================
/* setters */
void Platform_SetScreenBufferDimensions(int Width, int Height);
void Platform_SetFrameTimeTarget(double MillisecPerFrame);
void Platform_SetVSync(bool8 Enable);

/* getters */
platform_window_dimensions Platform_GetWindowDimensions(void);
double Platform_GetElapsedTimeMs(void); /* starting from right before App_OnEntry() */
double Platform_GetFrameTimeMs(void);
bool8 Platform_IsKeyDown(platform_key Key);
bool8 Platform_IsKeyPressed(platform_key Key);

/* event request */
void Platform_RequestRedraw(void);

/* misc */
int Platform_BeginTempMemory(void);
char *Platform_PushNullTerminatedFileContentBlocking(int *PlatformMemory, const char *FileName);
/* returned memory is aligned on 4-byte boundary */
void *Platform_PushMemory(int *PlatformMemory, int SizeBytes);
void Platform_PopMemory(int PlatformMemory);




#endif /* PLATFORM_H */

