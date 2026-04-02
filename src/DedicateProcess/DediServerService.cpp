#include "DediServerService.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <random>
#include "../ObjectPool.h"
#include "../PacketHandler.h"
#include "DediSessions.h"
#include "GameRoom.h"
#include "PlayerSession.h"


DediServerService::DediServerService() {
    _players.resize(51);
    for (int i=0; i<50; i++) {
        _freePlayerIds.push(i);
    }
}

DediServerService::~DediServerService() {
    if (_dediFd != -1) ::close(_dediFd);
}

bool DediServerService::InitMainIPC() {
    _dediFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (_dediFd == -1) {
        std::cerr << "D3-1 - X : 소켓 생성 실패" << std::endl;
        return false;
    }

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, "/tmp/dedicate.sock", sizeof(addr.sun_path) - 1);

    if (connect(_dediFd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        std::cerr << "D3-1 - X : 로비 서버 Connect 실패" << std::endl;
        close(_dediFd);
        return false;
    }

    _pD2MSession = new D2MSession(_dediFd, IORing);
    std::cout << "D3-1 - OK : DediServerService 객체 초기화 완" << std::endl;

    SendIdentityPacket();
    _pD2MSession->Recv();

    return true;
}

void DediServerService::SendIdentityPacket() {
    IPC_Protocol::D2MInitComplete pkt;
    pkt.set_pid(getpid());
    std::cout << "D3-2 : IPC_Protocol::D2MInitComplete으로 직렬화한 pid 전송 시도 : " << getpid() << std::endl;
    SendBuffer* pSendBuffer = PacketHandler::MakeSendBuffer(pkt);
    _pD2MSession->Send(pSendBuffer);
}

bool DediServerService::InitUDP() {
    _udpFd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (_udpFd == -1) {
        std::cerr << "D3-3 - X : UDP 소켓 생성 실패" << std::endl;
        return false;
    }

    bool bindSuccess = false;

    // TODO : 테스트용 임시 포트 범위, 나중에는 범위를 바꿀것이며 환경변수로 지정.
    constexpr int MIN_PORT = 7000;
    constexpr int MAX_PORT = 7100;

    for (int port = MIN_PORT; port <= MAX_PORT; ++port) {
        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(_udpFd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            _udpPort = port;
            bindSuccess = true;
            break;
        }
    }

    // 범위 내의 모든 포트가 꽉 찼을 경우
    if (!bindSuccess) {
        std::cerr << "D3-3 - X : 가용 UDP 포트가 없습니다! (최대 방 생성 개수 도달)" << std::endl;
        close(_udpFd);
        _udpFd = -1;
        return false;
    }

    std::cout << "D3-3 - OK : UDP 서버 준비 완료. EC2 통신용 포트: " << _udpPort << std::endl;

    _pClientSession = new D2CSession(_udpFd, IORing);
    _pClientSession->PumpRecvTasks();
    
    return true;
}

bool DediServerService::MakeRoomForThisGroup(int mapId, const std::vector<std::string>& ticketIds) {
    static int32_t roomId = 0;
    roomId++;

    GameRoom* newRoom = ObjectPool<GameRoom>::Acquire(mapId);
    std::vector<int32_t> sessionIds;
    std::vector<std::string> tokens;
    std::vector<int32_t> securityKeys;

    for (const auto& ticket : ticketIds) {
        std::string token = GetUniqueToken();
        
        int32_t sessionId = GetFreeSessionId();

        PlayerSession* newSession = ObjectPool<PlayerSession>::Acquire(ticket, token, sessionId, newRoom);
        newRoom->RegisterPlayerSession(newSession);
        
        _players[sessionId] = newSession;
        _tokenToPlayerSession[newSession->GetEntryToken()] = newSession;
        sessionIds.push_back(sessionId);
        tokens.push_back(newSession->GetEntryToken());
        securityKeys.push_back(newSession->GetSecurityKey());
    }

    _gameRooms.insert({roomId, newRoom});
    std::cout << "매치 테스트 7 - O : Room 할당 및 라우팅 세팅 완료 (RoomID: " << roomId << ")" << std::endl;

    IPC_Protocol::D2MUpdateEntryToken pkt = MakeD2MUpdateEntryTokenPkt(static_cast<int32_t>(_udpPort), ticketIds, sessionIds, tokens, securityKeys);
    SendBuffer* pSendBuffer = PacketHandler::MakeSendBuffer(pkt);
    _pD2MSession->Send(pSendBuffer);
    std::cout << "매치 테스트 8 : IPC를 통해 만들어진 Room의 인원(ticketId)에 대응하는 Token전송" << std::endl;

    return true;
}

PlayerSession* DediServerService::GetPlayerSession(int16_t sessionId) {
    if (sessionId >= _players.size())
        return nullptr;
    return _players[sessionId];
}

bool DediServerService::Handle_H2M2D_BCITSpkt(IPC_Protocol::H2M2DBindClientIpToSession& pkt) {
    const std::string& token = pkt.token();
    const std::string& ip = pkt.ip();
    
    auto it = _tokenToPlayerSession.find(token);
    if (it != _tokenToPlayerSession.end()) {
        it->second->SetIp(ip);
        _tokenToPlayerSession.erase(it);
        std::cout << "매치 테스트 11 - O : [인게임 프로세스] DediServer에서 token과 ip를 전달받아 인게임의 Session에 바인딩 완료. 파기한 token에 해당하는 메모리값 제거" << std::endl;
        return true;
    }
    return false;
}

int DediServerService::GetFreeSessionId() {
    int idx;
    if (_freePlayerIds.empty() == false) {
        idx = _freePlayerIds.front();
        _freePlayerIds.pop();
        return idx;
    }
    idx = static_cast<int32_t>(_players.size());
    _players.push_back(nullptr);
    return idx;
}

std::string DediServerService::GetUniqueToken() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    // 뽑아낼 문자열 풀 (숫자 + 대소문자 = 62개)
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::uniform_int_distribution<int> dis(0, sizeof(alphanum) - 2);

    std::string token;

    token.reserve(22); 

    do {
        token = "token_"; 

        for (int i = 0; i < 16; i++) {
            token += alphanum[dis(gen)];
        }

    } while (_tokenToPlayerSession.find(token) != _tokenToPlayerSession.end());

    return token;
}