#pragma once

#include <vector>
#include <string>
#include <iostream>
#include "absl/container/flat_hash_map.h"
#include "UnityGameObjects/PlayerObject.h"
#include "ExternalProtocol/External_Protocol.pb.h"
#include "MapDataManager.h"
// 브로드캐스트 템플릿이 PlayerSession 의 완전한 타입을 요구한다.
// (PlayerSession.h 는 GameRoom 을 전방선언만 하므로 순환하지 않는다)
#include "PlayerSession.h"

class SendBuffer;

class GameRoom {
public:
    GameRoom(int32_t mapId);
    virtual ~GameRoom() {};
    virtual void ReleaseThis() = 0;
    virtual void SpawnStaticObject(UnityGameObject* pGameObject) = 0;
    virtual void SpawnDynamicObject(UnityGameObject* pGameObject) = 0;
    virtual void SpawnPlayerObject(PlayerObject* pGameObject) = 0;
    virtual void Update() = 0;

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

    void RegisterPlayerSession(PlayerSession* pSession);
    PlayerSession* GetPlayerSession(int32_t sessionId);
    const absl::flat_hash_map<int32_t, PlayerSession*>& GetPlayerSessions() const { return _playerSessions; }

    // ── 룸 브로드캐스트 ──────────────────────────────────────────────────────
    // 세션 id 는 항상 0 이상이므로 -1 은 "제외 대상 없음" 표식으로 안전하다.
    static constexpr int32_t INVALID_SESSION_ID = -1;

    // 같은 룸의 INPLAY 세션 전원에게 같은 패킷을 보낸다.
    //
    // makeFn 에는 ClientPacketHandler 의 Make*() 계열을 그대로 넘기면 된다.
    // reliable / unreliable 구분은 그 함수가 이미 갖고 있으므로 별도 인자가 없다.
    // 반환값은 실제로 전송한 세션 수 — SendBuffer 확보에 실패하면 대상 수보다 작아진다.
    //
    // 대상을 INPLAY 로 한정하는 이유: _playerSessions 에는 아직 제거 로직이 없어
    // 로딩 중(CONNECTED)이거나 이미 빠진 세션이 남아 있다. 로딩 중인 쪽에 스폰 패킷을
    // 보내면 청사진을 받기 전의 오브젝트를 참조하게 된다.
    template<typename PBType>
    uint32_t Broadcast(const PBType& pkt, SendBuffer* (*makeFn)(const PBType&, PlayerSession*)) {
        return BroadcastExcept(pkt, makeFn, INVALID_SESSION_ID);
    }

    // exceptSessionId 세션 하나만 빼고 나머지 INPLAY 세션에게 보낸다.
    // 제외 대상을 포인터가 아닌 세션 id 로 받는 이유는 세션 슬롯 재사용 때문이다.
    template<typename PBType>
    uint32_t BroadcastExcept(const PBType& pkt, SendBuffer* (*makeFn)(const PBType&, PlayerSession*),
                             int32_t exceptSessionId) {
        uint32_t sentCount = 0;

        for (auto& [sessionId, pSession] : _playerSessions) {
            if (pSession == nullptr || !pSession->IsInplay()) continue;
            if (sessionId == exceptSessionId) continue;

            SendBuffer* pBuffer = makeFn(pkt, pSession);
            if (pBuffer == nullptr) {
                // reliable 이었다면 재전송 큐에도 오르지 못하므로 이 세션만 영구 누락된다
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

    // ── 귀환(탈출) 영역 ──
    uint32_t          GetRecallZoneCount() const { return _recallZoneCount; }
    const RecallZone* GetRecallZone(uint32_t index) const;   // 범위 밖이면 nullptr
    bool IsInRecallZone(uint32_t index, const Vector3& pos) const;  // 범위 밖이면 false

protected:
    int32_t _mapId;
    absl::flat_hash_map<int32_t, PlayerSession*> _playerSessions;

    std::vector<Vector3> _spawnSpots;
    uint32_t _spawnSpotIndex = 0;

    // 맵별 정적 테이블을 참조만 한다 (MapDataManager 소유, 복사·해제 없음)
    const RecallZone* _pRecallZones    = nullptr;
    uint32_t          _recallZoneCount = 0;

    uint32_t _nxtObjectId = 0;

    absl::flat_hash_map<uint32_t, UnityGameObject*> _staticObjects;
    absl::flat_hash_map<uint32_t, UnityGameObject*> _dynamicObjects;
    absl::flat_hash_map<uint32_t, PlayerObject*> _playerObjects;
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

    void Update() override;
    void ReleaseThis() override;
    void SpawnStaticObject(UnityGameObject* pGameObject) override;
    void SpawnDynamicObject(UnityGameObject* pGameObject) override;
    void SpawnPlayerObject(PlayerObject* pGameObject) override;
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

    void Update() override;
    void ReleaseThis() override;
    void SpawnStaticObject(UnityGameObject* pGameObject) override;
    void SpawnDynamicObject(UnityGameObject* pGameObject) override;
    void SpawnPlayerObject(PlayerObject* pGameObject) override;
};