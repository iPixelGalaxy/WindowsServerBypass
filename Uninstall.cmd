@echo off
net session >nul 2>&1
if %errorLevel% == 0 goto :run
powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
exit /b
:run
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "Install.ps1" -uninstall
