@echo off
setlocal enabledelayedexpansion

echo.
echo  ╔══════════════════════════════════════════════════════════╗
echo  ║      NEXUS OTA — Firmware Project Push Script           ║
echo  ║       github.com/Prateek-303/Firmware-OTA               ║
echo  ╚══════════════════════════════════════════════════════════╝
echo.

set PROJECT_DIR=D:\Nordic\OTA_FIRM

:: ---------------------------------------------------------------
:: Step 1 — Check Git is initialised
:: ---------------------------------------------------------------
cd /D "%PROJECT_DIR%"
git rev-parse --git-dir >nul 2>&1
if !errorlevel! neq 0 (
    echo [ERROR] This folder is not a Git repository!
    echo         Run: git init
    echo         Then set your remote: git remote add origin https://github.com/YOUR_USER/YOUR_REPO.git
    pause
    exit /b 1
)
echo [1/5] Git repository confirmed.

:: ---------------------------------------------------------------
:: Step 2 — Check remote is configured
:: ---------------------------------------------------------------
echo.
git remote get-url origin >nul 2>&1
if !errorlevel! neq 0 (
    echo [ERROR] No remote 'origin' configured for the project repo!
    echo.
    echo To fix this:
    echo   Run: git remote add origin https://github.com/Prateek-303/Firmware-OTA.git
    echo   Then run this script again.
    pause
    exit /b 1
)
for /f %%R in ('git remote get-url origin') do set REMOTE_URL=%%R
echo [2/5] Remote: %REMOTE_URL%

:: ---------------------------------------------------------------
:: Step 3 — Stage all project files
:: ---------------------------------------------------------------
echo.
echo [3/5] Staging project files...
echo       (build\, deploy_repo\ are excluded via .gitignore)
git add .
if !errorlevel! neq 0 (
    echo [ERROR] git add failed!
    pause
    exit /b 1
)
echo       Files staged.

:: ---------------------------------------------------------------
:: Step 4 — Commit with version + timestamp
:: ---------------------------------------------------------------
echo.
echo [4/5] Committing...
for /f "tokens=3" %%a in ('findstr /C:"VERSION_MAJOR" VERSION') do set MAJOR=%%a
for /f "tokens=3" %%a in ('findstr /C:"VERSION_MINOR" VERSION') do set MINOR=%%a
for /f "tokens=3" %%a in ('findstr /C:"PATCHLEVEL" VERSION') do set PATCH=%%a
set VER=%MAJOR%.%MINOR%.%PATCH%

for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value 2^>nul') do (
    if not "%%I"=="" set dt=%%I
)
set TIMESTAMP=%dt:~0,4%-%dt:~4,2%-%dt:~6,2% %dt:~8,2%:%dt:~10,2%:%dt:~12,2%

git commit -m "Firmware v%VER% — %TIMESTAMP%"
if !errorlevel! neq 0 (
    echo       No new changes to commit. Repo already up to date.
    goto :push
)
echo       Committed: "Firmware v%VER% — %TIMESTAMP%"

:: ---------------------------------------------------------------
:: Step 5 — Push
:: ---------------------------------------------------------------
:push
echo.
echo [5/5] Pushing firmware project to GitHub...
git push origin main
if !errorlevel! neq 0 (
    echo.
    echo [ERROR] Push failed!
    echo.
    echo Possible reasons:
    echo   1. No internet connection.
    echo   2. GitHub credentials not cached.
    echo      Fix: git config --global credential.helper manager
    echo   3. First push to a new repo — use:
    echo      git push -u origin main
    echo   4. Remote has diverged — run:
    echo      git pull origin main --rebase
    pause
    exit /b 1
)

:: ---------------------------------------------------------------
:: Done
:: ---------------------------------------------------------------
echo.
echo  ╔══════════════════════════════════════════════════════════╗
echo  ║                     ✅  SUCCESS!                        ║
echo  ║                                                          ║
echo  ║  Firmware project v%VER% pushed to:                     ║
echo  ║  github.com/Prateek-303/Firmware-OTA                    ║
echo  ╚══════════════════════════════════════════════════════════╝
echo.
pause
