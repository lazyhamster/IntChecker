@ECHO OFF

ECHO Fetching version

m4 -P version.txt.m4 > version.txt
SET /p PVER=<version.txt
DEL version.txt

IF "%PVER%" == "" GOTO :EMPTY_VERSION

ECHO Verifying prerequisites

IF DEFINED ProgramFiles(x86) (
  SET DEVENV_EXE_PATH="%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\Common7\IDE\devenv.exe"
) ELSE (
  SET DEVENV_EXE_PATH="%ProgramFiles%\Microsoft Visual Studio 12.0\Common7\IDE\devenv.exe"
)

IF EXIST %DEVENV_EXE_PATH% GOTO VS_FOUND
ECHO [ERROR] MS Visual Studio 2013 is not found. Exiting.
PAUSE
EXIT 1

:VS_FOUND
ECHO Using Visual Studio 2013 from
ECHO %DEVENV_EXE_PATH%
ECHO.

SET PACKER_CMD=@rar.exe a -y -r -ep1 -apIntChecker2

:Far2
REM ECHO Building version for Far 2 x86
REM %DEVENV_EXE_PATH% /Rebuild "Release-Far2|Win32" "..\IntChecker2.VS2010.sln"
REM IF NOT EXIST ..\bin\Release-Far2\ GOTO Far2x64
REM ECHO Packing
REM %PACKER_CMD% -- ..\bin\IntChecker2_Far2_x86_%1.rar "..\bin\Release-Far2\*" > nul
REM if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Far2x64
REM ECHO Building version for Far 2 x64
REM %DEVENV_EXE_PATH% /Rebuild "Release-Far2|x64" "..\IntChecker2.VS2010.sln"
REM IF NOT EXIST ..\bin\Release-Far2-x64\ GOTO Far3
REM ECHO Packing
REM %PACKER_CMD% -- ..\bin\IntChecker2_Far2_x64_%1.rar "..\bin\Release-Far2-x64\*" > nul
REM if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Far3
ECHO Building version for Far 3 x86
%DEVENV_EXE_PATH% /Rebuild "Release-Far3|Win32" "..\IntChecker2.sln"
IF NOT EXIST ..\bin\Release-Far3\ GOTO BUILD_ERROR
ECHO Packing
%PACKER_CMD% -- ..\bin\IntChecker2_Far3_x86_%PVER%.rar "..\bin\Release-Far3\*" .\*.lua > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Far3x64
ECHO Building version for Far 3 x64
%DEVENV_EXE_PATH% /Rebuild "Release-Far3|x64" "..\IntChecker2.sln"
IF NOT EXIST ..\bin\Release-Far3-x64\ GOTO BUILD_ERROR
ECHO Packing
%PACKER_CMD% -- ..\bin\IntChecker2_Far3_x64_%PVER%.rar "..\bin\Release-Far3-x64\*" .\*.lua > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Src
REM ECHO Packing source code
REM %PACKER_CMD% -xipch "-x*\ipch" "-x*\ipch\*" -x*.suo -x*.sdf -x*.opensdf -- ..\bin\IntChecker2_%PVER%_src.rar "..\source" "..\depends" "..\extra" "..\*.sln" > nul
REM if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Done

ECHO [SUCCESS]
EXIT 0

:PACK_ERROR
ECHO Unable to pack release into archive
EXIT 1

:EMPTY_VERSION
ECHO Unable to find version information
EXIT 2

:BUILD_ERROR
ECHO Build failed
EXIT 3
