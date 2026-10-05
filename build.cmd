@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 (
    echo Run this script from the x64 Native Tools Command Prompt for Visual Studio.
    exit /b 1
)
cd /d "%~dp0"
if not exist build mkdir build
pushd build
cl /nologo /std:c++17 /EHsc /W4 /O2 /MT /utf-8 /DUNICODE /D_UNICODE /I"..\imgui" /I"..\imgui\backends" ..\main.cpp ..\imgui\imgui.cpp ..\imgui\imgui_draw.cpp ..\imgui\imgui_tables.cpp ..\imgui\imgui_widgets.cpp ..\imgui\backends\imgui_impl_win32.cpp ..\imgui\backends\imgui_impl_dx11.cpp /Fe:JampusClient.exe /link /SUBSYSTEM:WINDOWS
set "jampusBuildResult=%errorlevel%"
popd
exit /b %jampusBuildResult%
