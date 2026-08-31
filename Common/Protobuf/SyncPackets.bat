@ECHO OFF
pushd "%~dp0"

SET "INCLUDE_DIR=..\..\..\Client\Common\Protobuf"

IF NOT EXIST "%INCLUDE_DIR%" (
    ECHO ERROR: %INCLUDE_DIR% does not exist!
    EXIT /B 1
)

XCOPY Enum.proto "%INCLUDE_DIR%" /E /Y /I /Q
XCOPY Protocol.proto "%INCLUDE_DIR%" /E /Y /I /Q
XCOPY Struct.proto "%INCLUDE_DIR%" /E /Y /I /Q

popd
