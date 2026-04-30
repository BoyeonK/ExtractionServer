#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "UnityGameObject.h"
#include "ExternalProtocol/External_Protocol.pb.h"

class Player;
class PlayerSession;

class GameRoom {
public:
    GameRoom(int32_t mapId) : _mapId(mapId) {}
    virtual ~GameRoom() {};
    virtual void ReleaseThis() = 0;
    virtual void Spawn() = 0;
    virtual void Update() {};

    void FillStaticObjects(std::vector<External_Game_Protocol::D2CResponseBlueprint>& outVec);

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

protected:
    int32_t _mapId;
    std::unordered_map<int32_t, PlayerSession*> _playerSessions;

    std::vector<Vector3> _spawnSpots;
    uint32_t sponSpot = 0;
    uint32_t maxSponSpot = 0;

    std::unordered_map<int32_t, UnityGameObject> _staticObjects;
    std::unordered_map<int32_t, UnityGameObject> _dynamicObjects;
    std::unordered_map<int32_t, UnityGameObject> _Players;
};

class TestGameRoom : public GameRoom {
public:
    TestGameRoom() : GameRoom(MAP_TUTORIAL) {
        _spawnSpots.reserve(4);
        _spawnSpots.emplace_back(10.0f, 0.0f, 0.0f);
        _spawnSpots.emplace_back(-10.0f, 0.0f, 0.0f);
        _spawnSpots.emplace_back(0.0f, 0.0f, 10.0f);
        _spawnSpots.emplace_back(0.0f, 0.0f, -10.0f);
        maxSponSpot = 4;
    }
    virtual ~TestGameRoom() {};

    void ReleaseThis() override;
    void Spawn() override;
};

class WinchesterGameRoom : public GameRoom {
public:
    WinchesterGameRoom() : GameRoom(MAP_WINCHESTER) {
        _spawnSpots.reserve(4);
        _spawnSpots.emplace_back(10.0f, 0.0f, 0.0f);
        _spawnSpots.emplace_back(-10.0f, 0.0f, 0.0f);
        _spawnSpots.emplace_back(0.0f, 0.0f, 10.0f);
        _spawnSpots.emplace_back(0.0f, 0.0f, -10.0f);
        maxSponSpot = 4;
    }
    virtual ~WinchesterGameRoom() {};

    void ReleaseThis() override;
    void Spawn() override;
};