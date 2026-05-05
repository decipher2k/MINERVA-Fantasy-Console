@echo off
chcp 65001 >nul
echo ==========================================
echo  Fantasy Pi - Portable Kernel Build
echo ==========================================
echo.

set MAKE=%~dp0tools\make\make.exe
set MAKEFILE=%~dp0build\make\Makefile

if not exist "%MAKE%" (
    echo ERROR: Portable make not found at tools\make\make.exe
    exit /b 1
)

echo Cleaning build

del build\make\*.bin
del build\make\*.elf
del build\make\*.lst
del build\make\*.img
del build\make\*.fpak
del build\make\*.json
del output_kernel\kernel8.img



echo Building kernel + ROM for Raspberry Pi...
echo.

"%MAKE%" -f "%MAKEFILE%" -C "%~dp0build\make" %*

if %errorlevel% neq 0 (
    echo.
    echo BUILD FAILED
    exit /b %errorlevel%
)

copy build\make\kernel8.img output_kernel\

echo.
echo ==========================================
echo  BUILD SUCCESSFUL
echo ==========================================
echo Output: build\make\kernel8.img
echo.

pause
