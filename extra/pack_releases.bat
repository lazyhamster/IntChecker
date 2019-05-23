@ECHO OFF

ECHO Fetching version

for /f %%i in ('m4 -P version.txt.m4') do set PVER=%%i

IF "%PVER%" == "" GOTO :EMPTY_VERSION
ECHO Version found: %PVER%
ECHO.

ECHO Searching for Visual Studio

IF DEFINED ProgramFiles(x86) (
  SET VS_WHERE_PATH="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
) ELSE (
  SET VS_WHERE_PATH="%ProgramFiles%\Microsoft Visual Studio\Installer"
)

SET PATH=%PATH%;%VS_WHERE_PATH%
for /f "usebackq tokens=1* delims=: " %%i in (`vswhere.exe -latest -requires Microsoft.VisualStudio.Workload.NativeDesktop`) do (
	if /i "%%i"=="productPath" set DEVENV_EXE_PATH="%%j"
)

IF EXIST %DEVENV_EXE_PATH% GOTO VS_FOUND
ECHO [ERROR] MS Visual Studio 2017 is not found. Exiting.
PAUSE
EXIT 1

:VS_FOUND
ECHO Using Visual Studio from
ECHO %DEVENV_EXE_PATH%
ECHO.

SET SOLUTION_FILE=..\IntChecker2.sln

:Far2
ECHO Building version for Far 2 x86
call :Build_Version Far2 Win32 x86
if ERRORLEVEL == 1 GOTO BUILD_ERROR
if ERRORLEVEL == 2 GOTO PACK_ERROR

:Far2x64
ECHO Building version for Far 2 x64
call :Build_Version Far2 x64 x64
if ERRORLEVEL == 1 GOTO BUILD_ERROR
if ERRORLEVEL == 2 GOTO PACK_ERROR

:Far3
ECHO Building version for Far 3 x86
call :Build_Version Far3 Win32 x86
if ERRORLEVEL == 1 GOTO BUILD_ERROR
if ERRORLEVEL == 2 GOTO PACK_ERROR

:Far3x64
ECHO Building version for Far 3 x64 
call :Build_Version Far3 x64 x64
if ERRORLEVEL == 1 GOTO BUILD_ERROR
if ERRORLEVEL == 2 GOTO PACK_ERROR

:Done
ECHO [SUCCESS]
EXIT 0

:Build_Version
REM Arguments <Far Version> <Platform Name> <Archive Platform Name>

ECHO Executing build
%DEVENV_EXE_PATH% /Rebuild "Release-%~1|%~2" "%SOLUTION_FILE%"
IF NOT EXIST ..\bin\Release-%~1-%~2\ EXIT /B 1
ECHO Packing archive
SET SCRIPTS=
IF "%~1" == "Far3" SET SCRIPTS=.\*.lua
rar.exe a -y -r -ep1 -apIntChecker2 -x*.iobj -x*.ipdb -- ..\bin\IntChecker2_%~1_%~3_%PVER%.rar "..\bin\Release-%~1-%~2\*" "..\source\Text\*.txt" %SCRIPTS% > nul
if NOT ERRORLEVEL == 0 EXIT /B 2
ECHO Cleanup
rmdir /s /q "..\bin\Release-%~1-%~2\"
rmdir /s /q "..\obj\Release-%~1-%~2\"
ECHO.
EXIT /B 0

:PACK_ERROR
ECHO Unable to pack release into archive
EXIT 1

:EMPTY_VERSION
ECHO Unable to find version information
EXIT 2

:BUILD_ERROR
ECHO Build failed
EXIT 3
