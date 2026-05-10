#pragma once

#include "absl/container/flat_hash_map.h"

struct WeaponSpec {
    int32_t baseDamage;
    int32_t RPM;
    int32_t ammoType;
    int32_t MOA;
    int32_t vRecoilMin;
    int32_t vRecoilMax;
    int32_t hRecoilMin;
    int32_t hRecoilMax;
};

struct ArmorSpec {
    int32_t maxShieldPoint;
    int32_t DamageReductionRate;
    int32_t regenerationPerSecond;
};

class ItemDataManager {
public:
    void Init() {
        // TODO : 하드코딩으로 추가 (추후에 DB긁어서 코드 추가하는 방식으로 바꿀 예정)
    }

    const WeaponSpec* GetWeaponSpec(uint32_t itemId) const {
        auto it = _weaponSpecs.find(itemId);
        if (it != _weaponSpecs.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    const ArmorSpec* GetArmorSpec(uint32_t itemId) const {
        auto it = _armorSpecs.find(itemId);
        if (it != _armorSpecs.end()) {
            return &(it->second);
        }
        return nullptr;
    }

private:
    absl::flat_hash_map<uint32_t, WeaponSpec> _weaponSpecs;
    absl::flat_hash_map<uint32_t, ArmorSpec> _armorSpecs;
};