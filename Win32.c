
#include "Platform.h"
#include "glad/glad.h"
/* TODO: unity build in a different file */
#include "glad/src/glad.c"
#include "App.c"

#include <stdio.h>
#include <stdarg.h>
#include <windows.h>
/* must be inclued after window.h */
#include <gl/GL.h>



typedef struct win32_context
{
    BITMAPINFO BitmapInfo;
    platform_screen_buffer BackBuffer;
} win32_context;

static HWND sWin32_MainWindowHandle;
static win32_context sWin32_Ctx;
static BOOL sWin32_WindowInitialized = FALSE;
static app_state sWin32_AppState;
static double sWin32_MsPerPerfCount;
static double sWin32_FrameTimeTargetMs;
static BOOL sWin32_VSync;
static LARGE_INTEGER sWin32_PerfCountBegin;
static double sWin32_FrameTimeMs;


typedef void wgl_swap_interval_ext(GLint);
wgl_swap_interval_ext *wglSwapInterval;


static void Win32_Fatal(const char *ErrorMessage)
{
    MessageBoxA(NULL, ErrorMessage, "Fatal Error", MB_ICONERROR);
    ExitProcess(1);
}

static win32_context Win32_CreateBackBuffer(int Width, int Height)
{
    win32_context Ctx = { 
        .BitmapInfo.bmiHeader = {
            .biSize = sizeof(BITMAPINFO),
            .biWidth = Width,
            .biHeight = -Height, 
            .biBitCount = 32,
            .biPlanes = 1,
            .biCompression = BI_RGB,
        },
        .BackBuffer.Width = Width, 
        .BackBuffer.Height = Height,
    };
    Ctx.BackBuffer.Ptr = VirtualAlloc(
        NULL, 
        Width*Height * sizeof(UINT32), 
        MEM_COMMIT, 
        PAGE_READWRITE
    );
    if (NULL == Ctx.BackBuffer.Ptr)
    {
        Win32_Fatal("Out of memory");
    }
    return Ctx;
}

static void WINAPI Win32_Stub()
{
    printf("stub called\n");
}

static void *Win32_LoadFunctionOrStub(PROC (WINAPI *Load)(const LPCSTR), const char *Name)
{
    void *Fn = (void *)Load(Name);
    if (Fn == NULL)
    {
        return (void *)Win32_Stub;
    }
    return Fn;
}

static void Win32_DisplayBufferToWindow(HWND Window, platform_screen_buffer *Buffer)
{
#if 0
    PAINTSTRUCT PaintStruct;
    HDC DeviceContext = BeginPaint(Window, &PaintStruct);
    int Left = PaintStruct.rcPaint.left;
    int Top = PaintStruct.rcPaint.top;
    int Width = PaintStruct.rcPaint.right - PaintStruct.rcPaint.left;
    int Height = PaintStruct.rcPaint.bottom - PaintStruct.rcPaint.top;

    StretchDIBits(
        DeviceContext, 
        Left, Top,
        Width, Height, 
        0, 0, 
        sWin32_Ctx.BackBuffer.Width, sWin32_Ctx.BackBuffer.Height, 
        sWin32_Ctx.BackBuffer.Ptr, 
        &sWin32_Ctx.BitmapInfo, 
        DIB_RGB_COLORS, SRCCOPY
    );
    EndPaint(Window, &PaintStruct);
#else
    platform_window_dimensions WindowDimensions = Platform_GetWindowDimensions();
    HDC WindowDC = GetDC(Window);
    {
        glViewport(0, 0, WindowDimensions.Width, WindowDimensions.Height);
        glClearColor(.2f, .3f, .4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        SwapBuffers(WindowDC);
    }
    ReleaseDC(Window, WindowDC);
#endif
}

static void Win32_DebugPrintf(const char *Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    char Buf[1024];
    vsnprintf(Buf, sizeof Buf, Fmt, Args);
    OutputDebugStringA(Buf);
    va_end(Args);
}





static LRESULT CALLBACK Win32_MainWndProc(HWND Window, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
    case WM_CLOSE:
    {
        PostQuitMessage(0);
    } break;
    case WM_SIZE:
    {
        if (sWin32_WindowInitialized)
        {
            App_OnRedrawRequest(&sWin32_AppState, &sWin32_Ctx.BackBuffer);
            Win32_DisplayBufferToWindow(Window, &sWin32_Ctx.BackBuffer);
        }
    } break;
    case WM_MOUSEMOVE:
    {
        mouse_data Data = {
            .Event = MOUSE_MOVE,
            .Status.Move = {
                .X = (i16)LOWORD(lParam), 
                .Y = (i16)HIWORD(lParam),
                .LButton = (wParam & MK_LBUTTON) != 0,
                .RButton = (wParam & MK_RBUTTON) != 0,
            },
        };
        App_OnMouseEvent(&sWin32_AppState, &Data);
    } break;
    case WM_MOUSEWHEEL:
    {
        mouse_data Data = {
            .Event = MOUSE_WHEEL,
            .Status.Wheel = {
                .ScrollTowardUser = (SHORT)HIWORD(wParam) < 0,
            },
        };
        App_OnMouseEvent(&sWin32_AppState, &Data);
    } break;
    default: 
        return DefWindowProcA(Window, Msg, wParam, lParam);
    }
    return 0;
}


static BOOL Win32_PollInputs(void)
{
    MSG Msg;
    while (PeekMessageA(&Msg, 0, 0, 0, PM_REMOVE) > 0)
    {
        if (Msg.message == WM_QUIT)
            return FALSE;

        TranslateMessage(&Msg);
        DispatchMessageA(&Msg);
    }
    return TRUE;
}

static BOOL Win32_InitOpenGL(HWND Window)
{
    HDC WindowDC = GetDC(Window);

    PIXELFORMATDESCRIPTOR DesiredPixelFormat = {
        .nSize = sizeof(DesiredPixelFormat),
        .nVersion = 1,
        .dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER, 
        .cColorBits = 24,   /* RGB */
        .cAlphaBits = 8,    /* A */
        .iLayerType = PFD_MAIN_PLANE,
    };
    int SuggestedPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesiredPixelFormat);
    PIXELFORMATDESCRIPTOR SuggestedPixelFormat = { 0 };
    DescribePixelFormat(WindowDC, SuggestedPixelFormatIndex, sizeof(PIXELFORMATDESCRIPTOR), &SuggestedPixelFormat);
    SetPixelFormat(WindowDC, SuggestedPixelFormatIndex, &SuggestedPixelFormat);

    HGLRC WiggleContext = wglCreateContext(WindowDC);
    if (!wglMakeCurrent(WindowDC, WiggleContext))
    {
        return FALSE;
    }

    /* TODO: write an OpenGL loader */
    if (!gladLoadGL())
    {
        return FALSE;
    }

    /* enable vsync if the app did not specify a framee time target */
    wglSwapInterval = (wgl_swap_interval_ext *)Win32_LoadFunctionOrStub(wglGetProcAddress, "wglSwapIntervalEXT");
    if (sWin32_VSync)
    {
        wglSwapInterval(1);
    }
    else
    {
        wglSwapInterval(0);
    }

    ReleaseDC(Window, WindowDC);
    return TRUE;
}

int WINAPI BogusWinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PCHAR CmdLine, int CmdShow);
int main(void)
{
    BogusWinMain(NULL, NULL, NULL, 0);
}

int WINAPI BogusWinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PCHAR CmdLine, int CmdShow)
{
    assert(sizeof(void *) == 8);
    (void)PrevInstance, (void)CmdLine, (void)CmdShow;
    WNDCLASSEXA WindowClass = {
        .cbSize = sizeof WindowClass,
        .hInstance = Instance,
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = Win32_MainWndProc,
        .lpszClassName = "AppCls",
        .hCursor = LoadCursorA(NULL, (LPSTR)IDC_ARROW),
    };
    RegisterClassExA(&WindowClass);

    /* initializer timer */
    {
        LARGE_INTEGER Freq;
        QueryPerformanceFrequency(&Freq);
        sWin32_MsPerPerfCount = 1000.0 / Freq.QuadPart;
        QueryPerformanceCounter(&sWin32_PerfCountBegin);
    }

    /* graph entry & window creation */
    sWin32_AppState = App_OnEntry();
    /* app has not call Platform_SetScreenBufferDimensions, set it to a default. */
    if (NULL == sWin32_Ctx.BackBuffer.Ptr)
    {
        Platform_SetScreenBufferDimensions(1080, 720);
    }
    sWin32_MainWindowHandle = CreateWindowExA(
        WS_EX_CLIENTEDGE, 
        "AppCls", 
        App_GetName(&sWin32_AppState), 
        WS_OVERLAPPEDWINDOW | WS_BORDER | WS_CAPTION, 
        CW_USEDEFAULT, 
        CW_USEDEFAULT, 
        sWin32_Ctx.BackBuffer.Width, 
        sWin32_Ctx.BackBuffer.Height,
        NULL, 
        NULL, 
        Instance, 
        NULL
    );
    if (NULL == sWin32_MainWindowHandle)
    {
        Win32_Fatal("Unable to create window.");
    }
    if (!Win32_InitOpenGL(sWin32_MainWindowHandle))
    {
        Win32_Fatal("Unable to initialize OpenGL, TODO: fallback to software rendering");
    }


    sWin32_WindowInitialized = TRUE;
    ShowWindow(sWin32_MainWindowHandle, SW_SHOW);
    LARGE_INTEGER StartTime, EndTime;
    QueryPerformanceCounter(&StartTime);
    while (Win32_PollInputs())
    {
        App_OnLoop(&sWin32_AppState);
        Win32_DisplayBufferToWindow(sWin32_MainWindowHandle, &sWin32_Ctx.BackBuffer);
        printf("\rfps: %f", 1000.0f / sWin32_FrameTimeMs);

        QueryPerformanceCounter(&EndTime);
        sWin32_FrameTimeMs = (EndTime.QuadPart - StartTime.QuadPart) * sWin32_MsPerPerfCount;
        if (sWin32_FrameTimeMs - 1 < sWin32_FrameTimeTargetMs)
        {
            LARGE_INTEGER SleepStart, SleepEnd;
            QueryPerformanceCounter(&SleepStart);
            Sleep(sWin32_FrameTimeTargetMs - sWin32_FrameTimeMs + 1);
            QueryPerformanceCounter(&SleepEnd);
            sWin32_FrameTimeMs += (SleepEnd.QuadPart - SleepStart.QuadPart) * sWin32_MsPerPerfCount;
        }
        QueryPerformanceCounter(&StartTime);
    }
    App_OnExit(&sWin32_AppState);

    /* don't need to cleanup, windows does it faster */
    (void)sWin32_MainWindowHandle;
    (void)sWin32_Ctx;

    ExitProcess(0);
}


void Platform_SetScreenBufferDimensions(int Width, int Height)
{
    platform_screen_buffer *Ctx = &sWin32_Ctx.BackBuffer;
    if (Ctx->Ptr)
    {
        int Size = Ctx->Width * Ctx->Height;
        VirtualFree(sWin32_Ctx.BackBuffer.Ptr, Size, MEM_RELEASE);
    }
    sWin32_Ctx = Win32_CreateBackBuffer(Width, Height);
    // TODO: redraw request? 
}


platform_screen_buffer Platform_GetScreenBuffer(void)
{
    return sWin32_Ctx.BackBuffer;
}

platform_window_dimensions Platform_GetWindowDimensions(void)
{
    RECT Rect;
    GetClientRect(sWin32_MainWindowHandle, &Rect);
    return (platform_window_dimensions) {
        .Width = Rect.right - Rect.left,
        .Height = Rect.bottom - Rect.top,
    };
}

void Platform_RequestRedraw(void)
{
    if (sWin32_WindowInitialized)
    {
        App_OnRedrawRequest(&sWin32_AppState, &sWin32_Ctx.BackBuffer);
        InvalidateRect(sWin32_MainWindowHandle, NULL, FALSE);
    }
}

double Platform_GetElapsedTimeMs(void)
{
    LARGE_INTEGER Count;
    QueryPerformanceCounter(&Count);
    return (Count.QuadPart - sWin32_PerfCountBegin.QuadPart) * sWin32_MsPerPerfCount;
}

void Platform_SetFrameTimeTarget(double MillisecPerFrame)
{
    sWin32_FrameTimeTargetMs = MillisecPerFrame;
}

void Platform_SetVSync(bool8 Enable)
{
    sWin32_VSync = Enable;
}


double Platform_GetFrameTimeMs(void)
{
    return sWin32_FrameTimeMs;
}

void *Platform_AllocateMemory(uint Size)
{
    SYSTEM_INFO Info;
    GetSystemInfo(&Info);
    if (Size % Info.dwPageSize)
    {
        /* round to highest page size */
        Size = (Size + Info.dwPageSize) / Info.dwPageSize * Info.dwPageSize;
    }
    void *Ptr = VirtualAlloc(NULL, Size, MEM_COMMIT, PAGE_READWRITE);
    assert(Ptr && "Memory allocation failed");

    return Ptr;
}

void Platform_EnableExecution(void *Memory, uint ByteCount)
{
    DWORD OldPerms;
    VirtualProtect(Memory, ByteCount, PAGE_EXECUTE_READ, &OldPerms);
}

void Platform_DisableExecution(void *Memory, uint ByteCount)
{
    DWORD OldPerms;
    VirtualProtect(Memory, ByteCount, PAGE_READWRITE, &OldPerms);
}


