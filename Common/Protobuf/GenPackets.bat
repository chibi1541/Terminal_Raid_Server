pushd %~dp0

protoc.exe -I=./ --cpp_out=./ ./Protocol.proto
protoc.exe -I=./ --cpp_out=./ ./Enum.proto
protoc.exe -I=./ --cpp_out=./ ./Struct.proto

GenPackets.exe --path=./Protocol.proto --output=ClientPacketHandler --recv=C_ --send=S_

IF ERRORLEVEL 1 PAUSE

XCOPY Enum.pb.h "../../ServerProj/source/Protocol" /E /Y /I
XCOPY Enum.pb.cc "../../ServerProj/source/Protocol" /E /Y /I
XCOPY Struct.pb.h "../../ServerProj/source/Protocol" /E /Y /I
XCOPY Struct.pb.cc "../../ServerProj/source/Protocol" /E /Y /I
XCOPY Protocol.pb.h "../../ServerProj/source/Protocol" /E /Y /I
XCOPY Protocol.pb.cc "../../ServerProj/source/Protocol" /E /Y /I
XCOPY ClientPacketHandler.h "../../ServerProj/source/Protocol" /E /Y /I


DEL /Q /F *.pb.h
DEL /Q /F *.pb.cc
DEL /Q /F *.h

PAUSE