#include "Platform.h"

app_state App_OnEntry(void)
{
    app_state App = { 0 };
    Platform_SetScreenBufferDimensions(1080, 720);
    Platform_SetVSync(false);
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
}


/* event handlers */
void App_OnMouseEvent(app_state *State, const mouse_data *Mouse)
{
}

void App_OnRedrawRequest(app_state *State, platform_screen_buffer *Ctx)
{
}


