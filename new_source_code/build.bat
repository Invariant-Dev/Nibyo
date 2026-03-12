@echo off
REM Nexo v5.0 Build Script for Windows
REM Usage: build.bat

echo  Nexo v5.0 - Build Script
echo.

REM Check if g++ is available
where g++ >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: g++ not found. Please install MinGW-w64 or add it to PATH.
    pause
    exit /b 1
)

echo Compiling Nexo v5.0...
echo.

REM Compile all .cpp files
g++ -std=c++17 -O3 ^
    Common.cpp ^
    Lexer.cpp ^
    Parser.cpp ^
    Interpreter.cpp ^
    main.cpp ^
    -o nexo.exe ^
    -static -static-libgcc -static-libstdc++ -pthread

if %ERRORLEVEL% neq 0 (
    echo.
    echo Build FAILED!
    pause
    exit /b 1
)

echo.
echo ========================================
echo  Build successful: nexo.exe
echo ========================================
echo.
echo Usage: .\nexo.exe your_program.nx
echo.
pause
