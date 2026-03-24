echo "Generating IPC files..."
protoc -I=./IPCProtocol/ --cpp_out=./Compiled/IPC/ IPC_enum.proto IPC_HTTP.proto IPC_Dedicate.proto 


echo "Generating External assosiated files..."
protoc -I=./ExternalProtocol/ --cpp_out=../src/DedicateProcess/ExternalProtocol/ External_Protocol.proto External_Unity_Object.proto
protoc -I=./ExternalProtocol/ --csharp_out=./Compiled/External/ External_Protocol.proto External_Unity_Object.proto
