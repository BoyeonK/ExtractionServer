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
int DedicateMain(int argc, char* argv[]) {
    std::cout << "D1 - OK : 인게임 프로세스 부팅 완료" << std::endl;

    try {
        IORing = new IoUringWrapper();
    } catch (const std::exception& e) {
        std::cerr << "D2 - X : IoUring객체 생성 실패" << std::endl;
        return 1;
    }

    std::cout << "D2 - OK : 인게임 프로세스에서 IoUring객체 생성" << std::endl;
    
    PacketHandler::Init();

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