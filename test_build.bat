@echo off
echo Testing Bio-Spheres Build...
echo.

echo Attempting to build the project...
echo Please open Visual Studio and try building again.
echo.

echo If you get more build errors, they will likely be similar include path issues.
echo Here's how to fix them:
echo.
echo 1. For each file with include errors, update the path like this:
echo    OLD: #include "config.h"
echo    NEW: #include "../../core/config.h"
echo.
echo 2. The number of "../" depends on how deep the file is:
echo    - Files in src/core/ don't need "../" for other core files
echo    - Files in src/rendering/camera/ need "../../" to reach src/
echo    - Files in src/rendering/core/mesh/ need "../../../" to reach src/
echo.
echo 3. Common fixes needed:
echo    config.h     -^> ../../core/config.h
echo    input.h      -^> ../../input/input.h  
echo    camera.h     -^> ../../rendering/camera/camera.h
echo    shader_class.h -^> ../../rendering/core/shader_class.h
echo.

pause
echo.
echo Opening Visual Studio for you to try building...
start "" "Biospheres.sln" 