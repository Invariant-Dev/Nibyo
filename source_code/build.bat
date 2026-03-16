@echo off
REM nibyo v1.0 beta build script for windows
REM usage: build.bat

echo  nibyo v1.0 beta - build script
echo.

REM check if g++ is available
where g++ >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo error: g++ not found. please install mingw-w64 or add it to path.
    pause
    exit /b 1
)

echo compiling nibyo v1.0 beta...
echo.

REM compile all .cpp files
g++ -std=c++17 -O3 ^
    Common.cpp ^
    Lexer.cpp ^
    Parser.cpp ^
    Interpreter.cpp ^
    main.cpp ^
    -o nibyo.exe ^
    -static -static-libgcc -static-libstdc++ -pthread

if %ERRORLEVEL% neq 0 (
    echo.
    echo build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo  build successful: nibyo.exe
echo ========================================
echo.
echo usage: .\nibyo.exe your_program.nb
echo.
pause
