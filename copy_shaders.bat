@echo off
echo Copying shaders to build directory...

REM Copy shaders to Debug directory
if exist "Debug" (
    xcopy "shaders" "Debug\shaders" /E /I /Y
    echo Copied shaders to Debug directory
)

REM Copy shaders to Release directory
if exist "x64\Debug" (
    xcopy "shaders" "x64\Debug\shaders" /E /I /Y
    echo Copied shaders to x64\Debug directory
)

if exist "x64\Release" (
    xcopy "shaders" "x64\Release\shaders" /E /I /Y
    echo Copied shaders to x64\Release directory
)

REM Copy shaders to Win32 directories if they exist
if exist "Win32\Debug" (
    xcopy "shaders" "Win32\Debug\shaders" /E /I /Y
    echo Copied shaders to Win32\Debug directory
)

if exist "Win32\Release" (
    xcopy "shaders" "Win32\Release\shaders" /E /I /Y
    echo Copied shaders to Win32\Release directory
)

echo Shader copy complete!
pause 