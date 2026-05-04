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

    Vector3 front;
    Vector3 position;
};