#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <mysql_connection.h>
#include "GlobalVariable.h"
#include "PacketHandler.h"
#include "IoUringWrapper.h"
#include "IOTask.h"
#include "HTTPserver.h"
#include "SocketWrapper.h"
#include "ObjectPool.h"
#include "RedisHandler.h"
#include "DBProxyRequest.h"
#include "EnvSetter.h"
#include "DediManager.h"
#include "MysqlHandle.h"
#include "DedicateProcess/DedicateMain.h"

// 메인 함수에서 인자를 받을 수 있다. 내 세상이 무너졌다.
int main(int argc, char* argv[]) {
    SetEnv();

    if (argc >= 2 && std::string(argv[1]) == "--dedicated") {
        return DedicateMain(argc, argv);
    }

    const char* env_redis_host  = std::getenv("REDIS_HOST");
    const char* env_redis_port  = std::getenv("REDIS_PORT");
    const char* env_db_host     = std::getenv("MYSQL_HOST");
    const char* env_db_port     = std::getenv("MYSQL_PORT");
    const char* env_db_user     = std::getenv("MYSQL_USER");
    const char* env_db_pass     = std::getenv("MYSQL_PASSWORD");
    const char* env_db_name     = std::getenv("MYSQL_DATABASE");

    std::string redis_url = "tcp://" + std::string(env_redis_host) + ":" + std::string(env_redis_port);

    std::string mysql_url = "tcp://" + std::string(env_db_host) + ":" + std::string(env_db_port);                               

    try {
        IORing = new IoUringWrapper();
        pRedis = new sw::redis::Redis(redis_url);
        pDediManager = new DediManager();
        pDBProxyService = new DBProxyService();

        // 아이템 캐시 구축용 일회성 연결이 아니다 — 이탈 확정 시 인벤토리 반영이 이 핸들을 탄다.
        pMysql = new MysqlHandle();
        if (pMysql->Init(mysql_url, env_db_user, env_db_pass, env_db_name) == false)
            throw std::runtime_error("MySQL 연결 실패");

        std::cout << "C1 - OK : 환경변수 로드, IoUring객체 및 Redis·MySQL 핸들 생성" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "C1 - X : 글로벌 변수 초기화 실패: " << e.what() << std::endl;
        return 1;
    }

    try {
        RedisHandler::InitializeItemCache(pMysql->Get(), *pRedis);

        std::cout << "C2-3 - OK : MySQL에서 Redis에 items필드 가져오는 중" << std::endl;
    } catch (const sql::SQLException& e) {
        std::cerr << "C2-3 - X : MySQL에서 Redis에 items필드 가져오기 대실패: " << e.what() << std::endl;
        return 1;
    }

    PacketHandler::Init();

    std::unique_ptr<IPCListenSocketWrapper> httpsIpc;
    std::unique_ptr<IPCListenSocketWrapper> dedicateIpc;
    pid_t nodePid;
    try {
        httpsIpc = std::make_unique<IPCListenSocketWrapper>("/tmp/https.sock", 1);
        httpsIpc->Init();

        dedicateIpc = std::make_unique<IPCListenSocketWrapper>("/tmp/dedicate.sock", 128);
        dedicateIpc->Init();

        if (launchNode(nodePid))
            std::cout << "C3-2 - OK : 자식 프로세스로 HTTP서버 구동 (PID: " << nodePid << ")" << std::endl;

        H2SAcceptTask* httpAcceptTask = ObjectPool<H2SAcceptTask>::Acquire(
            httpsIpc->GetFd(), 
            IORing
        );
        IORing->RegisterAcceptTask(httpsIpc->GetFd(), httpAcceptTask);

        DediAcceptTask* dediAcceptTask = ObjectPool<DediAcceptTask>::Acquire(
            dedicateIpc->GetFd(),
            IORing
        );
        IORing->RegisterAcceptTask(dedicateIpc->GetFd(), dediAcceptTask);

    } catch (const std::exception& e) {
        std::cerr << "C3-2 - X : 소켓 생성 ~ 초기화 실패 : " << e.what() << std::endl;
        _exit(1);
    }

    std::cout << "C4-1 : 인게임 로직 전용 Dedicated Process 미리 1개 생성" << std::endl;
    pDediManager->SpawnSingleServer();

    while (true) {
        pDediManager->MatchMake();

        bool hasIOWork = IORing->ExecuteCQTask();
        bool hasDBWork = pDBProxyService->ExecuteAll();

        if (!hasIOWork && !hasDBWork) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }

    return 0;
}