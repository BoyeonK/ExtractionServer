#pragma once

#include "UnityGameObject.h"
#include <cstdint>

class GameRoom;

// 값 단위: 100배 스케일 (예: 100.00 HP → 10000)
class CombatObject : public UnityGameObject {
public:
    static constexpr uint32_t NO_ATTACKER = 0xFFFFFFFF;

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

    // 실드 0 은 의도된 설계 — 방어구를 스왑해 실드 파괴 리스크를 회피하는 플레이를 막는다.
    // 착용 후 회복 수단은 RegenShield() 뿐이다
    void SetShield(int32_t maxShield, int32_t damageReductionRate, int32_t regenPerSec) {
        _maxShield            = maxShield;
        _currentShield        = 0;
        _damageReductionRate  = damageReductionRate;
        _shieldRegenPerSec    = regenPerSec;
        _shieldRegenAccum     = 0;
    }

    void ChargeShield(int32_t amount) {
        _currentShield += amount;
        if (_currentShield > _maxShield)
            _currentShield = _maxShield;
    }

    // 누적 단위는 (실드 × ms). 1000이 모일 때마다 1을 지급해 틱으로 나누어떨어지지 않는
    // 회복률에서도 오차가 쌓이지 않는다
    void RegenShield(uint32_t elapsedMs) {
        if (_shieldRegenPerSec <= 0 || _currentShield >= _maxShield || !IsAlive()) {
            _shieldRegenAccum = 0;
            return;
        }

        _shieldRegenAccum += _shieldRegenPerSec * static_cast<int32_t>(elapsedMs);

        int32_t gain = _shieldRegenAccum / 1000;
        if (gain <= 0) return;

        _shieldRegenAccum -= gain * 1000;
        ChargeShield(gain);
    }

    void TakeDamage(int32_t damage, uint32_t attackerObjectId) {
        _lastAttackerId = attackerObjectId;

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

    virtual void OnDeath() { _deathPending = true; }
    virtual void OnDamageApplied() {}

    // 회수 직전 룸이 호출한다. 흔적(전리품 등)을 남길 오브젝트만 override
    virtual void OnDeathResolved(GameRoom& room) {}

    bool IsDeathPending() const { return _deathPending; }

    uint32_t GetLastAttackerId() const { return _lastAttackerId; }

protected:
    int32_t  _maxHp;
    int32_t  _currentHp;
    bool     _deathPending   = false;
    uint32_t _lastAttackerId = NO_ATTACKER;

    int32_t _maxShield           = 0;
    int32_t _currentShield       = 0;
    int32_t _damageReductionRate = 0;  // 만분율 (5000 = 50%)
    int32_t _shieldRegenPerSec   = 0;
    int32_t _shieldRegenAccum    = 0;
};
