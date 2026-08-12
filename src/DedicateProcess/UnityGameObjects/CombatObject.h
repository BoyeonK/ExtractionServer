#pragma once

#include "UnityGameObject.h"
#include <cstdint>

// 값 단위: 100배 스케일 (예: 100.00 HP → 10000)
class CombatObject : public UnityGameObject {
public:
    CombatObject(uint32_t objectId, ObjectType objectType, bool isYFixed,
                 float x, float y, float z, int32_t maxHp)
        : UnityGameObject(objectId, objectType, isYFixed, x, y, z)
        , _maxHp(maxHp), _currentHp(maxHp) {}

    CombatObject(uint32_t objectId, ObjectType objectType, bool isYFixed,
                 Vector3 position, int32_t maxHp)
        : UnityGameObject(objectId, objectType, isYFixed, position)
        , _maxHp(maxHp), _currentHp(maxHp) {}

    int32_t GetMaxHp()     const { return _maxHp; }
    int32_t GetCurrentHp() const { return _currentHp; }
    bool    IsAlive()      const { return _currentHp > 0; }

    int32_t GetMaxShield()          const { return _maxShield; }
    int32_t GetCurrentShield()      const { return _currentShield; }
    int32_t GetDamageReductionRate() const { return _damageReductionRate; }
    int32_t GetShieldRegenPerSec()  const { return _shieldRegenPerSec; }

    void SetShield(int32_t maxShield, int32_t damageReductionRate, int32_t regenPerSec) {
        _maxShield            = maxShield;
        _currentShield        = 0;
        _damageReductionRate  = damageReductionRate;
        _shieldRegenPerSec    = regenPerSec;
    }

    void ChargeShield(int32_t amount) {
        _currentShield += amount;
        if (_currentShield > _maxShield)
            _currentShield = _maxShield;
    }

    void TakeDamage(int32_t damage) {
        bool penetrated = false;
        int32_t hpDamage = 0;

        _currentShield -= damage;
        if (_currentShield <= 0) {
            hpDamage = -_currentShield; // 관통된 초과 데미지
            _currentShield = 0;
            penetrated = true;
        }

        if (penetrated) {
            _currentHp -= hpDamage;
        } else {
            int32_t reduced = damage * (10000 - _damageReductionRate) / 10000;
            _currentHp -= reduced;
        }

        if (_currentHp <= 0) {
            _currentHp = 0;
            OnDeath();
        } else {
            OnDamageApplied();
        }
    }

    virtual void OnDeath() {}
    virtual void OnDamageApplied() {}

protected:
    int32_t _maxHp;
    int32_t _currentHp;

    int32_t _maxShield           = 0;
    int32_t _currentShield       = 0;
    int32_t _damageReductionRate = 0;  // 만분율 (5000 = 50%)
    int32_t _shieldRegenPerSec   = 0;
};
