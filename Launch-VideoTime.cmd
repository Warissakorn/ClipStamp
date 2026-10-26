@echo off
setlocal
if exist "%~dp0dist\VideoTime\VideoTime.exe" (
    start "" "%~dp0dist\VideoTime\VideoTime.exe" %*
    exit /b 0
)
if exist "%~dp0build\VideoTime.exe" (
    set "PATH=%~dp0.tools\Qt\6.8.3\mingw_64\bin;%~dp0.tools\Qt\Tools\mingw1310_64\bin;%PATH%"
    start "" "%~dp0build\VideoTime.exe" %*
    exit /b 0
)
echo VideoTime is not built yet.
echo Run: powershell -NoProfile -ExecutionPolicy Bypass -File tools\windows\build.ps1 -Setup -Package
pause
