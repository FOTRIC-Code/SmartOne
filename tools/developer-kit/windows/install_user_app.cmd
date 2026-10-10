@echo off
setlocal

where bash >nul 2>nul
if errorlevel 1 (
  echo error: bash not found. Install Git Bash first.
  exit /b 1
)

set SCRIPT_DIR=%~dp0
bash "%SCRIPT_DIR%..\install_user_app.sh" %*
