#pragma once

#include <cstdint>
#include "UnityGameObjects/UnityGameObject.h"

// 귀환(탈출) 가능 영역 — XZ 평면 원기둥
//
// 클라이언트와 형태를 맞출 필요가 없는 서버 전용 표현이다.
// 따라서 반지름은 미리 제곱해 보관하여 런타임 곱셈·sqrt를 없앴다.
// 회전 불변이므로 클라이언트가 트리거 콜라이더를 회전시켜도 판정이 어긋나지 않는다.
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

// 맵별 정적 데이터 조회 (ItemDataManager 와 동일한 정적 클래스 패턴)
//
// 여기의 테이블은 맵당 불변 상수이므로 GameRoom 이 값으로 복사해 보관하지 않는다.
// GameRoom 은 포인터와 개수만 들고 있어 룸 개수가 늘어나도 복제·힙 할당이 발생하지 않는다.
class MapDataManager {
public:
    MapDataManager() = delete;
    MapDataManager(const MapDataManager&) = delete;
    MapDataManager& operator=(const MapDataManager&) = delete;

    // GameRoom::MapType 과 값이 일치해야 한다 (GameRoom.cpp 의 static_assert 로 검증)
    enum MapId : int32_t {
        MAP_ID_TUTORIAL   = 0,
        MAP_ID_WINCHESTER = 1,
    };

    // mapId 에 해당하는 귀환 영역 테이블을 반환한다.
    // 반환 포인터는 정적 수명을 가지므로 호출자가 소유권을 신경 쓸 필요가 없다.
    // 알 수 없는 mapId 이면 nullptr / count=0.
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
    // ── 귀환 영역 테이블 ─────────────────────────────────────────────────────
    // 배열 인덱스 = C2DRequestRecall 의 '귀환 스팟 인덱스'.
    // 이 순서는 클라이언트와의 계약이므로 중간 삽입·순서 변경 금지 (추가는 뒤에만).
    //
    // radius 는 클라이언트 트리거 콜라이더보다 여유 있게(약 +0.5m) 잡을 것.
    // 서버 영역이 클라이언트 트리거보다 좁으면 "귀환 UI 는 떴는데 서버가 거부"하는 버그가 된다.
    //
    // TODO : 좌표·반경·높이는 실제 맵 지오메트리에 맞춰 확정 필요 (현재는 자리표시자)

    static constexpr RecallZone _tutorialRecallZones[] = {
        // 0 : 북동 탈출구  (중심 50, 50 / 반경 5.5m / 높이 -1 ~ 4m)
        {  50.0f,  50.0f, 5.5f * 5.5f, -1.0f, 4.0f },
        // 1 : 남서 탈출구  (중심 -50, -50 / 반경 5.5m / 높이 -1 ~ 4m)
        { -50.0f, -50.0f, 5.5f * 5.5f, -1.0f, 4.0f },
    };

    static constexpr RecallZone _winchesterRecallZones[] = {
        // 0 : 북측 탈출구  (중심 0, 80 / 반경 6.5m / 높이 -1 ~ 4m)
        {   0.0f,  80.0f, 6.5f * 6.5f, -1.0f, 4.0f },
        // 1 : 남측 탈출구  (중심 0, -80 / 반경 6.5m / 높이 -1 ~ 4m)
        {   0.0f, -80.0f, 6.5f * 6.5f, -1.0f, 4.0f },
        // 2 : 동측 탈출구  (중심 80, 0 / 반경 6.5m / 높이 -1 ~ 4m)
        {  80.0f,   0.0f, 6.5f * 6.5f, -1.0f, 4.0f },
    };
};
