#pragma once

#include <cstdint>
#include <vector>
#include "Items.h"
#include "ExternalProtocol/External_Protocol.pb.h"

class PlayerInventory {
public:
    static constexpr int32_t INVENTORY_SLOT_COUNT = 25;

    PlayerInventory(const std::vector<Slot>& inventorySlots, const std::vector<Slot>& equipmentSlots);

    const std::vector<Slot>& GetInventorySlots()  const { return _inventorySlots; }
    const Slot&              GetPrimaryWeapon()    const { return _primaryWeaponSlot; }
    const Slot&              GetSecondaryWeapon()  const { return _secondaryWeaponSlot; }
    const Slot&              GetArmorSlot()              const { return _armorSlot; }
    const Slot&              GetPrimaryWeaponMagazine()  const { return _primaryWeaponMagazineSlot; }
    const Slot&              GetSecondaryWeaponMagazine() const { return _secondaryWeaponMagazineSlot; }
    Slot&                GetPrimaryWeaponMagazineMutable()   { return _primaryWeaponMagazineSlot; }
    Slot&                GetSecondaryWeaponMagazineMutable() { return _secondaryWeaponMagazineSlot; }
    uint32_t GetInventoryVersion()    const { return _inventoryVersion; }
    int32_t  GetFirstEmptySlotIndex() const { return _firstEmptySlotIndex; }

    Slot* GetSlotMutable(int32_t slotIndex) {
        if (slotIndex < 0 || slotIndex >= INVENTORY_SLOT_COUNT) return nullptr;
        return &_inventorySlots[slotIndex];
    }
    void IncrementInventoryVersion() { ++_inventoryVersion; }
    void UpdateFirstEmptySlotIndex();

    bool EquipWeaponFromInventory(int32_t inventorySlotIndex, bool isPrimary);
    bool UnequipWeaponToInventory(bool isPrimary, int32_t inventorySlotIndex);
    bool EquipArmorFromInventory(int32_t inventorySlotIndex);
    bool UnequipArmorToInventory(int32_t inventorySlotIndex);
    bool MoveInventorySlot(int32_t srcSlotIndex, int32_t dstSlotIndex);

    // 외부 슬롯(컨테이너 등) 대상 장착/해제
    bool EquipWeaponFromSlot(Slot& srcSlot, bool isPrimary, uint32_t& outDenyReason);
    bool UnequipWeaponToSlot(Slot& dstSlot, bool isPrimary, uint32_t& outDenyReason);
    bool EquipArmorFromSlot(Slot& srcSlot, uint32_t& outDenyReason);
    bool UnequipArmorToSlot(Slot& dstSlot, uint32_t& outDenyReason);

    void SerializeFullInventory(External_Game_Protocol::D2CFullInventorySync* outMsg) const;

    // 소지품 전부 소실 (인벤토리 + 장비 + 탄창). 사망 이탈 처리에서 사용한다.
    void Clear();

private:
    void LoadMagazineFromInventory(const Slot& weaponSlot, Slot& magazineSlot);
    bool UnloadMagazineToInventory(bool isPrimary);

    Slot _primaryWeaponSlot;
    Slot _secondaryWeaponSlot;
    Slot _armorSlot;
    Slot _primaryWeaponMagazineSlot;
    Slot _secondaryWeaponMagazineSlot;
    std::vector<Slot> _inventorySlots;

    uint32_t _inventoryVersion    = 0;
    int32_t  _firstEmptySlotIndex = -1;
};
