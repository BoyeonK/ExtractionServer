#include "DedicateMain.h"
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include "../IoUringWrapper.h"
#include "../GlobalVariable.h"
#include "DediServerService.h"
#include "../PacketHandler.h"
#include "ClientPacketHandler.h"

DediServerService* pDediServer = nullptr;

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

    pDediServer = new DediServerService();
    if (pDediServer->Init() == false) {
        return 1;
    }

    std::cout << "D4 - OK : 인게임 프로세스에서 IoUring객체의 동작 시작" << std::endl;

    ClientPacketHandler::Init();
    std::cout << "D5 - OK : 인게임 프로세스에서 UDP PacketHandler 초기화 완료." << std::endl;

    auto lastRetransmit = std::chrono::steady_clock::now();

    while (true) {
        if (IORing->ExecuteCQTask() == false)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // TODO : sleep로직이랑 매치되는지 검증 (할 일이 없는경우 sleep인데, 재전송할 패킷이 있는 경우에도 sleep함.)
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRetransmit).count() >= 50) {
            pDediServer->CheckRetransmits(ClientPacketHandler::NowMs());
            lastRetransmit = now;
        }
    }

    return 0;
}