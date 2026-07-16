@echo off
setlocal
cd /d "%~dp0"

if not exist ".venv\Scripts\python.exe" (
    py -m venv .venv
    if errorlevel 1 goto :error
    ".venv\Scripts\python.exe" -m pip install -r requirements.txt
    if errorlevel 1 goto :error
)

start "ESP32 Screen Streamer" ".venv\Scripts\pythonw.exe" gui.py --config config.yaml
exit /b 0

:error
echo Failed to prepare the Python environment.
pause
exit /b 1
