#pragma once

#include "HostileNPC.h"

class Turret : public HostileNPC {
public:
    // 300.00 HP (전투 수치는 100배 스케일)
    static constexpr int32_t DEFAULT_MAX_HP = 30000;
    // 12.00 피해
    static constexpr int32_t ATTACK_DAMAGE = 1200;

    Turret(uint32_t objectId, const Vector3& position, float yawAngle)
    : HostileNPC(objectId, ObjectType::Turret, true, position, DEFAULT_MAX_HP, ATTACK_DAMAGE) {
        this->yawAngle = yawAngle;
    }
};
