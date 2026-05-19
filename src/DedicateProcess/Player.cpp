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

bool Player::EquipWeaponFromInventory(int32_t inventorySlotIndex, bool isPrimary) {
    if (inventorySlotIndex < 0 || inventorySlotIndex >= INVENTORY_SLOT_COUNT) return false;

    Slot& invSlot = _inventorySlots[inventorySlotIndex];
    if (invSlot.IsEmpty()) return false;
    if (invSlot.item.itemType != ItemType::WEAPON) return false;

    Slot& weaponSlot = isPrimary ? _primaryWeaponSlot : _secondaryWeaponSlot;

    if (weaponSlot.IsEmpty()) {
        weaponSlot.item = std::move(invSlot.item); 
        invSlot.Clear();
        
        if (inventorySlotIndex < _firstEmptySlotIndex || _firstEmptySlotIndex == -1)
            _firstEmptySlotIndex = inventorySlotIndex;
    } else {
        std::swap(weaponSlot.item, invSlot.item);
    }

    ++_inventoryVersion;
    return true;
}

bool Player::UnequipWeaponToInventory(bool isPrimary, int32_t inventorySlotIndex) {
    if (inventorySlotIndex < 0 || inventorySlotIndex >= INVENTORY_SLOT_COUNT) return false;

    Slot& weaponSlot = isPrimary ? _primaryWeaponSlot : _secondaryWeaponSlot;
    if (weaponSlot.IsEmpty()) return false;

    Slot& invSlot = _inventorySlots[inventorySlotIndex];
    const Slot& otherWeaponSlot = isPrimary ? _secondaryWeaponSlot : _primaryWeaponSlot;

    if (invSlot.IsEmpty()) {
        // [Game Rule] 맨손 금지: 
        // 유저는 최소 1개의 무기를 장착해야 합니다. 다른 무기마저 없다면 장착 해제를 거부합니다.
        if (otherWeaponSlot.IsEmpty()) return false;

        invSlot.item = std::move(weaponSlot.item);
        weaponSlot.Clear();

        if (inventorySlotIndex == _firstEmptySlotIndex)
            UpdateFirstEmptySlotIndex();
    } else {
        if (invSlot.item.itemType != ItemType::WEAPON) return false;

        std::swap(weaponSlot.item, invSlot.item);
    }

    ++_inventoryVersion;
    return true;
}

bool Player::MoveInventorySlot(int32_t srcSlotIndex, int32_t dstSlotIndex) {
    if (srcSlotIndex < 0 || srcSlotIndex >= INVENTORY_SLOT_COUNT) return false;
    if (dstSlotIndex < 0 || dstSlotIndex >= INVENTORY_SLOT_COUNT) return false;
    if (srcSlotIndex == dstSlotIndex) return false;

    Slot& srcSlot = _inventorySlots[srcSlotIndex];
    if (srcSlot.IsEmpty()) return false;

    Slot& dstSlot = _inventorySlots[dstSlotIndex];

    if (dstSlot.IsEmpty()) {
        dstSlot.item = std::move(srcSlot.item);
        dstSlot.quantity = srcSlot.quantity;
        srcSlot.Clear();

        if (srcSlotIndex < _firstEmptySlotIndex || _firstEmptySlotIndex == -1)
            _firstEmptySlotIndex = srcSlotIndex;
        else if (dstSlotIndex == _firstEmptySlotIndex)
            UpdateFirstEmptySlotIndex();
    } else {
        if (srcSlot.item.blueprintId == dstSlot.item.blueprintId
            && srcSlot.item.itemType != ItemType::WEAPON
            && srcSlot.item.itemType != ItemType::ARMOR) {
            dstSlot.quantity += srcSlot.quantity;
            srcSlot.Clear();

            if (srcSlotIndex < _firstEmptySlotIndex || _firstEmptySlotIndex == -1)
                _firstEmptySlotIndex = srcSlotIndex;
        } else {
            std::swap(srcSlot.item, dstSlot.item);
            std::swap(srcSlot.quantity, dstSlot.quantity);
        }
    }

    ++_inventoryVersion;
    return true;
}

bool Player::EquipArmorFromInventory(int32_t inventorySlotIndex) {
    if (inventorySlotIndex < 0 || inventorySlotIndex >= INVENTORY_SLOT_COUNT) return false;

    Slot& invSlot = _inventorySlots[inventorySlotIndex];
    if (invSlot.IsEmpty()) return false;
    if (invSlot.item.itemType != ItemType::ARMOR) return false;

    if (_armorSlot.IsEmpty()) {
        _armorSlot.item = std::move(invSlot.item);
        invSlot.Clear();

        if (inventorySlotIndex < _firstEmptySlotIndex || _firstEmptySlotIndex == -1)
            _firstEmptySlotIndex = inventorySlotIndex;
    } else {
        std::swap(_armorSlot.item, invSlot.item);
    }

    ++_inventoryVersion;
    return true;
}

bool Player::UnequipArmorToInventory(int32_t inventorySlotIndex) {
    if (inventorySlotIndex < 0 || inventorySlotIndex >= INVENTORY_SLOT_COUNT) return false;

    if (_armorSlot.IsEmpty()) return false;

    Slot& invSlot = _inventorySlots[inventorySlotIndex];

    if (invSlot.IsEmpty()) {
        invSlot.item = std::move(_armorSlot.item);
        _armorSlot.Clear();

        if (inventorySlotIndex == _firstEmptySlotIndex)
            UpdateFirstEmptySlotIndex();
    } else {
        if (invSlot.item.itemType != ItemType::ARMOR) return false;

        std::swap(_armorSlot.item, invSlot.item);
    }

    ++_inventoryVersion;
    return true;
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
