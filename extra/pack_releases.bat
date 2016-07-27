@ECHO OFF

IF "%1" == "" GOTO :EMPTY_VERSION

ECHO Verifying prerequisites

IF DEFINED ProgramFiles(x86) (
  SET DEVENV_EXE_PATH="%ProgramFiles(x86)%\Microsoft Visual Studio 10.0\Common7\IDE\devenv.exe"
) ELSE (
  SET DEVENV_EXE_PATH="%ProgramFiles%\Microsoft Visual Studio 10.0\Common7\IDE\devenv.exe"
)

IF EXIST %DEVENV_EXE_PATH% GOTO VS_FOUND
ECHO [ERROR] MS Visual Studio 2010 is not found. Exiting.
PAUSE
EXIT 1

:VS_FOUND
ECHO Using Visual Studio 2010 from
ECHO %DEVENV_EXE_PATH%
ECHO.

SET PACKER_CMD=@rar.exe a -y -r -ep1 -apIntChecker2

:Far2
ECHO Building version for Far 2 x86
%DEVENV_EXE_PATH% /Rebuild "Release-Far2|Win32" "..\IntChecker2.VS2010.sln"
IF NOT EXIST ..\bin\Release-Far2\ GOTO Far2x64
ECHO Packing
%PACKER_CMD% -- ..\bin\IntChecker2_Far2_x86_%1.rar "..\bin\Release-Far2\*" > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Far2x64
ECHO Building version for Far 2 x64
%DEVENV_EXE_PATH% /Rebuild "Release-Far2|x64" "..\IntChecker2.VS2010.sln"
IF NOT EXIST ..\bin\Release-Far2-x64\ GOTO Far3
ECHO Packing
%PACKER_CMD% -- ..\bin\IntChecker2_Far2_x64_%1.rar "..\bin\Release-Far2-x64\*" > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Far3
ECHO Building version for Far 3 x86
%DEVENV_EXE_PATH% /Rebuild "Release-Far3|Win32" "..\IntChecker2.VS2010.sln"
IF NOT EXIST ..\bin\Release-Far3\ GOTO Far3x64
ECHO Packing
%PACKER_CMD% -- ..\bin\IntChecker2_Far3_x86_%1.rar "..\bin\Release-Far3\*" .\*.lua > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Far3x64
ECHO Building version for Far 3 x64
%DEVENV_EXE_PATH% /Rebuild "Release-Far3|x64" "..\IntChecker2.VS2010.sln"
IF NOT EXIST ..\bin\Release-Far3-x64\ GOTO Src
ECHO Packing
%PACKER_CMD% -- ..\bin\IntChecker2_Far3_x64_%1.rar "..\bin\Release-Far3-x64\*" .\*.lua > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Src
ECHO Packing source code
%PACKER_CMD% -xipch "-x*\ipch" "-x*\ipch\*" -x*.suo -x*.sdf -x*.opensdf -- ..\bin\IntChecker2_%1_src.rar "..\source" "..\depends" "..\extra" "..\*.sln" > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Done

ECHO [SUCCESS]
EXIT 0

:PACK_ERROR
ECHO Unable to pack release into archive
EXIT 1

:EMPTY_VERSION
ECHO Please set release version
EXIT 2
