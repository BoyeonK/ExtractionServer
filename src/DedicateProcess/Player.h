#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include "Items.h"

class Player {
public:
    static constexpr int32_t INVENTORY_SLOT_COUNT = 25;

    Player(int32_t uid, const std::string& userId, int32_t rating,
           const std::vector<Slot>& inventorySlots, const std::vector<Slot>& equipmentSlots,
           int32_t characterType);

    int32_t                  GetUid()             const { return _uid; }
    const std::string&       GetUserId()          const { return _userId; }
    int32_t                  GetRating()          const { return _rating; }
    int32_t                  GetCharacterType()   const { return _characterType; }
    const std::vector<Slot>& GetInventorySlots()  const { return _inventorySlots; }
    const Slot&              GetPrimaryWeapon()   const { return _primaryWeaponSlot; }
    const Slot&              GetSecondaryWeapon() const { return _secondaryWeaponSlot; }
    const Slot&              GetArmorSlot()       const { return _armorSlot; }

private:
    int32_t     _uid;
    std::string _userId;
    int32_t     _rating;
    int32_t     _characterType;

    Slot _primaryWeaponSlot;
    Slot _secondaryWeaponSlot;
    Slot _armorSlot;
    std::vector<Slot> _inventorySlots;
};
