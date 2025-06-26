@echo off
echo Testing Bio-Spheres Project Setup...
echo.

echo 1. Checking if solution file exists...
if exist "Biospheres.sln" (
    echo ✓ Biospheres.sln found
) else (
    echo ✗ Biospheres.sln NOT found
    goto :error
)

echo.
echo 2. Checking if project file exists...
if exist "Biospheres.vcxproj" (
    echo ✓ Biospheres.vcxproj found
) else (
    echo ✗ Biospheres.vcxproj NOT found
    goto :error
)

echo.
echo 3. Checking main.cpp...
if exist "main.cpp" (
    echo ✓ main.cpp found
) else (
    echo ✗ main.cpp NOT found
    goto :error
)

echo.
echo 4. Checking key source files...
if exist "src\simulation\cell\cell_manager.cpp" (
    echo ✓ cell_manager.cpp found
) else (
    echo ✗ cell_manager.cpp NOT found
    goto :error
)

if exist "third_party\imgui\imgui.cpp" (
    echo ✓ imgui.cpp found
) else (
    echo ✗ imgui.cpp NOT found
    goto :error
)

if exist "third_party\lib\glfw3.lib" (
    echo ✓ glfw3.lib found
) else (
    echo ✗ glfw3.lib NOT found
    goto :error
)

echo.
echo 5. Opening solution in Visual Studio...
start "" "Biospheres.sln"

echo.
echo ============================================
echo All files found! The solution should open.
echo.
echo NEXT STEPS:
echo 1. Wait for Visual Studio to load
echo 2. Right-click "Biospheres" in Solution Explorer
echo 3. Select "Set as Startup Project"
echo 4. Change configuration to "Debug" and "x64"
echo 5. Press F5 to build and run
echo ============================================
echo.
pause
goto :end

:error
echo.
echo ============================================
echo ERROR: Some files are missing!
echo Please check the file organization.
echo ============================================
pause

:end 