@echo off
setlocal
set "ATLAS_ENABLE_ASSISTANT=1"
pushd "%~dp0" || exit /b 1
start "" "%~dp0atlas-studio.exe"
popd
