#include "Player.h"

Player::Player(int32_t uid, const std::string& userId, int32_t rating,
               const std::vector<Slot>& inventorySlots, const std::vector<Slot>& equipmentSlots,
               int32_t characterType)
    : _uid(uid), _userId(userId), _rating(rating),
      _characterType(characterType),
      _inventorySlots(INVENTORY_SLOT_COUNT)
{
    for (const auto& slot : inventorySlots) {
        if (slot.slotIndex >= 0 && slot.slotIndex < INVENTORY_SLOT_COUNT)
            _inventorySlots[slot.slotIndex] = slot;
    }

    // equipmentSlotId: 0=주무기, 1=보조무기, 2=방어구 (match.js LOADOUT 기준)
    for (const auto& slot : equipmentSlots) {
        if      (slot.slotIndex == 0) _primaryWeaponSlot   = slot;
        else if (slot.slotIndex == 1) _secondaryWeaponSlot = slot;
        else if (slot.slotIndex == 2) _armorSlot           = slot;
    }

    UpdateFirstEmptySlotIndex();
}

void Player::UpdateFirstEmptySlotIndex() {
    _firstEmptySlotIndex = -1;
    for (int32_t i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        if (_inventorySlots[i].IsEmpty()) {
            _firstEmptySlotIndex = i;
            return;
        }
    }
}
