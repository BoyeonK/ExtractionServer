#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include "Items.h"
#include "ExternalProtocol/External_Protocol.pb.h"

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

    int32_t  GetObjectId()          const { return _objectId; }
    void     SetObjectId(int32_t id)        { _objectId = id; }
    uint32_t GetInventoryVersion()    const { return _inventoryVersion; }
    uint32_t GetFireSequence()        const { return _fireSequence; }
    int32_t  GetFirstEmptySlotIndex() const { return _firstEmptySlotIndex; }

    bool EquipWeaponFromInventory(int32_t inventorySlotIndex, bool isPrimary);
    bool UnequipWeaponToInventory(bool isPrimary, int32_t inventorySlotIndex);
    bool UnequipArmorToInventory(int32_t inventorySlotIndex);
    bool EquipArmorFromInventory(int32_t inventorySlotIndex);
    bool MoveInventorySlot(int32_t srcSlotIndex, int32_t dstSlotIndex);

    void SerializeFullInventory(External_Game_Protocol::D2CFullInventorySync* outMsg) const;

private:
    void UpdateFirstEmptySlotIndex();

private:
    int32_t     _uid;
    std::string _userId;
    int32_t     _rating;
    int32_t     _characterType;
    int32_t     _objectId = -1;

    Slot _primaryWeaponSlot;
    Slot _secondaryWeaponSlot;
    Slot _armorSlot;
    std::vector<Slot> _inventorySlots;

    uint32_t _inventoryVersion    = 0;
    uint32_t _fireSequence        = 0;
    int32_t  _firstEmptySlotIndex = -1;
};
