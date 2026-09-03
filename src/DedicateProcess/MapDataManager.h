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

// 맵에 고정 배치되는 컨테이너. yawAngle 은 도 단위(클라이언트 오일러 y)
struct MapContainerSpawn {
    ObjectType type;
    Vector3    position;
    float      yawAngle;
};

// 장비 배치 쿼터 — containerCount 대의 컨테이너에 blueprintId 를 1개씩 넣는다
struct MapLootEquipQuota {
    uint32_t blueprintId;
    uint32_t containerCount;
};

class MapDataManager {
public:
    MapDataManager() = delete;
    MapDataManager(const MapDataManager&) = delete;
    MapDataManager& operator=(const MapDataManager&) = delete;

    enum MapId : int32_t {
        MAP_ID_TUTORIAL = 0,
        MAP_ID_TENERIFE = 1,
    };

    static const RecallZone* GetRecallZones(int32_t mapId, uint32_t& outCount) {
        switch (mapId) {
        case MAP_ID_TUTORIAL:
            outCount = static_cast<uint32_t>(sizeof(_tutorialRecallZones) / sizeof(_tutorialRecallZones[0]));
            return _tutorialRecallZones;

        case MAP_ID_TENERIFE:
            outCount = static_cast<uint32_t>(sizeof(_tenerifeRecallZones) / sizeof(_tenerifeRecallZones[0]));
            return _tenerifeRecallZones;

        default:
            outCount = 0;
            return nullptr;
        }
    }

    static const MapContainerSpawn* GetContainerSpawns(int32_t mapId, uint32_t& outCount) {
        switch (mapId) {
        case MAP_ID_TENERIFE:
            outCount = static_cast<uint32_t>(sizeof(_tenerifeContainerSpawns) / sizeof(_tenerifeContainerSpawns[0]));
            return _tenerifeContainerSpawns;

        default:
            outCount = 0;
            return nullptr;
        }
    }

    static const uint32_t* GetLootAmmoPool(int32_t mapId, uint32_t& outCount) {
        switch (mapId) {
        case MAP_ID_TENERIFE:
            outCount = static_cast<uint32_t>(sizeof(_tenerifeAmmoPool) / sizeof(_tenerifeAmmoPool[0]));
            return _tenerifeAmmoPool;

        default:
            outCount = 0;
            return nullptr;
        }
    }

    static const MapLootEquipQuota* GetLootEquipQuotas(int32_t mapId, uint32_t& outCount) {
        switch (mapId) {
        case MAP_ID_TENERIFE:
            outCount = static_cast<uint32_t>(sizeof(_tenerifeEquipQuotas) / sizeof(_tenerifeEquipQuotas[0]));
            return _tenerifeEquipQuotas;

        default:
            outCount = 0;
            return nullptr;
        }
    }

    // 기본 배치(전 컨테이너)와 추가 배치(일부 컨테이너)의 탄약 수량 범위. 양 끝 포함
    static constexpr int32_t  TENERIFE_BASE_AMMO_MIN  = 8;
    static constexpr int32_t  TENERIFE_BASE_AMMO_MAX  = 16;
    static constexpr uint32_t TENERIFE_EXTRA_AMMO_CONTAINERS = 20;
    static constexpr int32_t  TENERIFE_EXTRA_AMMO_MIN = 10;
    static constexpr int32_t  TENERIFE_EXTRA_AMMO_MAX = 20;

private:
    // 배열 인덱스 = C2DRequestRecall 의 귀환 스팟 인덱스 (클라이언트와의 계약).
    static constexpr RecallZone _tutorialRecallZones[] = {
        // 0 : 탈출구
        {  10.0f,  10.0f, 5.5f * 5.5f, -5.0f, 5.0f },
    };

    static constexpr RecallZone _tenerifeRecallZones[] = {
        {    0.0f,  70.00f, 8.0f * 8.0f, -2.0f, 5.0f },
        {  174.52f, -48.03f, 8.0f * 8.0f, -2.0f, 5.0f },
        {  165.0f,   58.78f, 8.0f * 8.0f, -2.0f, 5.0f },
    };

    static constexpr MapContainerSpawn _tenerifeContainerSpawns[] = {
        { ObjectType::TenerifeBlueCar,   {  -84.48f, 0.01f, -70.48f },  -90.0f },
        { ObjectType::TenerifeBlueCar,   { -135.60f, 0.01f, -73.45f },   90.0f },
        { ObjectType::TenerifeBlueCar,   { -100.50f, 0.01f, -73.45f },   90.0f },
        { ObjectType::TenerifeBlueCar,   {   41.81f, 0.01f, -73.53f },   90.0f },
        { ObjectType::TenerifeBlueCar,   {  -93.11f, 0.13f, -87.62f },    0.0f },
        { ObjectType::TenerifeBlueCar,   {   37.31f, 0.01f, -79.77f },    0.0f },
        { ObjectType::TenerifeBlueCar,   {   64.03f, 0.01f, -62.65f },   90.0f },
        { ObjectType::TenerifeBlueCar,   {   87.74f, 0.01f, -26.67f },   90.0f },
        { ObjectType::TenerifeBlueCar,   {   43.86f, 0.01f,  -1.37f },   90.0f },
        { ObjectType::TenerifeBlueCar,   {  -15.71f, 0.01f,   1.62f },  -90.0f },
        { ObjectType::TenerifeBlueCar,   {  -28.42f, 0.01f,  -1.45f },   90.0f },
        { ObjectType::TenerifeBlueCar,   {   29.97f, 0.01f, -34.41f },  -90.0f },
        { ObjectType::TenerifeBlueCar,   {  137.17f, 0.01f, -70.57f },  -90.0f },
        { ObjectType::TenerifeBlueCar,   {  -70.51f, 0.01f, -56.71f },    0.0f },
        { ObjectType::TenerifeBlueCar,   { -151.88f, 0.01f, -62.68f },  -90.0f },
        { ObjectType::TenerifeBlueCar,   {  -70.53f, 0.01f,  -8.93f },    0.0f },
        { ObjectType::TenerifeBlueCar,   { -102.23f, 0.01f,   1.53f },  -90.0f },

        { ObjectType::TenerifeYellowCar, {   24.41f, 0.01f, -87.89f },    0.0f },
        { ObjectType::TenerifeYellowCar, {   64.03f, 0.01f, -47.49f },  -90.0f },
        { ObjectType::TenerifeYellowCar, {  102.43f, 0.01f, -37.50f },   90.0f },
        { ObjectType::TenerifeYellowCar, { -102.07f, 0.13f, -79.84f },   90.0f },
        { ObjectType::TenerifeYellowCar, {  145.50f, 0.01f, -61.93f },    0.0f },
        { ObjectType::TenerifeYellowCar, {  -53.85f, 0.13f, -73.46f },   90.0f },
        { ObjectType::TenerifeYellowCar, {   56.45f, 0.01f, -49.63f },   90.0f },
        { ObjectType::TenerifeYellowCar, {  142.61f, 0.01f, -22.04f },  180.0f },
        { ObjectType::TenerifeYellowCar, {   57.85f, 0.01f,   1.69f },  -90.0f },
        { ObjectType::TenerifeYellowCar, {   30.12f, 0.01f,   1.66f },  -90.0f },
        { ObjectType::TenerifeYellowCar, {   23.44f, 0.01f,  -1.39f },   90.0f },
        { ObjectType::TenerifeYellowCar, {  -51.81f, 0.01f,   1.63f },  -90.0f },
        { ObjectType::TenerifeYellowCar, { -145.39f, 0.01f,  -8.84f },  180.0f },
        { ObjectType::TenerifeYellowCar, {   -7.97f, 0.01f, -37.58f },   90.0f },
        { ObjectType::TenerifeYellowCar, {   87.59f, 0.01f, -70.34f },  -90.0f },
        { ObjectType::TenerifeYellowCar, {  -73.33f, 0.01f, -26.77f },  180.0f },

        { ObjectType::TenerifeBrownCar,  {  -86.29f, 0.13f, -79.87f },    0.0f },
        { ObjectType::TenerifeBrownCar,  {  127.98f, 0.01f,   1.55f },  -90.0f },
        { ObjectType::TenerifeBrownCar,  {   26.55f, 0.01f, -80.00f },    0.0f },
        { ObjectType::TenerifeBrownCar,  {  134.02f, 0.01f, -37.33f },   90.0f },
        { ObjectType::TenerifeBrownCar,  { -142.43f, 0.01f, -48.45f },    0.0f },
        { ObjectType::TenerifeBrownCar,  { -142.63f, 0.01f, -24.10f },    0.0f },
        { ObjectType::TenerifeBrownCar,  {  -30.59f, 0.01f, -34.47f },  -90.0f },
        { ObjectType::TenerifeBrownCar,  {   13.75f, 0.01f, -37.53f },   90.0f },
        { ObjectType::TenerifeBrownCar,  { -123.33f, 0.01f, -70.50f },  -90.0f },
        { ObjectType::TenerifeBrownCar,  {  -57.03f, 0.01f, -37.38f },   90.0f },
        { ObjectType::TenerifeBrownCar,  {   79.72f, 0.01f, -24.58f },   90.0f },
        { ObjectType::TenerifeBrownCar,  { -134.75f, 0.01f,  -1.36f },   90.0f },

        { ObjectType::TenerifeRedCar,    { -131.39f, 0.01f, -37.48f },   90.0f },
        { ObjectType::TenerifeRedCar,    {   87.71f, 0.01f, -15.70f },  -90.0f },
        { ObjectType::TenerifeRedCar,    {    1.40f, 0.01f, -46.76f },    0.0f },
        { ObjectType::TenerifeRedCar,    {   60.77f, 0.00f, -37.42f },   90.0f },
        { ObjectType::TenerifeRedCar,    {  -29.19f, 0.01f, -70.47f },  -90.0f },
        { ObjectType::TenerifeRedCar,    {  119.78f, 0.01f,  -1.32f },   90.0f },
        { ObjectType::TenerifeRedCar,    {   56.62f, 0.01f, -58.28f },   90.0f },
        { ObjectType::TenerifeRedCar,    {  124.16f, 0.01f, -34.48f },  -90.0f },
        { ObjectType::TenerifeRedCar,    {   61.42f, -0.03f, -1.31f },   90.0f },
        { ObjectType::TenerifeRedCar,    {  -87.25f, 0.01f,  -1.31f },   90.0f },
        { ObjectType::TenerifeRedCar,    {   98.94f, 0.01f,  -1.20f },   90.0f },
        { ObjectType::TenerifeRedCar,    {  104.99f, 0.01f, -73.47f },   90.0f },
        { ObjectType::TenerifeRedCar,    {   87.61f, 0.01f, -22.38f },  -90.0f },
        { ObjectType::TenerifeRedCar,    {   70.74f, 0.01f,  -8.25f },  180.0f },

        { ObjectType::TenerifeBus,       {  -10.69f, 0.01f, -73.42f },   90.0f },
        { ObjectType::TenerifeBus,       {   57.32f, 0.01f, -55.10f },   90.0f },
        { ObjectType::TenerifeBus,       {  145.53f, 0.01f,  -9.01f },    0.0f },
        { ObjectType::TenerifeBus,       {    9.15f, 0.01f,   1.52f },   90.0f },
        { ObjectType::TenerifeBus,       {  -96.21f, 0.01f, -34.37f },   90.0f },
    };

    // 배치 시 ItemDataManager::GetType() 으로 카테고리를 확인한다 — DB 와 갈리면 그 항목만 건너뛴다
    static constexpr uint32_t _tenerifeAmmoPool[] = { 5, 6 };

    // 합이 장비를 받는 컨테이너 수(20)다. 서로 다른 컨테이너에 하나씩 들어간다
    static constexpr MapLootEquipQuota _tenerifeEquipQuotas[] = {
        { 1, 4 },   // AK-47
        { 2, 4 },   // M4A1
        { 3, 8 },   // SCAR
        { 4, 4 },   // 경량 조끼
    };
};
