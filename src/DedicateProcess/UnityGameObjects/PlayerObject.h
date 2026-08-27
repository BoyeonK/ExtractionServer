#pragma once

#include "CombatObject.h"

class PlayerObject : public CombatObject {
public:
    // TEMP : 200.00 HP — 피격 테스트 편의를 위해 실 스펙보다 크게 잡은 값
    static constexpr int32_t DEFAULT_MAX_HP = 20000;

    PlayerObject(uint32_t objectId, float x, float y, float z, int32_t characterType,
                 const std::string& objectName)
    : CombatObject(objectId, ObjectType::Player, true, x, y, z, DEFAULT_MAX_HP)
    , _characterType(characterType), _objectName(objectName) {}

    PlayerObject(uint32_t objectId, Vector3 position, int32_t characterType,
                 const std::string& objectName)
    : CombatObject(objectId, ObjectType::Player, true, position, DEFAULT_MAX_HP)
    , _characterType(characterType), _objectName(objectName) {}

    // 타입당 고정이 아니라 플레이어마다 다르다 (userId)
    const std::string& GetObjectName() const override { return _objectName; }

    int32_t GetCharacterType() const { return _characterType; }

    void ApplyState(const External_Game_Protocol::PlayerState& state);
    void FillState(External_Game_Protocol::PlayerState* pState) const;

    void     SetWeapons(uint32_t primaryId, uint32_t secondaryId);
    uint32_t GetCurrentWeaponId()   const;
    bool     IsUsingPrimary()      const { return _isUsingPrimary; }
    uint32_t GetPrimaryWeaponId()   const { return _primaryWeaponId; }
    uint32_t GetSecondaryWeaponId() const { return _secondaryWeaponId; }
    void     SetUsingPrimary(bool isPrimary) { _isUsingPrimary = isPrimary; }

    void     SetArmor(uint32_t armorId);

    float    pitch       = 0.0f;   // 무기 조준 수직 각도
    Vector3  velocity    = {};
    uint32_t actionState = 0;      // 행동 상태 (0=NONE, 1=SHOOTING)

private:
    int32_t     _characterType;
    std::string _objectName;
    uint32_t    _primaryWeaponId   = 0;   // blueprintId
    uint32_t    _secondaryWeaponId = 0;   // blueprintId
    bool        _isUsingPrimary    = true;
};