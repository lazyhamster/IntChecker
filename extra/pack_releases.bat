@ECHO OFF

IF "%1" == "" GOTO :EMPTY_VERSION

SET PACKER_CMD=@rar.exe a -y -r -ep1 -apIntChecker2

ECHO Packing Far2 x86
%PACKER_CMD% -- ..\bin\IntChecker2_Far2_x86_%1.rar "..\bin\Release-Far2\*" > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

ECHO Packing Far2 x64
%PACKER_CMD% -- ..\bin\IntChecker2_Far2_x64_%1.rar "..\bin\Release-Far2-x64\*" > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

ECHO [SUCCESS]
EXIT 0

:PACK_ERROR
ECHO Unable to pack release into archive
EXIT 1

:EMPTY_VERSION
ECHO Please set release version
EXIT 2
