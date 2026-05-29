#pragma once

#include <cstdint>
#include <vector>
#include "../Items.h"
#include "UnityGameObject.h"
#include "../ExternalProtocol/External_Protocol.pb.h"

class Container : public UnityGameObject {
public:
    static constexpr uint32_t DEFAULT_CONTAINER_VOLUME = 30;

    void SerializeOpenContainer(External_Game_Protocol::D2CResponseOpenContainer* outMsg) const;

protected:
    Container(uint32_t objectId, ObjectType objectType, bool isYFixed, float x, float y, float z)
        : UnityGameObject(objectId, objectType, isYFixed, x, y, z) {}
    Container(uint32_t objectId, ObjectType objectType, bool isYFixed, Vector3 position)
        : UnityGameObject(objectId, objectType, isYFixed, position) {}

private:
    std::vector<Slot> _inventorySlots;
    uint32_t _containerVolume = DEFAULT_CONTAINER_VOLUME;
    uint32_t _containerVersion = 0;
};