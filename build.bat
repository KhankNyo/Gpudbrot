@echo off

if "clean"=="%1" (
    if exist bin\ rmdir /q /s bin
    echo ======================
    echo Removed build binaries
    echo ======================
) else (
    if not exist bin\ mkdir bin

    rem uhhh hopefully people have msvc installed in their C drive 
    if "%VisualStudioVersion%"=="" call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

    pushd bin
        if 0==0 (
            if 0==0 (
                tcc -D_CRT_SECURE_NO_WARNINGS -o main.exe ..\Win32.c^
                    -I"..\external" -I"..\external\glad\include" ^
                    -luser32 -lkernel32 -lopengl32 -lmsvcrt -lgdi32 -lshell32
            ) else (
                cl /Femain.exe ..\Win32.c^
                    /I"..\external" -I"..\external\glad\include" ^
                    user32.lib kernel32.lib opengl32.lib gdi32.lib
            )
        ) else (
            cl /Femain.exe ..\main.c ..\external\glad\src\glad.c ^
                /I"..\external" /I"..\external\glad\include" ^
                ..\external\glfw3.lib user32.lib kernel32.lib opengl32.lib gdi32.lib msvcrt.lib shell32.lib
        )
    popd

    echo ======================
    echo Build finished
    echo ======================
)

