#pragma once

#include <string>
#include <string_view>
#include "absl/container/flat_hash_map.h"
#include "Items.h"

struct WeaponSpec {
    uint32_t baseDamage;
    uint32_t RPM;
    uint32_t MOA;
    uint32_t vRecoilMin;
    uint32_t vRecoilMax;
    uint32_t hRecoilMax;

    uint32_t spreadBase;
    uint32_t spreadMax;
    uint32_t spreadIncreasePerShot;
    uint32_t spreadRecoveryRate;

    // 의존성을 가지는 AMMO의 blueprint_id
    uint32_t ammoType;
};

struct ArmorSpec {
    uint32_t maxShieldPoint;
    uint32_t DamageReductionRate;
    uint32_t regenerationPerSecond;
};

class ItemDataManager {
public:
    ItemDataManager() = delete;
    ItemDataManager(const ItemDataManager&) = delete;
    ItemDataManager& operator=(const ItemDataManager&) = delete;

    static ItemType GetType(int32_t id) {
        auto it = _typeMap.find(id);
        return (it != _typeMap.end()) ? it->second : ItemType::NONE;
    }

    static std::string_view GetName(int32_t id) {
        auto it = _nameMap.find(id);
        return (it != _nameMap.end()) ? it->second : "버그 발생";
    }

    static const WeaponSpec* GetWeaponSpec(uint32_t itemId) {
        auto it = _weaponSpecs.find(itemId);
        if (it != _weaponSpecs.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    static const ArmorSpec* GetArmorSpec(uint32_t itemId) {
        auto it = _armorSpecs.find(itemId);
        if (it != _armorSpecs.end()) {
            return &(it->second);
        }
        return nullptr;
    }

private:
    inline static const absl::flat_hash_map<int32_t, ItemType> _typeMap = {
        { 1, ItemType::WEAPON },
        { 2, ItemType::WEAPON },
        { 3, ItemType::WEAPON },
        { 4, ItemType::ARMOR },
        { 5, ItemType::AMMO },
        { 6, ItemType::AMMO },
        { 7, ItemType::MISC },
    };

    inline static const absl::flat_hash_map<int32_t, std::string> _nameMap = {
        { 1, "AK-47" },
        { 2, "M4A1" },
        { 3, "M16" },
        { 4, "경량 조끼" },
        { 5, "5.56mm" },
        { 6, "7.62mm" },
        { 7, "돌맹이" },
    };

    // Python 스크립트가 DB의 weapon_specs, armor_specs 테이블에서 자동생성
    inline static const absl::flat_hash_map<uint32_t, WeaponSpec> _weaponSpecs = {};
    inline static const absl::flat_hash_map<uint32_t, ArmorSpec> _armorSpecs = {};
};
