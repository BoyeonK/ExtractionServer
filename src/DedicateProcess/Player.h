#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include "PlayerInventory.h"

class Player {
public:
    Player(int32_t uid, const std::string& userId, int32_t rating,
           const std::vector<Slot>& inventorySlots, const std::vector<Slot>& equipmentSlots,
           int32_t characterType);

    int32_t                  GetUid()             const { return _uid; }
    const std::string&       GetUserId()          const { return _userId; }
    int32_t                  GetRating()          const { return _rating; }
    int32_t                  GetCharacterType()   const { return _characterType; }

    int32_t  GetObjectId()          const { return _objectId; }
    void     SetObjectId(int32_t id)        { _objectId = id; }
    uint32_t GetFireSequence()        const { return _fireSequence; }
    void     SetNextFireSequence(uint32_t seq) { _fireSequence = seq; }

    int32_t  GetInteractingContainerId()          const { return _interactingContainerId; }
    void     SetInteractingContainerId(int32_t id)       { _interactingContainerId = id; }

    PlayerInventory&       GetInventory()       { return _inventory; }
    const PlayerInventory& GetInventory() const { return _inventory; }

private:
    int32_t     _uid;
    std::string _userId;
    int32_t     _rating;
    int32_t     _characterType;
    int32_t     _objectId = -1;
    int32_t     _interactingContainerId = -1;

    uint32_t _fireSequence = 0;

    PlayerInventory _inventory;
};
