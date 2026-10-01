@echo off
rem Pi-Gaze: build and run the portable unit tests on Windows with MSVC.
rem Usage: build_tests_windows.cmd [test-name-filter]
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if "%VSDIR%"=="" (echo MSVC not found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || exit /b 1
cd /d "%~dp0"
if not exist build-win mkdir build-win
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /W4 /permissive- /Iinclude src\core\*.cpp tests\*.cpp /Fobuild-win\ /Febuild-win\pigaze_tests.exe || exit /b 1
build-win\pigaze_tests.exe %*
