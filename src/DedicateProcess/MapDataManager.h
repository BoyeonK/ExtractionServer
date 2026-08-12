#pragma once

#include <cstdint>
#include "UnityGameObjects/UnityGameObject.h"

// 귀환(탈출) 가능 영역 — XZ 평면 원기둥
struct RecallZone {
    float centerX;
    float centerZ;
    float radiusSq;   // 반지름의 제곱
    float minY;       // 높이 하한 (포함)
    float maxY;       // 높이 상한 (포함)

    bool Contains(const Vector3& p) const {
        const float dx = p.x - centerX;
        const float dz = p.z - centerZ;
        if ((dx * dx + dz * dz) > radiusSq) return false;
        return p.y >= minY && p.y <= maxY;
    }
};

class MapDataManager {
public:
    MapDataManager() = delete;
    MapDataManager(const MapDataManager&) = delete;
    MapDataManager& operator=(const MapDataManager&) = delete;

    enum MapId : int32_t {
        MAP_ID_TUTORIAL   = 0,
        MAP_ID_WINCHESTER = 1,
    };

    static const RecallZone* GetRecallZones(int32_t mapId, uint32_t& outCount) {
        switch (mapId) {
        case MAP_ID_TUTORIAL:
            outCount = static_cast<uint32_t>(sizeof(_tutorialRecallZones) / sizeof(_tutorialRecallZones[0]));
            return _tutorialRecallZones;

        case MAP_ID_WINCHESTER:
            outCount = static_cast<uint32_t>(sizeof(_winchesterRecallZones) / sizeof(_winchesterRecallZones[0]));
            return _winchesterRecallZones;

        default:
            outCount = 0;
            return nullptr;
        }
    }

private:
    // 배열 인덱스 = C2DRequestRecall 의 귀환 스팟 인덱스 (클라이언트와의 계약).
    static constexpr RecallZone _tutorialRecallZones[] = {
        // 0 : 탈출구
        {  10.0f,  10.0f, 5.5f * 5.5f, -5.0f, 5.0f },
    };

    // TODO : 좌표·반경·높이는 실제 맵 지오메트리에 맞춰 확정 필요 (현재는 자리표시자)
    static constexpr RecallZone _winchesterRecallZones[] = {
        // 0 : 북측 탈출구
        {   0.0f,  80.0f, 6.5f * 6.5f, -1.0f, 4.0f },
        // 1 : 남측 탈출구
        {   0.0f, -80.0f, 6.5f * 6.5f, -1.0f, 4.0f },
        // 2 : 동측 탈출구
        {  80.0f,   0.0f, 6.5f * 6.5f, -1.0f, 4.0f },
    };
};
