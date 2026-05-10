#pragma once

#include <string>
#include <cstdint>

class Player {
public:
    Player(int32_t uid, const std::string& userId, int32_t rating,
           const std::string& inventoryItems, const std::string& equipmentItems,
           int32_t characterType);

    int32_t            GetUid()            const { return _uid; }
    const std::string& GetUserId()         const { return _userId; }
    int32_t            GetRating()         const { return _rating; }
    const std::string& GetInventoryItems() const { return _inventoryItems; }
    const std::string& GetEquipmentItems() const { return _equipmentItems; }
    int32_t            GetCharacterType()  const { return _characterType; }

private:
    int32_t     _uid;
    std::string _userId;
    int32_t     _rating;
    std::string _inventoryItems;
    std::string _equipmentItems;
    int32_t     _characterType;
};
