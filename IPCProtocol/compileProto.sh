echo "Generating C++ files..."

protoc -I=./ --cpp_out=./Compiled ./IPC_enum.proto ./IPC_HTTP.proto ./IPC_Dedicate.proto