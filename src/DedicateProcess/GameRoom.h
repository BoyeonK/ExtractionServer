#pragma once

#include <vector>
#include <string>
#include <iostream>
#include "absl/container/flat_hash_map.h"
#include "UnityGameObjects/PlayerObject.h"
#include "ExternalProtocol/External_Protocol.pb.h"
#include "MapDataManager.h"
#include "PlayerSession.h"

class SendBuffer;

class GameRoom {
public:
    GameRoom(int32_t roomId, int32_t mapId);
    virtual ~GameRoom();
    virtual void ReleaseThis() = 0;

    virtual void SpawnStaticObject(UnityGameObject* pGameObject) = 0;
    virtual void SpawnDynamicObject(UnityGameObject* pGameObject) = 0;
    virtual void SpawnPlayerObject(PlayerObject* pGameObject, int32_t ownerSessionId) = 0;

    // override 하는 경우 파생 로직을 먼저 처리하고 마지막에 GameRoom::Update() 를 부를 것.
    // 먼저 부르면 그 틱의 변화가 다음 틱에야 나간다
    virtual void Update();

    void FillStaticObjects(std::vector<External_Game_Protocol::D2CResponseBlueprintStaticObjects>& outVec);
    void FillDynamicObjects(std::vector<External_Game_Protocol::D2CResponseSpawnMeDynamicObjects>& outVec);
    void FillPlayerObjects(External_Game_Protocol::D2CSpawnPlayerObjects& outPkt);
    void FillPlayerStates(External_Game_Protocol::D2CUpdatePlayerStates& outPkt);
    virtual void SetSpawnSpot(External_Game_Protocol::D2CResponseSpawnMeSpawnSpot* pPkt) = 0;

    enum MapType : int32_t {
        MAP_NONE = -1,
        MAP_TUTORIAL = 0,
        MAP_WINCHESTER = 1,
        //MAP_DESERT,
        //MAP_FOREST,
        MAP_MAX
    };

    int32_t GetRoomId() const { return _roomId; }

    void RegisterPlayerSession(PlayerSession* pSession);
    PlayerSession* GetPlayerSession(int32_t sessionId);
    PlayerSession* FindSessionByObjectId(int32_t objectId) const;
    const absl::flat_hash_map<int32_t, PlayerSession*>& GetPlayerSessions() const { return _playerSessions; }

    // 예약된 이탈(MarkLeaving)을 진행시키는 유일한 지점. 매 룸 업데이트 직전에 호출된다.
    void ProcessLeaves();

    static constexpr int32_t INVALID_SESSION_ID = -1;

    // OPTION: dynamicObject 의 전송과 INPLAY 전환 사이에 생긴 container 누락 탐지 로직 추가
    template<typename PBType>
    uint32_t Broadcast(const PBType& pkt, SendBuffer* (*makeFn)(const PBType&, PlayerSession*)) {
        return BroadcastExcept(pkt, makeFn, INVALID_SESSION_ID);
    }

    template<typename PBType>
    uint32_t BroadcastExcept(const PBType& pkt, SendBuffer* (*makeFn)(const PBType&, PlayerSession*),
                             int32_t exceptSessionId) {
        uint32_t sentCount = 0;

        for (auto& [sessionId, pSession] : _playerSessions) {
            if (pSession == nullptr || !pSession->IsInplay()) continue;
            if (sessionId == exceptSessionId) continue;

            SendBuffer* pBuffer = makeFn(pkt, pSession);
            if (pBuffer == nullptr) {
                std::cout << "[Broadcast] SendBuffer 확보 실패 (sessionId=" << sessionId << ")" << std::endl;
                continue;
            }

            pSession->Send(pBuffer);
            ++sentCount;
        }

        return sentCount;
    }

    uint32_t GetNewObjectId() { return _nxtObjectId++; }
    UnityGameObject* FindNonplayerObject(uint32_t objectId) const;
    PlayerObject*    FindPlayerObject(uint32_t objectId) const;

    void DestroyDeadObject(uint32_t objectId);

    uint32_t          GetRecallZoneCount() const { return _recallZoneCount; }
    const RecallZone* GetRecallZone(uint32_t index) const;
    bool IsInRecallZone(uint32_t index, const Vector3& pos) const;

protected:
    // 끊김 판정의 유일한 지점. ProcessLeaves() 가 이탈 루프에 들어가기 직전에 부른다
    void DetectDisconnectedSessions();

    // ProcessLeaves() 외의 호출부를 만들지 말 것.
    void DetachPlayer(PlayerSession* pSession);
    void RemovePlayerObject(uint32_t objectId);

    void CheckAllLeft();

    void NotifySpawnObject(UnityGameObject* pGameObject);
    void NotifySpawnPlayerObject(PlayerObject* pGameObject, int32_t ownerSessionId);

    void BroadcastPlayerStates();

    // 실드 재생의 유일한 구동 지점. 통보 패킷은 없다 — 클라이언트가 마지막 D2CNotifyHealthChange
    // 를 기준으로 자기 실드를 예측하고, 서버 권위값은 다음 피격 통보가 정정한다
    void RegenPlayerShields();

    // 프로세스가 멈췄다 재개했을 때 한꺼번에 회복되지 않도록 한 번에 반영할 경과를 제한한다
    static constexpr uint32_t MAX_REGEN_STEP_MS = 1000;

    bool CanBeStaticObject(UnityGameObject* pGameObject) const;

    void SpawnCorpseContainer(PlayerObject* pPlayerObject, const PlayerInventory& inventory);

    int32_t _roomId;
    int32_t _mapId;
    bool    _allLeftReported = false;
    absl::flat_hash_map<int32_t, PlayerSession*> _playerSessions;

    std::vector<Vector3> _spawnSpots;
    uint32_t _spawnSpotIndex = 0;

    const RecallZone* _pRecallZones    = nullptr;
    uint32_t          _recallZoneCount = 0;

    uint32_t _nxtObjectId = 0;

    uint32_t _lastRegenMs = 0;   // 0 = 아직 기준 시각을 잡지 않음

    absl::flat_hash_map<uint32_t, UnityGameObject*> _staticObjects;
    absl::flat_hash_map<uint32_t, UnityGameObject*> _dynamicObjects;
    absl::flat_hash_map<uint32_t, PlayerObject*> _playerObjects;
};

class TestGameRoom : public GameRoom {
public:
    TestGameRoom(int32_t roomId) : GameRoom(roomId, MAP_TUTORIAL) {
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
    void SpawnPlayerObject(PlayerObject* pGameObject, int32_t ownerSessionId) override;
};

class WinchesterGameRoom : public GameRoom {
public:
    WinchesterGameRoom(int32_t roomId) : GameRoom(roomId, MAP_WINCHESTER) {
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
    void SpawnPlayerObject(PlayerObject* pGameObject, int32_t ownerSessionId) override;
};
