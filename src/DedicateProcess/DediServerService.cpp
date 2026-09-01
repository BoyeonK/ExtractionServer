#include "DediServerService.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <random>
#include <cstring>
#include "../ObjectPool.h"
#include "../PacketHandler.h"
#include "../GlobalVariable.h"
#include "../IoUringWrapper.h"
#include "DedicateGlobalVariable.h"
#include "TimerExecuter.h"
#include "DediSessions.h"
#include "GameRoom.h"
#include "PlayerSession.h"
#include "Items.h"


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

    // TEMP : 테스트용 임시 포트 범위, 나중에는 범위를 바꿀것이며 환경변수로 지정.
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

    if (!bindSuccess) {
        std::cerr << "D3-3 - X : 가용 UDP 포트가 없습니다! (최대 방 생성 개수 도달)" << std::endl;
        close(_udpFd);
        _udpFd = -1;
        return false;
    }

    std::cout << "D3-3 - OK : UDP 서버 준비 완료. 서버 통신용 포트:" << _udpPort << std::endl;

    _pClientSession = new D2CSession(_udpFd, IORing);
    _pClientSession->PumpRecvTasks();
    
    return true;
}

bool DediServerService::MakeRoomForThisGroup(const IPC_Protocol::M2DMakeRoomForThisGroup& pkt) {
    static int32_t roomId = 0;
    roomId++;

    GameRoom* newRoom = nullptr;
    switch(pkt.map_id())   {
        case GameRoom::MAP_TUTORIAL:
            newRoom = ObjectPool<TestGameRoom>::Acquire(roomId);
            break;
        case GameRoom::MAP_TENERIFE:
            newRoom = ObjectPool<TenerifeGameRoom>::Acquire(roomId);
            break;
        default:
            std::cout << "매치 테스트 7 - X : 알 수 없는 MapID (" << pkt.map_id() << ")" << std::endl;
            break;
    }
    if (newRoom == nullptr) {
        std::cout << "매치 테스트 7 - X : Room 할당 실패 (RoomID: " << roomId << ")" << std::endl;
        return false;
    }
    std::vector<std::string> ticketIds;
    std::vector<int32_t> sessionIds;
    std::vector<std::string> tokens;
    std::vector<int32_t> securityKeys;

    for (const auto& info : pkt.player_infos()) {
        std::string token = GetUniqueToken();
        int32_t sessionId = GetFreeSessionId();

        std::vector<Slot> inventorySlots;
        for (const auto& s : info.inventory_items()) {
            Slot slot;
            slot.item.blueprintId = s.item_id();
            slot.slotIndex        = s.slot_index();
            slot.quantity         = s.quantity();
            inventorySlots.push_back(slot);
        }

        std::vector<Slot> equipmentSlots;
        for (const auto& s : info.equipment_items()) {
            Slot slot;
            slot.item.blueprintId = s.item_id();
            slot.slotIndex        = s.slot_index();
            slot.quantity         = s.quantity();
            equipmentSlots.push_back(slot);
        }

        PlayerSession* newSession = ObjectPool<PlayerSession>::Acquire(
            info.ticket_id(), token, sessionId, newRoom,
            info.uid(), info.user_id(), info.rating(),
            inventorySlots, equipmentSlots,
            info.character_type()
        );
        newRoom->RegisterPlayerSession(newSession);

        _players[sessionId] = newSession;
        _tokenToPlayerSession[newSession->GetEntryToken()] = newSession;
        ticketIds.push_back(info.ticket_id());
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

void DediServerService::NotifyPlayerLeftToMain(PlayerSession* pSession) {
    if (pSession == nullptr || _pD2MSession == nullptr) return;

    const PlayerSession::LeaveReason reason = pSession->GetLeaveReason();

    IPC_Protocol::D2MNotifyPlayerLeft pkt;
    pkt.set_uid(pSession->GetUid());
    pkt.set_leave_reason(static_cast<IPC_Protocol::LeaveReason>(reason));

    if (reason == PlayerSession::LeaveReason::RECALLED)
        pSession->SerializeInventoryForIPC(&pkt);

    SendBuffer* pSendBuffer = PacketHandler::MakeSendBuffer(pkt);
    if (pSendBuffer == nullptr) {
        std::cerr << "[NotifyPlayerLeftToMain] SendBuffer 확보 실패 (uid=" << pSession->GetUid()
                  << ")" << std::endl;
        return;
    }
    _pD2MSession->Send(pSendBuffer);
}

void DediServerService::NotifyRoomDestroyedToMain(int32_t roomId, int32_t playerCount) {
    if (_pD2MSession == nullptr) return;

    IPC_Protocol::D2MNotifyRoomDestroyed pkt;
    pkt.set_room_id(roomId);
    pkt.set_player_count(playerCount);

    SendBuffer* pSendBuffer = PacketHandler::MakeSendBuffer(pkt);
    if (pSendBuffer == nullptr) {
        std::cerr << "[NotifyRoomDestroyedToMain] SendBuffer 확보 실패 (roomId=" << roomId
                  << ", 인원=" << playerCount << ")" << std::endl;
        return;
    }
    _pD2MSession->Send(pSendBuffer);
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

void DediServerService::Send(SendBuffer* buffer, const sockaddr_in& destAddr) {
    _pClientSession->Send(buffer, destAddr);
}

bool DediServerService::CheckRetransmits(uint32_t nowMs) {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastRetransmitTime).count() < 50)
        return false;
    _lastRetransmitTime = now;

    bool didRetransmit = false;

    // 50ms 마다 홀·짝 인덱스를 번갈아 처리 → 세션당 실질 주기는 100ms
    const int phase = _retransmitPhase;
    _retransmitPhase ^= 1;

    for (size_t i = phase; i < _players.size(); i += 2) {
        PlayerSession* pSession = _players[i];
        if (pSession == nullptr) continue;

        if (pSession->GetLeaveState() == PlayerSession::LeaveState::FINALIZED) continue;

        for (PendingPacket* pending : pSession->GetRetransmitCandidates(nowMs)) {
            uint32_t size = pending->allocSize;
            SendBuffer* retransmitBuf = IORing->OpenSendBuffer(size);
            if (retransmitBuf == nullptr) continue;

            std::memcpy(retransmitBuf->Buffer(), pending->GetData(), size);
            retransmitBuf->Close(size);

            _pClientSession->Send(retransmitBuf, pending->destAddr);

            pending->sentAtMs = nowMs;
            didRetransmit = true;
        }
    }

    return didRetransmit;
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
    
    // 숫자 + 대소문자 = 62개
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

bool DediServerService::UpdateGameRooms() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastRoomUpdateTime).count() < 25)
        return false;
    _lastRoomUpdateTime = now;

    for (auto& [roomId, room] : _gameRooms) {
        if (roomId % 4 == _updatePhase) {
            room->ProcessLeaves();
            room->Update();
        }
    }
    _updatePhase = (_updatePhase + 1) % 4;
    return true;
}

void DediServerService::ReserveRoomDestroy(int32_t roomId) {
    _pendingDestroyRooms.push_back(roomId);
}

bool DediServerService::DestroyPendingRooms() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastRoomDestroyTime).count() < 1000)
        return false;
    _lastRoomDestroyTime = now;

    if (_pendingDestroyRooms.empty())
        return false;

    for (int32_t roomId : _pendingDestroyRooms)
        DestroyRoom(roomId);

    _pendingDestroyRooms.clear();
    return true;
}

void DediServerService::DestroyRoom(int32_t roomId) {
    auto it = _gameRooms.find(roomId);
    if (it == _gameRooms.end()) return;

    GameRoom* pRoom = it->second;

    const int32_t playerCount = static_cast<int32_t>(pRoom->GetPlayerSessions().size());
    NotifyRoomDestroyedToMain(roomId, playerCount);

    for (const auto& [sessionId, pSession] : pRoom->GetPlayerSessions()) {
        if (pSession == nullptr) continue;

        // 세션을 반납하기 전에만 읽을 수 있다. 바인딩을 마친 세션은 이미 지워져 no-op
        _tokenToPlayerSession.erase(pSession->GetEntryToken());

        // 슬롯 무효화가 id 반납보다 먼저여야 한다 — 잔여 지연 콜백은 재조회 결과가
        // nullptr 이거나 uid 가 다를 때만 스스로 포기한다
        if (sessionId >= 0 && sessionId < static_cast<int32_t>(_players.size()))
            _players[sessionId] = nullptr;
        _freePlayerIds.push(sessionId);

        ObjectPool<PlayerSession>::Release(pSession);
    }

    _gameRooms.erase(it);
    pRoom->ReleaseThis();

    std::cout << "[DestroyRoom] 룸 회수 완료 (roomId=" << roomId
              << ", 남은 룸=" << _gameRooms.size() << ")" << std::endl;
}