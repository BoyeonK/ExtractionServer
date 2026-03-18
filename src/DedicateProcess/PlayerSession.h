#pragma once

#include <string>
#include <netinet/in.h>
#include <chrono>

class GameRoom;

class PlayerSession {
public:
    PlayerSession(const std::string& ticket, const std::string& token, int32_t sessionId, GameRoom* pRoom);

    const std::string& GetEntryToken() const;

    int32_t GetSessionId() const { return _sessionId; }
    GameRoom* GetGameRoom() const { return _pRoom; }

private:
    int32_t _uid = 0;
    std::string _ticket;
    std::string _entryToken;
    int32_t _sessionId;
    GameRoom* _pRoom;
    
    sockaddr_in _clientAddr = {}; 
    std::chrono::time_point<std::chrono::steady_clock> _lastRecvTime;
};