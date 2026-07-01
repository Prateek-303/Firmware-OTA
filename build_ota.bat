@echo off
echo ==========================================
echo   Building OTA Firmware...
echo ==========================================

west build -b nrf7002dk/nrf5340/cpuapp --sysbuild 

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed!
    exit /b %errorlevel%
)

echo.
echo ==========================================
echo   Extracting Version and Renaming Binary...
echo ==========================================

for /f "tokens=3" %%a in ('findstr /C:"VERSION_MAJOR" VERSION') do set MAJOR=%%a
for /f "tokens=3" %%a in ('findstr /C:"VERSION_MINOR" VERSION') do set MINOR=%%a
for /f "tokens=3" %%a in ('findstr /C:"PATCHLEVEL" VERSION') do set PATCH=%%a

set VERSION_STR=%MAJOR%.%MINOR%.%PATCH%
set SRC_BIN=build\OTA_FIRM\zephyr\zephyr.signed.bin
set DEST_BIN=app_update_%VERSION_STR%.bin

echo.
echo ==========================================
echo   Preparing Deployment Repository...
echo ==========================================
if not exist "deploy_repo" (
    echo Cloning your GitHub repository for deployment...
    git clone https://github.com/Prateek-303/nrf54-OTA.git deploy_repo
)
cd deploy_repo
git pull origin main
cd ..

copy /Y "%SRC_BIN%" "deploy_repo\%DEST_BIN%" > nul

echo.
echo ==========================================
echo   Updating Manifest & Pushing to GitHub...
echo ==========================================
python update_manifest.py %VERSION_STR% %DEST_BIN%

cd deploy_repo
git add %DEST_BIN% manifest.json
git commit -m "Auto OTA Update to v%VERSION_STR%"
git push origin main
cd ..

echo.
echo ==========================================
echo [SUCCESS] Update v%VERSION_STR% has been automatically pushed to GitHub!
echo Wait 5 minutes for GitHub cache to update, then hit RESET on your board!
echo ==========================================
