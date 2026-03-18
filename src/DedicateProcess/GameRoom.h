#pragma once

#include <vector>
#include <string>
#include <unordered_map>

class Player;
class PlayerSession;

class GameRoom {
public:
    GameRoom(int32_t mapId) : _mapId(mapId) {}

    void RegisterPlayerSession(PlayerSession* pSession);
    PlayerSession* GetPlayerSession(int32_t sessionId);

private:
    int32_t _mapId;
    std::unordered_map<int32_t, PlayerSession*> _playerSessions;
};