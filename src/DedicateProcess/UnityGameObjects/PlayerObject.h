#pragma once

#include "UnityGameObject.h"

class PlayerObject : public UnityGameObject {
public:
    PlayerObject(uint32_t objectId, float x, float y, float z) 
    : UnityGameObject(objectId, ObjectType::Player, true, x, y, z) {}

    PlayerObject(uint32_t objectId, Vector3 position) 
    : UnityGameObject(objectId, ObjectType::Player, true, position) {}

    
};