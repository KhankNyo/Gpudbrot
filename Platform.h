#ifndef PLATFORM_H
#define PLATFORM_H

#include "Common.h"

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
            bool8 LButton;
            bool8 RButton;
        } Move;
        struct {
            bool8 ScrollTowardUser;
        } Wheel;
    } Status;
} mouse_data;

typedef struct
{
    u32 *Ptr;
    i32 Width, Height;
} platform_screen_buffer;

typedef struct 
{
    i32 Width, Height;
} platform_window_dimensions;


typedef struct 
{
    int Dummty;
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
void App_OnRedrawRequest(app_state *State, platform_screen_buffer *Ctx);
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
platform_screen_buffer Platform_GetScreenBuffer(void);
platform_window_dimensions Platform_GetWindowDimensions(void);
double Platform_GetElapsedTimeMs(void); /* starting from right before Graph_OnEntry() */
double Platform_GetFrameTimeMs(void);

/* event request */
void Platform_RequestRedraw(void);

/* misc */
void *Platform_AllocateMemory(uint Size);
void Platform_EnableExecution(void *Memory, uint ByteCount);
void Platform_DisableExecution(void *Memory, uint ByteCount);



#endif /* PLATFORM_H */

