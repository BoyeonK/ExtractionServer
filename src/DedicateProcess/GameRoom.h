#pragma once

#include <vector>
#include <string>
#include <unordered_map>

class Player;
class PlayerSession;



class GameRoom {
public:
    GameRoom(int32_t mapId) : _mapId(mapId) {}
    virtual ~GameRoom() {};
    virtual void ReleaseThis() = 0;

    enum MapType : int32_t {
        MAP_NONE = -1,
        MAP_TUTORIAL = 0,
        MAP_WINCHESTER = 1,
        //MAP_DESERT,
        //MAP_FOREST,
        MAP_MAX
    };

    void RegisterPlayerSession(PlayerSession* pSession);
    PlayerSession* GetPlayerSession(int32_t sessionId);

private:
    int32_t _mapId;
    std::unordered_map<int32_t, PlayerSession*> _playerSessions;
};

class TestGameRoom : public GameRoom {
public:
    TestGameRoom() : GameRoom(MAP_TUTORIAL) {}
    virtual ~TestGameRoom() {};

    void ReleaseThis() override;
}

class WinchesterGameRoom : public GameRoom {
public:
    WinchesterGameRoom() : GameRoom(MAP_WINCHESTER) {}
    virtual ~WinchesterGameRoom() {};

    void ReleaseThis() override;
}