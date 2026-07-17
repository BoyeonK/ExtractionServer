#pragma once

#include "UnityGameObject.h"
#include <cstdint>

// 전투 가능한 오브젝트의 공통 베이스 클래스
// HP 및 전투 관련 공통 상태를 담당한다.
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

    // ── HP ──
    int32_t GetMaxHp()     const { return _maxHp; }
    int32_t GetCurrentHp() const { return _currentHp; }
    bool    IsAlive()      const { return _currentHp > 0; }

    // ── Shield (AP) ──
    int32_t GetMaxShield()          const { return _maxShield; }
    int32_t GetCurrentShield()      const { return _currentShield; }
    int32_t GetDamageReductionRate() const { return _damageReductionRate; }
    int32_t GetShieldRegenPerSec()  const { return _shieldRegenPerSec; }

    void SetShield(int32_t maxShield, int32_t damageReductionRate, int32_t regenPerSec) {
        _maxShield            = maxShield;
        _currentShield        = maxShield;
        _damageReductionRate  = damageReductionRate;
        _shieldRegenPerSec    = regenPerSec;
    }

private:
    int32_t _maxHp;
    int32_t _currentHp;

    int32_t _maxShield           = 0;
    int32_t _currentShield       = 0;
    int32_t _damageReductionRate = 0;  // 만분율 (5000 = 50%)
    int32_t _shieldRegenPerSec   = 0;
};
