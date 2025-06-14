@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
echo Building Biospheres 2 - Debug Configuration...
msbuild CellSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:SolutionDir="%~dp0" /verbosity:minimal
if %ERRORLEVEL% EQU 0 (
    echo Build succeeded!
) else (
    echo Build failed with error code %ERRORLEVEL%
)
pause
