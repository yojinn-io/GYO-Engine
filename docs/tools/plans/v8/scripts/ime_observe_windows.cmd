@echo off
rem v8-tool-ui-ime-observe: starts the PvP client with the text input diagnostics.
rem Put this file next to gyo_object_fps_pvp.exe and double-click it.
rem The log is written to logs\client-<time>.log next to the executable.
"%~dp0gyo_object_fps_pvp.exe" --text-input-trace
