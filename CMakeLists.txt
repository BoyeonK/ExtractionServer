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
# file(COPY) 는 구성 시점에만 실행돼 .proto 재생성 후 make 만으로는 반영되지 않는다.
# 복사를 빌드 그래프에 걸어야 산출물이 바뀔 때 자동으로 따라온다.
set(IPC_PB_NAMES IPC_HTTP IPC_Dedicate IPC_enum)
set(IPC_PB_COPIED "")

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/src/IPCProtocol")

foreach(pbName ${IPC_PB_NAMES})
    foreach(pbExt cc h)
        set(pbSrc "${CMAKE_SOURCE_DIR}/Protocol/Compiled/IPC/${pbName}.pb.${pbExt}")
        set(pbDst "${CMAKE_SOURCE_DIR}/src/IPCProtocol/${pbName}.pb.${pbExt}")

        if(NOT EXISTS "${pbSrc}")
            message(FATAL_ERROR
                "protobuf 산출물이 없습니다: ${pbSrc}\n"
                "Protocol/ 디렉터리에서 compileProto.sh 를 먼저 실행하세요.")
        endif()

        add_custom_command(
            OUTPUT  "${pbDst}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${pbSrc}" "${pbDst}"
            DEPENDS "${pbSrc}"
            COMMENT "IPC pb 갱신: ${pbName}.pb.${pbExt}"
        )
        list(APPEND IPC_PB_COPIED "${pbDst}")
    endforeach()
endforeach()

add_custom_target(CopyIpcPb DEPENDS ${IPC_PB_COPIED})

# 소스 트리·빌드 디렉터리로 나가는 사본들도 같은 이유로 빌드 그래프에 건다.
# POST_BUILD 는 타깃이 재링크될 때만 실행돼, 원본만 고치면 사본이 옛것으로 남는다.
set(SYNC_COPIED "")

set(nodeProtoSrc "${CMAKE_SOURCE_DIR}/Protocol/IPCProtocol/IPC_HTTP.proto")
set(nodeProtoDst "${CMAKE_SOURCE_DIR}/HTTPServer/IPC_HTTP.proto")
add_custom_command(
    OUTPUT  "${nodeProtoDst}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${nodeProtoSrc}" "${nodeProtoDst}"
    DEPENDS "${nodeProtoSrc}"
    COMMENT "Node 측 IPC_HTTP.proto 사본 갱신"
)
list(APPEND SYNC_COPIED "${nodeProtoDst}")

# 아이템 수치의 출처는 database/ 이고 서버 헤더는 그 사본이다.
# 없을 때 건너뛰면 낡은 사본으로 조용히 빌드되므로 여기서 끊는다.
set(itemDataSrc "${CMAKE_SOURCE_DIR}/database/ItemDataManager.h")
set(itemDataDst "${CMAKE_SOURCE_DIR}/src/DedicateProcess/ItemDataManager.h")

if(NOT EXISTS "${itemDataSrc}")
    message(FATAL_ERROR "아이템 데이터 헤더가 없습니다: ${itemDataSrc}")
endif()

add_custom_command(
    OUTPUT  "${itemDataDst}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${itemDataSrc}" "${itemDataDst}"
    DEPENDS "${itemDataSrc}"
    COMMENT "아이템 데이터 헤더 갱신"
)
list(APPEND SYNC_COPIED "${itemDataDst}")

set(envSrc "${CMAKE_SOURCE_DIR}/HTTPServer/.env")
set(envDst "${CMAKE_CURRENT_BINARY_DIR}/.env")
if(EXISTS "${envSrc}")
    add_custom_command(
        OUTPUT  "${envDst}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${envSrc}" "${envDst}"
        DEPENDS "${envSrc}"
        COMMENT ".env 갱신"
    )
    list(APPEND SYNC_COPIED "${envDst}")
else()
    message(WARNING "HTTPServer/.env 가 없어 빌드 디렉터리로 복사하지 않습니다")
endif()

add_custom_target(SyncRuntimeFiles DEPENDS ${SYNC_COPIED})

# 내 서버 실행 파일 생성
add_executable(LinuxServerTest ${PROJECT_SOURCES})

# 어떤 소스보다 pb 복사가 먼저 끝나야 한다 (헤더를 include 하는 TU 가 많다)
add_dependencies(LinuxServerTest CopyIpcPb SyncRuntimeFiles)

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

