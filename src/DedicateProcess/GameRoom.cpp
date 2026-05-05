#include "GameRoom.h"

#include "../ObjectPool.h"
#include "Player.h"
#include "PlayerSession.h" 
#include "DediServerService.h" // pDediServer 전역 변수 접근을 위해 include
#include "UnityGameObjects/TestGameObjects.h"

void GameRoom::RegisterPlayerSession(PlayerSession* pSession) {
    if (pSession == nullptr) return;

    int32_t sessionId = pSession->GetSessionId(); 
    _playerSessions.insert({sessionId, pSession});
}

void GameRoom::FillStaticObjects(std::vector<External_Game_Protocol::D2CResponseBlueprintStaticObjects>& outVec) {
    if (_staticObjects.empty()) {
        External_Game_Protocol::D2CResponseBlueprintStaticObjects emptyPkt;
        emptyPkt.set_index(0);
        emptyPkt.set_is_last(true);
        outVec.push_back(std::move(emptyPkt));
        return;
    }
    
    // 헤더 35, 안전빵 10
    const int32_t SAFE_PAYLOAD_LIMIT = 1024 - 45;

    External_Game_Protocol::D2CResponseBlueprintStaticObjects current;
    int32_t currentPayloadSize = 0;
    uint32_t index = 0;

    for (auto& [id, obj] : _staticObjects) {
        External_Game_Protocol::UnityGameObject pbObj = obj.Serialize();
        int32_t itemSize = pbObj.ByteSizeLong() + 2;

        if (currentPayloadSize + itemSize > SAFE_PAYLOAD_LIMIT) {
            current.set_index(index++);
            current.set_is_last(false);
            outVec.push_back(std::move(current));
            
            current.Clear();
            currentPayloadSize = 0;
        }

        *current.add_ingame_objects() = std::move(pbObj);
        currentPayloadSize += itemSize;
    }

    if (currentPayloadSize > 0) {
        current.set_index(index);
        current.set_is_last(true);
        outVec.push_back(std::move(current));
    }
}

PlayerSession* GameRoom::GetPlayerSession(int32_t sessionId) {
    auto it = _playerSessions.find(sessionId);
    if (it != _playerSessions.end()) {
        return it->second;
    }
    return nullptr;
}

void TestGameRoom::InitTestGameRoom() {
    uint32_t oid = GetNewObjectId();
    TestItemBox* pBox = new TestItemBox(oid, 0, 0, 0);
    SpawnStaticObject(pBox);
}

void TestGameRoom::SetSpawnSpot(External_Game_Protocol::D2CResponseBlueprintSpawnPoint* pPkt) {
    assert(pPkt != nullptr && "SetSpawnSpot - pPkt is null!");
    assert(!_spawnSpots.empty() && "SetSpawnSpot - spawnSpots is empty!");

    _spawnSpots[_spawnSpotIndex].Serialize(pPkt->mutable_spawn_point());
    _spawnSpotIndex = (_spawnSpotIndex + 1) % _spawnSpots.size();
}

void TestGameRoom::ReleaseThis() {
    ObjectPool<TestGameRoom>::Release(this);
}

void TestGameRoom::SpawnStaticObject(UnityGameObject* pGameObject) {
    _staticObjects[GetNewObjectId()] = pGameObject;
    // TODO : 생성 정보를 broadcast
}

void TestGameRoom::SpawnDynamicObject(UnityGameObject* pGameObject) {
    _dynamicObjects[GetNewObjectId()] = pGameObject;
    // TODO : 생성 정보를 broadcast
}

void WinchesterGameRoom::SetSpawnSpot(External_Game_Protocol::D2CResponseBlueprintSpawnPoint* pPkt) {
    assert(pPkt != nullptr && "SetSpawnSpot - pPkt is null!");
    assert(!_spawnSpots.empty() && "SetSpawnSpot - spawnSpots is empty!");

    _spawnSpots[_spawnSpotIndex].Serialize(pPkt->mutable_spawn_point());
    _spawnSpotIndex = (_spawnSpotIndex + 1) % _spawnSpots.size();
}

void WinchesterGameRoom::ReleaseThis() {
    ObjectPool<WinchesterGameRoom>::Release(this);
}

void WinchesterGameRoom::SpawnStaticObject(UnityGameObject* pGameObject) {
    _staticObjects[GetNewObjectId()] = pGameObject;
    // TODO : 생성 정보를 broadcast
}

void WinchesterGameRoom::SpawnDynamicObject(UnityGameObject* pGameObject) {
    _dynamicObjects[GetNewObjectId()] = pGameObject;
    // TODO : 생성 정보를 broadcast
}