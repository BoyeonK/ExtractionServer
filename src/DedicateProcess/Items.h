#pragma once

#include <cstdint>

enum class ItemType : uint8_t {
    NONE = 0,
    WEAPON,
    EQUIPMENT,
    AMMO,
    MISC,
};

struct Item {
    uint64_t instanceUid = 0; 
    uint32_t blueprintId = 0; 
    
    //int32_t dynamicValue = 0; 추후에 내구도 같은 정보가 추가될 경우 필요.
};

struct Slot {
    Item item;
    
    int32_t slotIndex = -1;
    int32_t quantity = 0;

    bool IsEmpty() const { 
        return quantity <= 0 && item.blueprintId == 0; 
    }

    void Clear() {
        item = Item{};
        quantity = 0;
    }
};
