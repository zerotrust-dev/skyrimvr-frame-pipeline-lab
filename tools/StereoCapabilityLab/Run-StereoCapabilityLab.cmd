@echo off
setlocal
set "LAB_DIR=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%LAB_DIR%Invoke-StereoCapabilityLab.ps1" -Executable "%LAB_DIR%StereoCapabilityLab.exe" %*
exit /b %ERRORLEVEL%
