#include "Player.h"

Player::Player(int32_t uid, const std::string& userId, int32_t rating,
               const std::string& inventoryItems, const std::string& equipmentItems,
               int32_t characterType)
    : _uid(uid), _userId(userId), _rating(rating),
      _inventoryItems(inventoryItems), _equipmentItems(equipmentItems),
      _characterType(characterType)
{}
