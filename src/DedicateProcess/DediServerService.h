#pragma once

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <cstdint> 
#include "../IPCProtocol/IPC_HTTP.pb.h"
#include "../IPCProtocol/IPC_Dedicate.pb.h"

class D2MSession;
class D2CSession;
class GameRoom;
class PlayerSession;

// TODO :
// 1. UDP Session 완성
// 2. 매칭된 플레이어들을 하나의 방으로 묶어 관리
// 3. 매칭된 플레이어에게 방이 할당되면 Redis에 상태변수 갱신 및 접속할 IP주소 및 포트 등록
// 4. 접속 요청된 플레이어의 정보를 저장하고, 해당 유저가 올바른 ticket을 가지고 있다면 핸들러 함수 허용

class DediServerService {
public:
    DediServerService();
    ~DediServerService();

    bool Init() {
        if (InitMainIPC() == false)
            return false;

        if (InitUDP() == false)
            return false;

        return true;
    }

    bool InitMainIPC();
    void SendIdentityPacket();
    bool InitUDP();
    bool MakeRoomForThisGroup(int mapId, const std::vector<std::string>& ticketIds);

    PlayerSession* GetPlayerSession(int16_t sessionId);
    bool Handle_H2M2D_BCITSpkt(IPC_Protocol::H2M2DBindClientIpToSession& pkt);

private:
    int32_t GetFreeSessionId();
    std::string GetUniqueToken();

    static IPC_Protocol::D2MUpdateEntryToken MakeD2MUpdateEntryTokenPkt(int32_t udpPort, const std::vector<std::string>& ticketIds, const std::vector<int32_t>& sessionIds, const std::vector<std::string>& tokens, const std::vector<int32_t>& securityKeys) {
        IPC_Protocol::D2MUpdateEntryToken pkt;
        if (ticketIds.size() == sessionIds.size() && sessionIds.size() == tokens.size() && tokens.size() == securityKeys.size()) {
            pkt.set_size(static_cast<int32_t>(ticketIds.size()));
            pkt.set_port(udpPort);

            pkt.mutable_ticket_ids()->Reserve(ticketIds.size());
            pkt.mutable_session_ids()->Reserve(sessionIds.size());
            pkt.mutable_entry_tokens()->Reserve(tokens.size());
            pkt.mutable_security_keys()->Reserve(securityKeys.size());
            for (size_t i=0; i < ticketIds.size(); i++) {
                pkt.add_ticket_ids(ticketIds[i]);
                pkt.add_session_ids(sessionIds[i]);
                pkt.add_entry_tokens(tokens[i]);
                pkt.add_security_keys(securityKeys[i]);
            }
        } else {
            pkt.set_size(-1);
        }
        return pkt;
    }

private:
    int _dediFd = -1;
    D2MSession* _pD2MSession = nullptr;
    
    int _udpFd = -1;
    D2CSession* _pClientSession = nullptr;
    uint16_t _udpPort = 0;

    //TODO : UDP로 접속할 클라이언트들의 그룹인 GameRoom을 담은 컨테이너 추가
    std::unordered_map<int32_t, GameRoom*> _gameRooms;
    std::vector<PlayerSession*> _players;
    std::unordered_map<std::string, PlayerSession*> _tokenToPlayerSession;

    std::queue<int32_t> _freePlayerIds;
};

extern DediServerService* pDediServer;