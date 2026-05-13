#include "DedicateMain.h"
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include "../IoUringWrapper.h"
#include "../GlobalVariable.h"
#include "DedicateGlobalVariable.h"
#include "DediServerService.h"
#include "../PacketHandler.h"
#include "ClientPacketHandler.h"
#include "TimerExecuter.h"
#include "ItemDataManager.h"

int DedicateMain(int argc, char* argv[]) {
    std::cout << "D1 - OK : 인게임 프로세스 부팅 완료" << std::endl;

    const char* env_redis_host  = std::getenv("REDIS_HOST");
    const char* env_redis_port  = std::getenv("REDIS_PORT");
    std::string redis_url = "tcp://" + std::string(env_redis_host) + ":" + std::string(env_redis_port);

    try {
        IORing = new IoUringWrapper();
        pRedis = new sw::redis::Redis(redis_url);
    } catch (const std::exception& e) {
        std::cerr << "D2 - X : 환경변수, IoUring객체, Redis핸들 중에서 최소 하나 실패" << std::endl;
        return 1;
    }

    std::cout << "D2 - OK : 인게임 프로세스에서 환경변수 로드, IoUring객체 및 Redis핸들 생성" << std::endl;
    
    PacketHandler::Init();

    pItemDataManager = new ItemDataManager();
    pItemDataManager->Init();
    std::cout << "D3 - OK : ItemDataManager 초기화 완료" << std::endl;

    pDediServer = new DediServerService();
    pTimerExecuter = new TimerExecuter();
    if (pDediServer->Init() == false) {
        return 1;
    }

    std::cout << "D4 - OK : 인게임 프로세스에서 IoUring객체의 동작 시작" << std::endl;

    ClientPacketHandler::Init();
    std::cout << "D5 - OK : 인게임 프로세스에서 UDP PacketHandler 초기화 완료." << std::endl;

    while (true) {
        // 세 작업 중 하나라도 실제 작업을 수행했으면 sleep을 생략한다.
        bool hasWork = false;

        if (IORing->ExecuteCQTask())
            hasWork = true;

        if (pDediServer->CheckRetransmits(ClientPacketHandler::NowMs()))
            hasWork = true;

        if (pDediServer->UpdateGameRooms())
            hasWork = true;

        if (pTimerExecuter->Tick())
            hasWork = true;

        if (!hasWork)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}