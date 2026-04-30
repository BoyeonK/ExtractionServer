#include <stdint.h>
#include "ExternalProtocol/External_Unity_Object.pb.h"

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default; // 기본 생성자
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct UnityGameObject {
private:
    UnityGameObject() = delete;

public:
    UnityGameObject(uint32_t objectId, uint32_t objectType) : 
        objectId(objectId), 
        objectType(objectType), 
        state(0)
    {}

    void Serialize(External_Game_Protocol::UnityGameObject* pPkt) const {
        if (pPkt == nullptr) return;

        pPkt->set_object_id(objectId);
        pPkt->set_object_type(objectType);

        External_Game_Protocol::Vector3* pPos = pPkt->mutable_position();
        pPos->set_x(position.x);
        pPos->set_y(position.y);
        pPos->set_z(position.z);
    }

    External_Game_Protocol::UnityGameObject Serialize() const {
        External_Game_Protocol::UnityGameObject pkt;
        Serialize(&pkt);
        return pkt;
    }

    uint32_t objectId;
    uint32_t objectType;
    uint32_t state; 

    Vector3 front;
    Vector3 position;
};
