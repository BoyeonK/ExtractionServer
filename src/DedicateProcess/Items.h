#pragma once

#include <cstdint>

enum class ItemType : uint8_t {
    NONE = 0,
    WEAPON,
    ARMOR,
    AMMO,
    MISC,
};

// 월드에 배치되는 아이템의 instanceUid 시작값. DB 의 BIGINT AUTO_INCREMENT 공간과
// 겹치지 않도록 상위 비트를 쓴다 — 클라이언트가 uid 를 키로 써도 전리품 컨테이너의
// 실 DB uid 와 충돌하지 않는다
inline constexpr uint64_t WORLD_ITEM_UID_BASE = 1ull << 62;

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
