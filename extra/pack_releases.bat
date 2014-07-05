@ECHO OFF

IF "%1" == "" GOTO :EMPTY_VERSION

SET PACKER_CMD=@rar.exe a -y -r -ep1 -apIntChecker2

:Far2
IF NOT EXIST ..\bin\Release-Far2\ GOTO Far2x64
ECHO Packing Far2 x86
%PACKER_CMD% -- ..\bin\IntChecker2_Far2_x86_%1.rar "..\bin\Release-Far2\*" > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Far2x64
IF NOT EXIST ..\bin\Release-Far2-x64\ GOTO Far3
ECHO Packing Far2 x64
%PACKER_CMD% -- ..\bin\IntChecker2_Far2_x64_%1.rar "..\bin\Release-Far2-x64\*" > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Far3

IF NOT EXIST ..\bin\Release-Far3\ GOTO Far3x64
ECHO Packing Far3 x86
%PACKER_CMD% -- ..\bin\IntChecker2_Far3_x86_%1.rar "..\bin\Release-Far3\*" > nul
if NOT ERRORLEVEL == 0 GOTO PACK_ERROR

:Far3x64
IF NOT EXIST ..\bin\Release-Far3-x64\ GOTO Done
ECHO Packing Far3 x64
%PACKER_CMD% -- ..\bin\IntChecker2_Far3_x64_%1.rar "..\bin\Release-Far3-x64\*" > nul
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
