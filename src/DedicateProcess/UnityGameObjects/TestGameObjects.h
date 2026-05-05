#pragma once

#include "UnityGameObject.h"

class TestItemBox : public UnityGameObject {
public:
    TestItemBox(uint32_t objectId, float x, float y, float z) 
    : UnityGameObject(objectId, ObjectType::TestItemBox, true, x, y, z) {}

    TestItemBox(uint32_t objectId, Vector3 position) 
    : UnityGameObject(objectId, ObjectType::TestItemBox, true, position) {}

    
};