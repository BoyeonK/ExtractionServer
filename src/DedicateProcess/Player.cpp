#include "Player.h"

Player::Player(int32_t uid, const std::string& userId, int32_t rating,
               const std::vector<Slot>& inventorySlots, const std::vector<Slot>& equipmentSlots,
               int32_t characterType)
    : _uid(uid), _userId(userId), _rating(rating),
      _characterType(characterType),
      _inventory(inventorySlots, equipmentSlots)
{
}
