#pragma once

#include <string>
#include <netinet/in.h>
#include <chrono>
#include <array>
#include "enum.h"

class GameRoom;

class PlayerSession {
public:
    PlayerSession(const std::string& ticket, const std::string& token, int32_t sessionId, GameRoom* pRoom);

    const std::string& GetEntryToken() const;

    int32_t GetSessionId() const { return _sessionId; }
    GameRoom* GetGameRoom() const { return _pRoom; }
    uint32_t GetSecurityKey() const { return _securityKey; }
    uint32_t GenerateSequenceNum(uint16_t packetId) {
        if (packetId < PKT_ID_MAX) {
            return ++_sequenceNums[packetId];
        }
        return 0;
    }
    bool IsNewSequenceNum(uint16_t packetId, uint32_t seqNum);
    sockaddr_in GetAddress() const { return _clientAddr; }

    void SetIp(const std::string& ip);
    void SetPort(uint16_t port);

private:
    int32_t _uid = 0;
    std::string _ticket;
    std::string _entryToken;
    int32_t _sessionId;
    std::array<uint32_t, PKT_ID_MAX> _sequenceNums = {};
    uint32_t _securityKey;
    GameRoom* _pRoom;
    
    sockaddr_in _clientAddr = {}; 
    std::chrono::time_point<std::chrono::steady_clock> _lastRecvTime;
};