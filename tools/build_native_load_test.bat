@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if defined L4N_VS_INSTALL_PATH set "VSINSTALL=%L4N_VS_INSTALL_PATH%"
if not defined VSINSTALL if exist "%VSWHERE%" for /f "tokens=*" %%I in ('"%VSWHERE%" -latest -products * -property installationPath') do if not defined VSINSTALL set "VSINSTALL=%%I"
if not defined VSINSTALL (
    echo Could not locate Visual Studio. Set L4N_VS_INSTALL_PATH and try again.
    exit /b 1
)
if not exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" (
    echo VsDevCmd.bat not found below "%VSINSTALL%".
    exit /b 1
)
call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl.exe /nologo /W4 /EHsc /MT /Fo:"%~dp0native_load_test.obj" /Fe:"%~dp0native_load_test.exe" "%~dp0native_load_test.cpp"
