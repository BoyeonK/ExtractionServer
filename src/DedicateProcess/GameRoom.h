#pragma once

#include <vector>
#include <string>
#include "absl/container/flat_hash_map.h"
#include "UnityGameObjects/UnityGameObject.h"
#include "ExternalProtocol/External_Protocol.pb.h"

class PlayerSession;

class GameRoom {
public:
    GameRoom(int32_t mapId) : _mapId(mapId) {}
    virtual ~GameRoom() {};
    virtual void ReleaseThis() = 0;
    virtual void SpawnStaticObject(UnityGameObject* pGameObject) = 0;
    virtual void SpawnDynamicObject(UnityGameObject* pGameObject) = 0;
    virtual void Update() {};

    void FillStaticObjects(std::vector<External_Game_Protocol::D2CResponseBlueprintStaticObjects>& outVec);
    void FillDynamicObjects(std::vector<External_Game_Protocol::D2CResponseSpawnMeDynamicObjects>& outVec);
    virtual void SetSpawnSpot(External_Game_Protocol::D2CResponseSpawnMeSpawnSpot* pPkt) = 0;

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
    absl::flat_hash_map<int32_t, PlayerSession*> _playerSessions;

    std::vector<Vector3> _spawnSpots;
    uint32_t _spawnSpotIndex = 0;

    uint32_t _nxtObjectId = 0;
    uint32_t GetNewObjectId() { return _nxtObjectId++; }

    absl::flat_hash_map<uint32_t, UnityGameObject*> _staticObjects;
    absl::flat_hash_map<uint32_t, UnityGameObject*> _dynamicObjects;
    absl::flat_hash_map<uint32_t, UnityGameObject*> _playerObjects;
};

class TestGameRoom : public GameRoom {
public:
    TestGameRoom() : GameRoom(MAP_TUTORIAL) {
        _spawnSpots.reserve(4);
        _spawnSpots.emplace_back(10.0f, 0.0f, 0.0f);
        _spawnSpots.emplace_back(-10.0f, 0.0f, 0.0f);
        _spawnSpots.emplace_back(0.0f, 0.0f, 10.0f);
        _spawnSpots.emplace_back(0.0f, 0.0f, -10.0f);

        InitTestGameRoom();
    }
    virtual ~TestGameRoom() {};

    void InitTestGameRoom();

    void SetSpawnSpot(External_Game_Protocol::D2CResponseSpawnMeSpawnSpot* pPkt) override;

    void ReleaseThis() override;
    void SpawnStaticObject(UnityGameObject* pGameObject) override;
    void SpawnDynamicObject(UnityGameObject* pGameObject) override;
};

class WinchesterGameRoom : public GameRoom {
public:
    WinchesterGameRoom() : GameRoom(MAP_WINCHESTER) {
        _spawnSpots.reserve(4);
        _spawnSpots.emplace_back(10.0f, 0.0f, 0.0f);
        _spawnSpots.emplace_back(-10.0f, 0.0f, 0.0f);
        _spawnSpots.emplace_back(0.0f, 0.0f, 10.0f);
        _spawnSpots.emplace_back(0.0f, 0.0f, -10.0f);
    }
    virtual ~WinchesterGameRoom() {};

    void SetSpawnSpot(External_Game_Protocol::D2CResponseSpawnMeSpawnSpot* pPkt) override;

    void ReleaseThis() override;
    void SpawnStaticObject(UnityGameObject* pGameObject) override;
    void SpawnDynamicObject(UnityGameObject* pGameObject) override;
};