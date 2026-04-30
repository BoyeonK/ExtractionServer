#include <stdint.h>

struct UnityGameObject {
    int32_t objectId;
    int32_t objectType;
    int32_t state;

    Vector3 front;
    Vector3 position;
};

struct Vector3 {
    float x;
    float y;
    float z;
};