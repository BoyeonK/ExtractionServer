#include <cstdint>
#include <cmath>
#include <algorithm>
#include <stdint.h>
#include <cassert>
#include "ExternalProtocol/External_Unity_Object.pb.h"

enum class ObjectType : int16_t {
    None = -1,
    Player = 0,
    TestItemBox = 1,
};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default; // 기본 생성자
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

    // (메모리 -> 패킷) 128비트를 32비트로 압축
    void Serialize(External_Game_Protocol::TransformInfo* pTrans) const {
        assert(pTrans != nullptr && "Quaternion::Serialize - pTrans is null!");

        const float comp[4] = { x, y, z, w };

        // 가장 큰 값의 인덱스 찾기
        uint32_t maxIndex = 0;
        float maxValue = std::abs(comp[0]);
        for (int i = 1; i < 4; ++i) {
            float absVal = std::abs(comp[i]);
            if (absVal > maxValue) {
                maxIndex = i;
                maxValue = absVal;
            }
        }

        // 부호 결정
        float sign = (comp[maxIndex] < 0.0f) ? -1.0f : 1.0f;

        // 가장 큰 값을 제외한 나머지 3개의 값을 배열로 추출 (if-else 체인 삭제)
        float out[3];
        int outIdx = 0;
        for (int i = 0; i < 4; ++i) {
            if (i != maxIndex) {
                out[outIdx++] = comp[i] * sign;
            }
        }

        // 10비트 압축
        uint32_t packA = PackFloat(out[0]);
        uint32_t packB = PackFloat(out[1]);
        uint32_t packC = PackFloat(out[2]);

        // 32비트 하나로 병합
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
    UnityGameObject(uint32_t objectId, ObjectType objectType) :
        objectId(objectId),
        objectType(objectType),
        state(0)
    {}

    void Serialize(External_Game_Protocol::UnityGameObject* pPkt) const;
    External_Game_Protocol::UnityGameObject Serialize() const;

    uint32_t objectId;
    ObjectType objectType = ObjectType::None;
    uint16_t state;

    Vector3 position;

    bool IsYFixed;
    Quaternion quaternion;
    float yawAngle;
};