@echo off
echo Halo MCC VR mod: exporting Halo 4 grenade models for the VR mod...
echo This only reads the mod tools' files and writes to the vrmod_export folder.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0vrmod_export_grenades.ps1"
echo.
pause
