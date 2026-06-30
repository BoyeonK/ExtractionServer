#pragma once

#include "Container.h"

class TestItemBox : public Container {
public:
    TestItemBox(uint32_t objectId, float x, float y, float z)
    : Container(objectId, ObjectType::TestItemBox, true, x, y, z) { InitializeTestItems(); }

    TestItemBox(uint32_t objectId, Vector3 position)
    : Container(objectId, ObjectType::TestItemBox, true, position) { InitializeTestItems(); }

private:
    void InitializeTestItems() {
        InitializeSlots(DEFAULT_CONTAINER_VOLUME);
        PlaceItem(0, 5, 60, 1001);  // 5.56mm x60
        PlaceItem(1, 6, 30, 1002);  // 7.62mm x30
        PlaceItem(2, 4,  1, 1003);  // 경량 조끼 x1
    }
};