#pragma once

#include "Container.h"

class TestItemBox : public Container {
public:
    TestItemBox(uint32_t objectId, float x, float y, float z)
    : Container(objectId, ObjectType::TestItemBox, true, x, y, z) {}

    TestItemBox(uint32_t objectId, Vector3 position)
    : Container(objectId, ObjectType::TestItemBox, true, position) {}


};