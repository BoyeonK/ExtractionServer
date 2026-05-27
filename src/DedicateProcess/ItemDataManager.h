#pragma once

#include <string>
#include <string_view>
#include "absl/container/flat_hash_map.h"
#include "Items.h"

struct WeaponSpec {
    int32_t baseDamage;
    int32_t RPM;
    int32_t maxAmmo;
    int32_t MOA;
    int32_t vRecoilMin;
    int32_t vRecoilMax;
    int32_t spreadBase;
    int32_t spreadMax;
    int32_t spreadIncreasePerShot;
    int32_t spreadRecoveryRate;
    int32_t ammoType;
};

struct ArmorSpec {
    int32_t maxShieldPoint;
    int32_t DamageReductionRate;
    int32_t regenerationPerSecond;
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
        return (it != _nameMap.end()) ? std::string_view(it->second) : "알 수 없는 아이템";
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

    inline static const absl::flat_hash_map<uint32_t, WeaponSpec> _weaponSpecs = {
        { 1, { 4800, 600, 30, 0, 200, 220, 50, 1000, 120, 40, 6 } },
        { 2, { 4000, 700, 30, 0, 150, 165, 50, 1000, 100, 40, 5 } },
        { 3, { 4200, 650, 30, 0, 200, 220, 50, 1000, 120, 40, 5 } },
    };

    inline static const absl::flat_hash_map<uint32_t, ArmorSpec> _armorSpecs = {
        { 4, { 10000, 5000, 100 } },
    };
};
