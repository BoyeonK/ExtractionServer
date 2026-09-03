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
    void SerializeRecentContainerInfo(External_Game_Protocol::D2CResponseRecentContainerInfo* outMsg) const;

    Slot* GetSlotMutable(uint32_t slotIndex) {
        if (slotIndex >= _inventorySlots.size()) return nullptr;
        return &_inventorySlots[slotIndex];
    }

    uint32_t GetContainerVersion() const { return _containerVersion; }
    void IncrementContainerVersion() { ++_containerVersion; }

    // 룸 생성자에서만 부를 것 — 열어본 클라이언트가 없는 시점을 전제로 _containerVersion 을
    // 올리지 않는다. 낮은 인덱스부터 순서대로 채운다
    bool PlaceInitialLoot(uint32_t blueprintId, int32_t quantity, uint64_t instanceUid) {
        if (_nextLootSlot >= _inventorySlots.size()) return false;
        if (!PlaceItem(_nextLootSlot, blueprintId, quantity, instanceUid)) return false;

        ++_nextLootSlot;
        return true;
    }

    // 점유자 없음. objectId 는 룸 안에서 재사용되지 않으므로 낡은 값이 남의 것을 가리키지 않는다
    static constexpr uint32_t NO_INTERACTING_PLAYER = 0xFFFFFFFF;

    uint32_t GetInteractingPlayerId() const { return _interactingPlayerId; }
    void SetInteractingPlayerId(uint32_t playerObjectId) { _interactingPlayerId = playerObjectId; }

protected:
    Container(uint32_t objectId, ObjectType objectType, bool isYFixed, float x, float y, float z)
        : UnityGameObject(objectId, objectType, isYFixed, x, y, z) {}
    Container(uint32_t objectId, ObjectType objectType, bool isYFixed, Vector3 position)
        : UnityGameObject(objectId, objectType, isYFixed, position) {}

    void InitializeSlots(uint32_t slotCount) {
        _inventorySlots.resize(slotCount);
        for (uint32_t i = 0; i < slotCount; ++i)
            _inventorySlots[i].slotIndex = static_cast<int32_t>(i);

        _containerVolume = slotCount;
    }

    bool PlaceItem(uint32_t slotIndex, uint32_t blueprintId, int32_t quantity, uint64_t instanceUid) {
        if (slotIndex >= _inventorySlots.size() || quantity <= 0) return false;
        Slot& slot = _inventorySlots[slotIndex];
        slot.item.blueprintId = blueprintId;
        slot.item.instanceUid = instanceUid;
        slot.quantity = quantity;
        return true;
    }

private:
    std::vector<Slot> _inventorySlots;
    uint32_t _nextLootSlot = 0;
    uint32_t _containerVolume = DEFAULT_CONTAINER_VOLUME;
    uint32_t _containerVersion = 0;
    uint32_t _interactingPlayerId = NO_INTERACTING_PLAYER;
};