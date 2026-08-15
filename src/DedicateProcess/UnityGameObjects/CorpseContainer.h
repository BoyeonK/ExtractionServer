#pragma once

#include "Container.h"
#include "../PlayerInventory.h"

class CorpseContainer : public Container {
public:
    CorpseContainer(uint32_t objectId, const Vector3& position, float yawAngle,
                    const PlayerInventory& inventory)
    : Container(objectId, ObjectType::Corpse, true, position) {
        this->yawAngle = yawAngle;
        InitializeSlots(DEFAULT_CONTAINER_VOLUME);
        FillFrom(inventory);
    }

private:
    // 5 = 주무기 + 보조무기 + 방어구 + 탄창 2
    static_assert(static_cast<uint32_t>(PlayerInventory::INVENTORY_SLOT_COUNT) + 5 <= DEFAULT_CONTAINER_VOLUME,
                  "시신 컨테이너 용량 부족 - PlaceItem() 은 범위를 넘으면 아이템을 조용히 버린다");

    void FillFrom(const PlayerInventory& inventory) {
        uint32_t nextSlotIndex = 0;

        auto place = [this, &nextSlotIndex](const Slot& slot) {
            if (slot.IsEmpty()) return;
            PlaceItem(nextSlotIndex++, slot.item.blueprintId, slot.quantity, slot.item.instanceUid);
        };

        place(inventory.GetPrimaryWeapon());
        place(inventory.GetPrimaryWeaponMagazine());
        place(inventory.GetSecondaryWeapon());
        place(inventory.GetSecondaryWeaponMagazine());
        place(inventory.GetArmorSlot());

        for (const Slot& slot : inventory.GetInventorySlots())
            place(slot);
    }
};
