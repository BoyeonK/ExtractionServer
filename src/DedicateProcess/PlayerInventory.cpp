#include "PlayerInventory.h"
#include "ItemDataManager.h"

PlayerInventory::PlayerInventory(const std::vector<Slot>& inventorySlots, const std::vector<Slot>& equipmentSlots)
    : _inventorySlots(INVENTORY_SLOT_COUNT)
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

    LoadMagazineFromInventory(_primaryWeaponSlot, _primaryWeaponMagazineSlot);
    LoadMagazineFromInventory(_secondaryWeaponSlot, _secondaryWeaponMagazineSlot);

    UpdateFirstEmptySlotIndex();
}

void PlayerInventory::LoadMagazineFromInventory(const Slot& weaponSlot, Slot& magazineSlot) {
    if (weaponSlot.IsEmpty()) return;

    const WeaponSpec* spec = ItemDataManager::GetWeaponSpec(weaponSlot.item.blueprintId);
    if (!spec) return;

    int32_t ammoType = spec->ammoType;
    int32_t remaining = spec->maxAmmo - magazineSlot.quantity;
    if (remaining <= 0) return;

    bool changed = false;

    for (int32_t i = 0; i < INVENTORY_SLOT_COUNT && remaining > 0; ++i) {
        Slot& invSlot = _inventorySlots[i];
        if (invSlot.IsEmpty() || invSlot.item.blueprintId != static_cast<uint32_t>(ammoType))
            continue;

        int32_t take = std::min(invSlot.quantity, remaining);

        if (magazineSlot.IsEmpty()) {
            magazineSlot.item = invSlot.item;
            magazineSlot.quantity = take;
        } else {
            magazineSlot.quantity += take;
        }

        invSlot.quantity -= take;
        remaining -= take;
        changed = true;

        if (invSlot.quantity <= 0)
            invSlot.Clear();
    }

    if (changed) {
        UpdateFirstEmptySlotIndex();
        ++_inventoryVersion;
    }
}

bool PlayerInventory::UnloadMagazineToInventory(bool isPrimary) {
    Slot& magSlot = isPrimary ? _primaryWeaponMagazineSlot : _secondaryWeaponMagazineSlot;
    if (magSlot.IsEmpty()) return true;

    // 동일 blueprintId 아이템이 인벤토리에 있으면 quantity 합산
    for (int32_t i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        if (!_inventorySlots[i].IsEmpty()
            && _inventorySlots[i].item.blueprintId == magSlot.item.blueprintId) {
            _inventorySlots[i].quantity += magSlot.quantity;
            magSlot.Clear();
            return true;
        }
    }

    // 빈 슬롯에 배치, 빈 슬롯이 없으면 파기
    if (_firstEmptySlotIndex == -1) {
        magSlot.Clear();
        return true;
    }

    int32_t emptyIdx = _firstEmptySlotIndex;
    _inventorySlots[emptyIdx].item = std::move(magSlot.item);
    _inventorySlots[emptyIdx].quantity = magSlot.quantity;
    magSlot.Clear();
    UpdateFirstEmptySlotIndex();
    return true;
}

bool PlayerInventory::EquipWeaponFromInventory(int32_t inventorySlotIndex, bool isPrimary) {
    if (inventorySlotIndex < 0 || inventorySlotIndex >= INVENTORY_SLOT_COUNT) return false;

    Slot& invSlot = _inventorySlots[inventorySlotIndex];
    if (invSlot.IsEmpty()) return false;
    if (ItemDataManager::GetType(invSlot.item.blueprintId) != ItemType::WEAPON) return false;

    Slot& weaponSlot = isPrimary ? _primaryWeaponSlot : _secondaryWeaponSlot;

    // 무기 교체 전, 대응되는 탄창 슬롯을 인벤토리로 내림
    if (!UnloadMagazineToInventory(isPrimary)) return false;

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

bool PlayerInventory::UnequipWeaponToInventory(bool isPrimary, int32_t inventorySlotIndex) {
    if (inventorySlotIndex < 0 || inventorySlotIndex >= INVENTORY_SLOT_COUNT) return false;

    Slot& weaponSlot = isPrimary ? _primaryWeaponSlot : _secondaryWeaponSlot;
    if (weaponSlot.IsEmpty()) return false;

    Slot& invSlot = _inventorySlots[inventorySlotIndex];
    const Slot& otherWeaponSlot = isPrimary ? _secondaryWeaponSlot : _primaryWeaponSlot;

    // 무기 해제 전, 대응되는 탄창 슬롯을 인벤토리로 내림
    if (!UnloadMagazineToInventory(isPrimary)) return false;

    if (invSlot.IsEmpty()) {
        // [Game Rule] 맨손 금지:
        // 유저는 최소 1개의 무기를 장착해야 합니다. 다른 무기마저 없다면 장착 해제를 거부합니다.
        if (otherWeaponSlot.IsEmpty()) return false;

        invSlot.item = std::move(weaponSlot.item);
        weaponSlot.Clear();

        if (inventorySlotIndex == _firstEmptySlotIndex)
            UpdateFirstEmptySlotIndex();
    } else {
        if (ItemDataManager::GetType(invSlot.item.blueprintId) != ItemType::WEAPON) return false;

        std::swap(weaponSlot.item, invSlot.item);
    }

    ++_inventoryVersion;
    return true;
}

bool PlayerInventory::MoveInventorySlot(int32_t srcSlotIndex, int32_t dstSlotIndex) {
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
            && ItemDataManager::GetType(srcSlot.item.blueprintId) != ItemType::WEAPON
            && ItemDataManager::GetType(srcSlot.item.blueprintId) != ItemType::ARMOR) {
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

bool PlayerInventory::EquipArmorFromInventory(int32_t inventorySlotIndex) {
    if (inventorySlotIndex < 0 || inventorySlotIndex >= INVENTORY_SLOT_COUNT) return false;

    Slot& invSlot = _inventorySlots[inventorySlotIndex];
    if (invSlot.IsEmpty()) return false;
    if (ItemDataManager::GetType(invSlot.item.blueprintId) != ItemType::ARMOR) return false;

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

bool PlayerInventory::UnequipArmorToInventory(int32_t inventorySlotIndex) {
    if (inventorySlotIndex < 0 || inventorySlotIndex >= INVENTORY_SLOT_COUNT) return false;

    if (_armorSlot.IsEmpty()) return false;

    Slot& invSlot = _inventorySlots[inventorySlotIndex];

    if (invSlot.IsEmpty()) {
        invSlot.item = std::move(_armorSlot.item);
        _armorSlot.Clear();

        if (inventorySlotIndex == _firstEmptySlotIndex)
            UpdateFirstEmptySlotIndex();
    } else {
        if (ItemDataManager::GetType(invSlot.item.blueprintId) != ItemType::ARMOR) return false;

        std::swap(_armorSlot.item, invSlot.item);
    }

    ++_inventoryVersion;
    return true;
}

static void FillSlotProto(const Slot& slot, External_Game_Protocol::InventorySlot* outSlot) {
    outSlot->set_slot_index(slot.slotIndex);
    auto* item = outSlot->mutable_item();
    item->set_blueprint_id(slot.item.blueprintId);
    item->set_instance_uid(slot.item.instanceUid);
    item->set_item_type(static_cast<uint32_t>(ItemDataManager::GetType(slot.item.blueprintId)));
    item->set_quantity(slot.quantity);
}

void PlayerInventory::SerializeFullInventory(External_Game_Protocol::D2CFullInventorySync* outMsg) const {
    outMsg->set_inventory_version(_inventoryVersion);

    for (int i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        const Slot& slot = _inventorySlots[i];
        if (!slot.IsEmpty())
            FillSlotProto(slot, outMsg->add_inventory_slots());
    }

    if (!_primaryWeaponSlot.IsEmpty())
        FillSlotProto(_primaryWeaponSlot, outMsg->mutable_primary_weapon());

    if (!_secondaryWeaponSlot.IsEmpty())
        FillSlotProto(_secondaryWeaponSlot, outMsg->mutable_secondary_weapon());

    if (!_armorSlot.IsEmpty())
        FillSlotProto(_armorSlot, outMsg->mutable_armor());

    if (!_primaryWeaponMagazineSlot.IsEmpty())
        FillSlotProto(_primaryWeaponMagazineSlot, outMsg->mutable_primary_weapon_magazine());

    if (!_secondaryWeaponMagazineSlot.IsEmpty())
        FillSlotProto(_secondaryWeaponMagazineSlot, outMsg->mutable_secondary_weapon_magazine());
}

void PlayerInventory::UpdateFirstEmptySlotIndex() {
    _firstEmptySlotIndex = -1;
    for (int32_t i = 0; i < INVENTORY_SLOT_COUNT; ++i) {
        if (_inventorySlots[i].IsEmpty()) {
            _firstEmptySlotIndex = i;
            return;
        }
    }
}
