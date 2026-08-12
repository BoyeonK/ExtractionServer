cmake_minimum_required(VERSION 3.16)
project(LinuxServerTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(URING_LIB uring REQUIRED)
find_library(REDIS_LIB redis++ REQUIRED)
find_library(HIREDIS_LIB hiredis REQUIRED)
find_library(MYSQL_LIB mysqlcppconn REQUIRED)

set(Protobuf_USE_STATIC_LIBS ON)
find_package(Protobuf REQUIRED)

# FetchContent 모듈 불러오기
include(FetchContent)

# 내 라이브러리 선언
FetchContent_Declare(
    MyUtils
    GIT_REPOSITORY https://github.com/BoyeonK/myUtils.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(MyUtils)

FetchContent_Declare(
    absl
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG        20240116.1  # 최신 안정화 버전 태그 (날짜 기반)
)
set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_BUILD_TESTING OFF)
FetchContent_MakeAvailable(absl)


set(PROJECT_SOURCES
    src/main.cpp
    src/NetAddress.cpp
    src/SocketWrapper.cpp
    src/IOTask.cpp
    src/HTTPserver.cpp
    src/IoUringWrapper.cpp
    src/ObjectPool.cpp
    src/SendBuffer.cpp
    src/PacketHandler.cpp
    src/RecvBuffer.cpp
    src/GlobalVariable.cpp
    src/EnvSetter.cpp
    src/RedisHandler.cpp
    src/DBProxyRequest.cpp
    src/MysqlHandle.cpp
    src/DediManager.cpp
    src/M2DSessions.cpp
    src/Matchmaker.cpp

    src/DedicateProcess/DediServerService.cpp
    src/DedicateProcess/DedicateMain.cpp
    src/DedicateProcess/DedicateGlobalVariable.cpp
    src/DedicateProcess/DediSessions.cpp
    src/DedicateProcess/UDPTask.cpp
    src/DedicateProcess/GameRoom.cpp
    src/DedicateProcess/Player.cpp
    src/DedicateProcess/PlayerInventory.cpp
    src/DedicateProcess/PlayerSession.cpp
    src/DedicateProcess/Items.cpp
    src/DedicateProcess/ClientPacketHandler.cpp
    src/DedicateProcess/TimerExecuter.cpp

    src/DedicateProcess/UnityGameObjects/UnityGameObject.cpp
    src/DedicateProcess/UnityGameObjects/TestGameObjects.cpp
    src/DedicateProcess/UnityGameObjects/PlayerObject.cpp
    src/DedicateProcess/UnityGameObjects/Container.cpp

    src/IPCProtocol/IPC_HTTP.pb.cc
    src/IPCProtocol/IPC_Dedicate.pb.cc
    src/IPCProtocol/IPC_enum.pb.cc

    src/DedicateProcess/ExternalProtocol/External_Protocol.pb.cc
    src/DedicateProcess/ExternalProtocol/External_Unity_Object.pb.cc

    src/NetAddress.h
    src/SocketWrapper.h
    src/IOTask.h
    src/HTTPserver.h
    src/IoUringWrapper.h
    src/ObjectPool.h
    src/SendBuffer.h
    src/PacketHandler.h
    src/RecvBuffer.h
    src/GlobalVariable.h
    src/EnvSetter.h
    src/RedisHandler.h
    src/DBProxyRequest.h
    src/DediManager.h
    src/M2DSessions.h
    src/Matchmaker.h

    src/DedicateProcess/DediServerService.h
    src/DedicateProcess/DedicateMain.h
    src/DedicateProcess/DediSessions.h
    src/DedicateProcess/UDPTask.h
    src/DedicateProcess/GameRoom.h
    src/DedicateProcess/Player.h
    src/DedicateProcess/PlayerInventory.h
    src/DedicateProcess/PlayerSession.h
    src/DedicateProcess/Items.h
    src/DedicateProcess/ClientPacketHandler.h
    src/DedicateProcess/TimerExecuter.h
    src/DedicateProcess/ItemDataManager.h
    src/DedicateProcess/MapDataManager.h
    src/DedicateProcess/DedicateGlobalVariable.h

    src/DedicateProcess/UnityGameObjects/UnityGameObject.h
    src/DedicateProcess/UnityGameObjects/CombatObject.h
    src/DedicateProcess/UnityGameObjects/TestGameObjects.h
    src/DedicateProcess/UnityGameObjects/PlayerObject.h
    src/DedicateProcess/UnityGameObjects/Container.h

    src/IPCProtocol/IPC_HTTP.pb.h
    src/IPCProtocol/IPC_Dedicate.pb.h
    src/IPCProtocol/IPC_enum.pb.h

    src/DedicateProcess/ExternalProtocol/External_Protocol.pb.h
    src/DedicateProcess/ExternalProtocol/External_Unity_Object.pb.h
)

# Protocol/Compiled/IPC/ → src/IPCProtocol/ 복사 (git에서 제외된 .pb 파일)
file(GLOB IPC_PB_FILES "${CMAKE_SOURCE_DIR}/Protocol/Compiled/IPC/*.pb.cc"
                       "${CMAKE_SOURCE_DIR}/Protocol/Compiled/IPC/*.pb.h")
file(COPY ${IPC_PB_FILES} DESTINATION "${CMAKE_SOURCE_DIR}/src/IPCProtocol/")

# 내 서버 실행 파일 생성
add_executable(LinuxServerTest ${PROJECT_SOURCES})

target_include_directories(LinuxServerTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${Protobuf_INCLUDE_DIRS}
)

target_link_libraries(LinuxServerTest PRIVATE 
    MyUtils::MyUtils 
    ${URING_LIB}
    ${Protobuf_LIBRARIES}
    ${REDIS_LIB}
    ${HIREDIS_LIB}
    ${MYSQL_LIB}
    pthread
    absl::flat_hash_map

)

add_custom_command(TARGET LinuxServerTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/Protocol/IPCProtocol/IPC_HTTP.proto"
            "${CMAKE_SOURCE_DIR}/HTTPServer/IPC_HTTP.proto"
)

add_custom_command(TARGET LinuxServerTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/HTTPServer/.env"
            "${CMAKE_CURRENT_BINARY_DIR}/.env"
)
