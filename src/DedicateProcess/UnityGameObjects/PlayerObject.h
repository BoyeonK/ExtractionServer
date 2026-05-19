#pragma once

#include "UnityGameObject.h"

class PlayerObject : public UnityGameObject {
public:
    PlayerObject(uint32_t objectId, float x, float y, float z, int32_t characterType)
    : UnityGameObject(objectId, ObjectType::Player, true, x, y, z)
    , _characterType(characterType) {}

    PlayerObject(uint32_t objectId, Vector3 position, int32_t characterType)
    : UnityGameObject(objectId, ObjectType::Player, true, position)
    , _characterType(characterType) {}

    int32_t GetCharacterType() const { return _characterType; }

    void ApplyState(const External_Game_Protocol::PlayerState& state);
    void FillState(External_Game_Protocol::PlayerState* pState) const;

    // 무기 상태
    void     SetWeapons(uint32_t primaryId, uint32_t secondaryId);
    uint32_t GetCurrentWeaponId()   const;
    uint32_t GetPrimaryWeaponId()   const { return _primaryWeaponId; }
    uint32_t GetSecondaryWeaponId() const { return _secondaryWeaponId; }
    void     SwitchWeapon();

    float   pitch    = 0.0f;   // 무기 조준 수직 각도
    Vector3 velocity = {};     // 보간용 속도 벡터

private:
    int32_t  _characterType;
    uint32_t _primaryWeaponId   = 0;   // blueprintId
    uint32_t _secondaryWeaponId = 0;   // blueprintId
    bool     _isUsingPrimary    = true;
};