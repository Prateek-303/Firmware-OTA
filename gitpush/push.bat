@echo off
setlocal enabledelayedexpansion

echo.
echo  ╔══════════════════════════════════════════════════════════╗
echo  ║       NEXUS OTA — SERVER README Push Script              ║
echo  ║    Pushes README only to nRF53-OTA-server repo          ║
echo  ║    (manifest.json + .bin are managed by build_ota.bat)  ║
echo  ╚══════════════════════════════════════════════════════════╝
echo.

:: ---------------------------------------------------------------
:: Step 0 — Set paths
:: ---------------------------------------------------------------
set REPO_DIR=D:\Nordic\OTA_FIRM\deploy_repo
set README_SRC=D:\Nordic\OTA_FIRM\gitpush\README.md

:: ---------------------------------------------------------------
:: Step 1 — Ensure deploy_repo exists
:: ---------------------------------------------------------------
if not exist "%REPO_DIR%" (
    echo [1/5] deploy_repo not found. Cloning from GitHub...
    git clone https://github.com/Prateek-303/nrf54-OTA.git "%REPO_DIR%"
    if !errorlevel! neq 0 (
        echo.
        echo [ERROR] Git clone failed!
        echo         Check your internet connection and GitHub credentials.
        echo         Run: git config --global credential.helper manager
        pause
        exit /b 1
    )
    echo       Clone successful.
) else (
    echo [1/5] deploy_repo found. Pulling latest from GitHub...
    cd /D "%REPO_DIR%"
    git pull origin main
    if !errorlevel! neq 0 (
        echo [WARNING] Git pull failed. Continuing with local state...
    )
    cd /D D:\Nordic\OTA_FIRM
)

:: ---------------------------------------------------------------
:: Step 2 — Copy updated README into the repo
:: ---------------------------------------------------------------
echo.
echo [2/5] Copying README.md into the cloud server repository...
if exist "%README_SRC%" (
    copy /Y "%README_SRC%" "%REPO_DIR%\README.md" > nul
    echo       README.md updated successfully.
) else (
    echo [WARNING] README.md not found at: %README_SRC%
    echo           Skipping README copy — other files will still be pushed.
)

:: ---------------------------------------------------------------
:: Step 3 — Stage all files
:: ---------------------------------------------------------------
echo.
echo [3/5] Staging README.md only...
echo       (manifest.json + .bin files are NOT touched here)
cd /D "%REPO_DIR%"
git add README.md
if !errorlevel! neq 0 (
    echo [ERROR] git add README.md failed!
    cd /D D:\Nordic\OTA_FIRM
    pause
    exit /b 1
)
echo       README.md staged.

:: ---------------------------------------------------------------
:: Step 4 — Commit with auto timestamp
:: ---------------------------------------------------------------
echo.
echo [4/5] Committing changes...
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value 2^>nul') do (
    if not "%%I"=="" set dt=%%I
)
set YYYY=%dt:~0,4%
set MM=%dt:~4,2%
set DD=%dt:~6,2%
set HH=%dt:~8,2%
set MIN=%dt:~10,2%
set SS=%dt:~12,2%
set TIMESTAMP=%YYYY%-%MM%-%DD% %HH%:%MIN%:%SS%

git commit -m "OTA Server Update: %TIMESTAMP%"
if !errorlevel! neq 0 (
    echo       No new changes to commit. Repository is already up to date.
    goto :push
)
echo       Committed: "OTA Server Update: %TIMESTAMP%"

:: ---------------------------------------------------------------
:: Step 5 — Push to GitHub
:: ---------------------------------------------------------------
:push
echo.
echo [5/5] Pushing to GitHub (github.com/Prateek-303/nrf54-OTA)...
git push origin main
if !errorlevel! neq 0 (
    echo.
    echo [ERROR] Push failed!
    echo.
    echo Possible reasons:
    echo   1. No internet connection
    echo   2. GitHub credentials not cached
    echo      Fix: git config --global credential.helper manager
    echo           Then run any git push manually to log in once.
    echo   3. Remote has changes your local doesn't have
    echo      Fix: cd deploy_repo then run: git pull origin main
    cd /D D:\Nordic\OTA_FIRM
    pause
    exit /b 1
)

cd /D D:\Nordic\OTA_FIRM

:: ---------------------------------------------------------------
:: Done
:: ---------------------------------------------------------------
echo.
echo  ╔══════════════════════════════════════════════════════════╗
echo  ║                     ✅  SUCCESS!                        ║
echo  ║                                                          ║
echo  ║  README.md pushed to nRF53-OTA-server.                  ║
echo  ║  manifest.json and .bin files were NOT modified.        ║
echo  ╚══════════════════════════════════════════════════════════╝
echo.
pause
