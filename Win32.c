
#include "glad/glad.h"
#include "Platform.h"
#include "ModernOpenGL.h"

/* TODO: unity build in a different file */
#include "glad/src/glad.c"
#include "App.c"

#include <stdio.h>
#include <windows.h>
/* must be inclued after window.h */
#include <gl/GL.h>

#define WIN32_KEYCODE_COUNT UINT8_MAX



typedef struct win32_context
{
    HWND Handle;
    int Width, Height;
} win32_context;

static win32_context sWin32_MainWindow;
static app_state sWin32_AppState;
static double sWin32_MsPerPerfCount;
static double sWin32_FrameTimeTargetMs;
static LARGE_INTEGER sWin32_PerfCountBegin;
static double sWin32_FrameTimeMs;
static uint8_t sWin32_WasKeyDown[WIN32_KEYCODE_COUNT];
static uint8_t sWin32_IsKeyDown[WIN32_KEYCODE_COUNT];
/* 
    memory format: 
        [sp - data size]:   data ...
        [sp]:               junk
*/
/* NOTE: uint32_t is the type to force 4-byte alignment */
static uint32_t sWin32_StackAllocatorMemory[4*MB / sizeof(uint32_t)]; 
static BYTE *sWin32_StackAllocatorTop = (BYTE *)sWin32_StackAllocatorMemory;


typedef void WINAPI wgl_swap_interval_ext(GLint);
typedef HGLRC WINAPI wgl_create_context_attribs_arb(HDC hDC, HGLRC hShareContext, const int *attribList);
wgl_swap_interval_ext *wglSwapInterval;


static void Win32_Fatal(const char *ErrorMessage)
{
    MessageBoxA(NULL, ErrorMessage, "Fatal Error", MB_ICONERROR);
    ExitProcess(1);
}

static void WINAPI Win32_Stub()
{
    printf("stub called\n");
}

static void *Win32_LoadWGLFunctionOrStub(const char *Name)
{
    void *Fn = (void *)wglGetProcAddress(Name);
    if (Fn == NULL)
    {
        return (void *)Win32_Stub;
    }
    return Fn;
}

static void Win32_DisplayBufferToWindow(HWND Window)
{
    platform_window_dimensions WindowDimensions = Platform_GetWindowDimensions();
    HDC WindowDC = GetDC(Window);
    {
        App_OnRedrawRequest(&sWin32_AppState, WindowDimensions.Width, WindowDimensions.Height);
        SwapBuffers(WindowDC);
    }
    ReleaseDC(Window, WindowDC);
}




static LRESULT CALLBACK Win32_MainWndProc(HWND Window, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
    case WM_CLOSE:
    {
        PostQuitMessage(0);
    } break;
    case WM_PAINT:
    {
        Win32_DisplayBufferToWindow(Window);
        PAINTSTRUCT PaintStruct;
        BeginPaint(Window, &PaintStruct);
        EndPaint(Window, &PaintStruct);
    } break;
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        uint8_t Keycode = wParam;
        sWin32_WasKeyDown[Keycode] = sWin32_IsKeyDown[Keycode];
        sWin32_IsKeyDown[Keycode] = Msg == WM_KEYDOWN;
    } break;
    case WM_MOUSEMOVE:
    {
        mouse_data Data = {
            .Event = MOUSE_MOVE,
            .Status.Move = {
                .X = (i16)LOWORD(lParam), 
                .Y = (i16)HIWORD(lParam),
                .IsLeftClicking = (wParam & MK_LBUTTON) != 0,
                .IsRightClicking = (wParam & MK_RBUTTON) != 0,
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
    memcpy(sWin32_WasKeyDown, sWin32_IsKeyDown, WIN32_KEYCODE_COUNT);
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
    /* 
        To get OpenGL up and running, 
        we need to create an old ass OpenGL context (version 1.1 or something, refer to Handmade Hero for this), 
        then we can query OpenGL for a newer version using wglCreateConextAttribsARB
    */
    HDC WindowDC = GetDC(Window);

    /* fudging with wgl to get the correct pixel format */
    PIXELFORMATDESCRIPTOR DesiredPixelFormat = {
        .nSize = sizeof(DesiredPixelFormat),
        .nVersion = 1,
        .dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER, 
        .cColorBits = 24,   /* Raymond Chen hasn't responded :( */
        .cAlphaBits = 8,    /* A */
        .iLayerType = PFD_MAIN_PLANE,
    };
    int SuggestedPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesiredPixelFormat);
    PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
    DescribePixelFormat(WindowDC, SuggestedPixelFormatIndex, sizeof(PIXELFORMATDESCRIPTOR), &SuggestedPixelFormat);
    SetPixelFormat(WindowDC, SuggestedPixelFormatIndex, &SuggestedPixelFormat);

    /* 
        create the old context, 
        we will discard this as soon as we can get our hands on the juicier and newer version 
    */
    HGLRC WiggleContext = wglCreateContext(WindowDC);
    if (!wglMakeCurrent(WindowDC, WiggleContext))
    {
        return FALSE;
    }

    /* load window's GL functions */
    wglSwapInterval = (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");
    wgl_create_context_attribs_arb *wglCreateContextAttribs = 
        (wgl_create_context_attribs_arb *)wglGetProcAddress("wglCreateContextAttribsARB");

    if (wglCreateContextAttribs)
    {
        /* we are in a modern version of OpenGL */
        static const GLint Attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4, 
            WGL_CONTEXT_MINOR_VERSION_ARB, 0, 
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            /* NOTE: forward compat means old stuff will not work */
            // WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB, 
            0
        };
        /* used for sharing texture data and such, not used here though */
        HGLRC SharedContext = NULL; 
        HGLRC ModernWiggleContext = wglCreateContextAttribs(WindowDC, SharedContext, Attribs);
        if (ModernWiggleContext)
        {
            /* escalate to the newer context */
            wglMakeCurrent(WindowDC, ModernWiggleContext);
            wglDeleteContext(WiggleContext);
            WiggleContext = ModernWiggleContext;
        }
        else
        {
            assert(ModernWiggleContext && "TODO: call GetLastError, too lazy for this");
        }
    }
    else
    {
        /* we are still in the old ass version of OpenGL */
    }

    /* TODO: write an OpenGL loader */
    if (!gladLoadGL())
    {
        return FALSE;
    }

    ReleaseDC(Window, WindowDC);
    return TRUE;
}

int WINAPI BogusWinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PCHAR CmdLine, int CmdShow);
int main(void)
{
    BogusWinMain(GetModuleHandle(NULL), NULL, NULL, 0);
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

    /* initialize timer */
    {
        LARGE_INTEGER Freq;
        QueryPerformanceFrequency(&Freq);
        sWin32_MsPerPerfCount = 1000.0 / Freq.QuadPart;
        QueryPerformanceCounter(&sWin32_PerfCountBegin);
    }

    /* window creation */
    Platform_SetScreenBufferDimensions(1080, 720);
    sWin32_MainWindow.Handle = CreateWindowExA(
        WS_EX_CLIENTEDGE, 
        "AppCls", 
        App_GetName(&sWin32_AppState), 
        WS_OVERLAPPEDWINDOW | WS_BORDER | WS_CAPTION, 
        CW_USEDEFAULT, 
        CW_USEDEFAULT, 
        sWin32_MainWindow.Width, 
        sWin32_MainWindow.Height,
        NULL, 
        NULL, 
        Instance, 
        NULL
    );
    if (NULL == sWin32_MainWindow.Handle)
    {
        Win32_Fatal("Unable to create window.");
    }
    if (!Win32_InitOpenGL(sWin32_MainWindow.Handle))
    {
        Win32_Fatal("Unable to initialize OpenGL, TODO: fallback to software rendering");
    }

    /* app entry point, after window creation and opengl initialization */
    sWin32_AppState = App_OnEntry();

    ShowWindow(sWin32_MainWindow.Handle, SW_SHOW);
    LARGE_INTEGER StartTime, EndTime;
    QueryPerformanceCounter(&StartTime);
    while (Win32_PollInputs())
    {
        App_OnLoop(&sWin32_AppState);
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
    (void)sWin32_MainWindow.Handle;
    (void)sWin32_MainWindow;

    ExitProcess(0);
}


void Platform_SetScreenBufferDimensions(int Width, int Height)
{
    sWin32_MainWindow.Width = Width;
    sWin32_MainWindow.Height = Height;
    if (sWin32_MainWindow.Handle)
    {
        SetWindowPos(
            sWin32_MainWindow.Handle, 
            NULL, 
            0, 0, Width, Height, 
            SWP_NOMOVE | SWP_NOREPOSITION | SWP_SHOWWINDOW
        );
    }
}



platform_window_dimensions Platform_GetWindowDimensions(void)
{
    RECT Rect;
    GetClientRect(sWin32_MainWindow.Handle, &Rect);
    return (platform_window_dimensions) {
        .Width = Rect.right - Rect.left,
        .Height = Rect.bottom - Rect.top,
    };
}

void Platform_RequestRedraw(void)
{
    if (sWin32_MainWindow.Handle)
    {
        InvalidateRect(sWin32_MainWindow.Handle, NULL, FALSE);
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

void Platform_SetVSync(bool8 EnableVSync)
{
    if (wglSwapInterval)
    {
        /* NOTE: this is not just a true or false value */
        int VBlankWaitCount = EnableVSync? 1 : 0;
        wglSwapInterval(VBlankWaitCount);
    }
}


double Platform_GetFrameTimeMs(void)
{
    return sWin32_FrameTimeMs;
}


int Platform_BeginTempMemory(void)
{
    return 0;
}

char *Platform_PushNullTerminatedFileContentBlocking(int *PlatformMemory, const char *FileName)
{
    HANDLE FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == FileHandle)
    {
        return NULL;
    }
    DWORD Low, High = 0;
    Low = GetFileSize(FileHandle, &High);
    uint64_t FileSize = Low | (uint64_t)High << 32;

    assert(FileSize < INT32_MAX);
    int FileContentBufferSize = FileSize + 1;
    char *FileContentBuffer = Platform_PushMemory(PlatformMemory, FileContentBufferSize);

    DWORD BytesRead;
    if (!ReadFile(FileHandle, FileContentBuffer, FileSize, &BytesRead, NULL)
    || FileSize != BytesRead)
    {
        *PlatformMemory -= FileContentBufferSize;
        Platform_PopMemory(FileContentBufferSize);
        return NULL;
    }

    FileContentBuffer[FileSize] = 0;

    CloseHandle(FileHandle);
    return FileContentBuffer;
}

void *Platform_PushMemory(int *PlatformMemory, int SizeBytes)
{
    int32_t AlignedSize = 0;
    if (SizeBytes > 0)
    {
        /* align size to 4-byte boundary */
        AlignedSize = (SizeBytes + sizeof(AlignedSize)) & ~0x3;
    }

    /* allocate the memory */
    void *Memory = sWin32_StackAllocatorTop;
    sWin32_StackAllocatorTop += AlignedSize;
    BYTE *StackEnd = (BYTE *)(sWin32_StackAllocatorMemory + STATIC_ARRAY_SIZE(sWin32_StackAllocatorMemory));
    assert(sWin32_StackAllocatorTop <= StackEnd && "Out of memory");

    (*PlatformMemory) += AlignedSize;
    return Memory;
}

void Platform_PopMemory(int SizeBytes)
{
    sWin32_StackAllocatorTop -= SizeBytes;
    assert(sWin32_StackAllocatorTop >= (BYTE *)sWin32_StackAllocatorMemory);
}


static uint8_t Win32_KeycodeFromPlatformKey(platform_key Key)
{
    static const uint8_t Lookup[PLATFORM_KEY_COUNT] = {
        [PLATFORM_KEY_LEFT_SHIFT] = VK_SHIFT,
        [PLATFORM_KEY_DOWN_ARROW] = VK_DOWN,
        [PLATFORM_KEY_UP_ARROW] = VK_UP,
    };
    return Lookup[Key];
}

bool8 Platform_IsKeyPressed(platform_key Key)
{
    uint8_t Keycode = Win32_KeycodeFromPlatformKey(Key);
    bool8 Pressed = sWin32_WasKeyDown[Keycode] && !sWin32_IsKeyDown[Keycode];
    return Pressed;
}

bool8 Platform_IsKeyDown(platform_key Key)
{
    uint8_t Keycode = Win32_KeycodeFromPlatformKey(Key);
    bool8 Down = sWin32_IsKeyDown[Keycode];
    return Down;
}

