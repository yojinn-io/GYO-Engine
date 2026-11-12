@echo off
rem lag-l2 + K8 session, one Windows player: double-click this file.
rem The kit builder (make_kit.sh) writes the machine name below.
setlocal
cd /d "%~dp0"
set NAME=@NAME@
echo === lag-l2 session: %NAME% ===
set /p MACIP=Mac IP address (shown in the Mac terminal):
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0windows.ps1" -MacIp %MACIP% -Name %NAME% -Package "%~dp0gyo-object_fps_pvp" -MovementDiag
echo.
echo Done. Hand back the file lag-l2-%NAME%*.zip in this folder.
pause
