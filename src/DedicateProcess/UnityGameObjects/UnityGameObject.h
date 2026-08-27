#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <stdint.h>
#include <cassert>
#include <string>
#include "../ExternalProtocol/External_Unity_Object.pb.h"

enum class ObjectType : int16_t {
    None = 0,
    Player = 1,
    TestItemBox = 2,
    PlayerLoot = 3,
};

// 타입당 고정 이름. 인스턴스마다 이름이 다른 오브젝트는 GetObjectName() 을 override 한다
inline const std::string OBJECT_NAME_NONE          = "None";
inline const std::string OBJECT_NAME_PLAYER        = "Player";
inline const std::string OBJECT_NAME_TEST_ITEM_BOX = "TestItemBox";
inline const std::string OBJECT_NAME_PLAYER_LOOT   = "PlayerLoot";

// 이름을 특정할 수 없을 때. 사유(가해자 부재·조회 실패)는 클라이언트에 알리지 않는다
inline const std::string OBJECT_NAME_UNRESOLVED = "";

inline const std::string& ObjectTypeToName(ObjectType objectType) {
    switch (objectType) {
        case ObjectType::Player:      return OBJECT_NAME_PLAYER;
        case ObjectType::TestItemBox: return OBJECT_NAME_TEST_ITEM_BOX;
        case ObjectType::PlayerLoot:  return OBJECT_NAME_PLAYER_LOOT;
        default:                      return OBJECT_NAME_NONE;
    }
}

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    void Serialize(External_Game_Protocol::Vector3* pVector3) const {
        assert(pVector3 != nullptr && "Vector3::CopyTo - pVector3 is null!");

        pVector3->set_x(x);
        pVector3->set_y(y);
        pVector3->set_z(z);
    }
};

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    Quaternion() = default;
    Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    // 쿼터니언 128비트 → 32비트 압축
    void Serialize(External_Game_Protocol::TransformInfo* pTrans) const {
        assert(pTrans != nullptr && "Quaternion::Serialize - pTrans is null!");

        const float comp[4] = { x, y, z, w };

        uint32_t maxIndex = 0;
        float maxValue = std::abs(comp[0]);
        for (int i = 1; i < 4; ++i) {
            float absVal = std::abs(comp[i]);
            if (absVal > maxValue) {
                maxIndex = i;
                maxValue = absVal;
            }
        }

        float sign = (comp[maxIndex] < 0.0f) ? -1.0f : 1.0f;

        // 가장 큰 값을 제외한 나머지 3개
        float out[3];
        int outIdx = 0;
        for (int i = 0; i < 4; ++i) {
            if (i != maxIndex) {
                out[outIdx++] = comp[i] * sign;
            }
        }

        uint32_t packA = PackFloat(out[0]);
        uint32_t packB = PackFloat(out[1]);
        uint32_t packC = PackFloat(out[2]);

        // [2b maxIndex][10b][10b][10b]
        uint32_t compressed = (maxIndex << 30) | (packA << 20) | (packB << 10) | packC;

        pTrans->set_compressed_quat(compressed);
    }

    // (패킷 -> 메모리) 32비트를 다시 128비트 원본으로 복원
    void DeserializeFrom(uint32_t compressed_quat) {
        uint32_t maxIndex = (compressed_quat >> 30) & 0x03;
        float abc[3] = {
            UnpackFloat((compressed_quat >> 20) & 0x3FF),
            UnpackFloat((compressed_quat >> 10) & 0x3FF),
            UnpackFloat( compressed_quat        & 0x3FF)
        };

        float d = std::sqrt(std::max(1.0f - (abc[0]*abc[0] + abc[1]*abc[1] + abc[2]*abc[2]), 0.0f));

        float comp[4];
        int outIdx = 0;
        for (int i = 0; i < 4; ++i) {
            comp[i] = (i == maxIndex) ? d : abc[outIdx++];
        }

        x = comp[0]; y = comp[1]; z = comp[2]; w = comp[3];
    }

private:
    // 쿼터니언 성분의 남은 3개 값은 수학적으로 무조건 [-0.707106, 0.707106] 범위를 넘지 못합니다.
    static uint32_t PackFloat(float val) {
        // 1.41421356f (루트 2)를 곱해 범위를 [-1.0, 1.0]으로 확장한 뒤, 511.5를 곱해 [0, 1023]으로 매핑합니다.
        float mapped = (val * 1.41421356f + 1.0f) * 511.5f;
        return static_cast<uint32_t>(std::clamp(std::round(mapped), 0.0f, 1023.0f));
    }

    static float UnpackFloat(uint32_t val) {
        // [0, 1023]을 다시 [-0.707106, 0.707106]으로 복원합니다.
        float mapped = static_cast<float>(val) / 511.5f - 1.0f;
        return mapped * 0.70710678f; 
    }
};

class UnityGameObject {
private:
    UnityGameObject() = delete;

public:
    UnityGameObject(uint32_t objectId, ObjectType objectType, bool isYFixed, float x, float y, float z) :
        objectId(objectId),
        objectType(objectType),
        state(0),
        IsYFixed(isYFixed),
        position(Vector3(x, y, z))
    {}

    UnityGameObject(uint32_t objectId, ObjectType objectType, bool isYFixed, Vector3 position) :
        objectId(objectId),
        objectType(objectType),
        state(0),
        IsYFixed(isYFixed),
        position(position)
    {}

    virtual ~UnityGameObject() = default;

    virtual const std::string& GetObjectName() const { return ObjectTypeToName(objectType); }

    void Serialize(External_Game_Protocol::UnityGameObject* pPkt) const;
    External_Game_Protocol::UnityGameObject Serialize() const;

    uint32_t objectId;
    ObjectType objectType = ObjectType::None;
    uint16_t state;

    virtual void Update() {};

    Vector3 position;

    bool IsYFixed;
    Quaternion quaternion;
    float yawAngle = 0;
};