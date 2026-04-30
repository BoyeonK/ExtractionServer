#include <stdint.h>
#include "ExternalProtocol/External_Unity_Object.pb.cc"

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

    GameProtocol::UnityGameObject Serialize() {
        GameProtocol::UnityGameObject pkt;
        //pkt.set
        return pkt;
    }

    uint32_t objectId;
    uint32_t objectType;
    uint32_t state; 

    Vector3 front;
    Vector3 position;
};
