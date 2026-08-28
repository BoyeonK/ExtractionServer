import mysql.connector

# DB 연결
conn = mysql.connector.connect(
    host="localhost",
    port=3306,
    user="root", 
    password="매우 치명적인 실수, 비밀번호를 다 바꿔야 되는 부분", 
    database="game_db"
)

cursor = None
try:
    cursor = conn.cursor(dictionary=True)

    # items 테이블
    cursor.execute("SELECT item_id, item_name, item_type, description FROM items")
    items = cursor.fetchall()

    # weapon_specs 테이블 (C#, C++ 양쪽에서 동일하게 사용할 컬럼들)
    cursor.execute("""
        SELECT item_id, base_damage, rpm, ammo_max,
               moa, v_recoil_min, v_recoil_max, h_recoil_max,
               spread_base, spread_max, spread_increase_per_shot,
               spread_recovery_rate, ammo_type
        FROM weapon_specs
    """)
    weapon_specs = cursor.fetchall()

    # armor_specs 테이블
    cursor.execute("SELECT item_id, max_shield_point, damage_reduction_rate, regeneration_per_second FROM armor_specs")
    armor_specs = cursor.fetchall()

finally:
    if cursor:
        cursor.close()
    conn.close()

# ==========================================
# 1. C# (ItemDBHelper.cs) 생성
# ==========================================
CS_TYPE_MAP = {
    "WEAPON": "Weapon",
    "ARMOR":  "Armor",
    "AMMO":   "Ammo",
    "MISC":   "Misc",
}

item_entries_cs = "\n".join(
    f'        {{ {r["item_id"]}, new ItemDB {{ Id = {r["item_id"]}, Type = ItemType.{CS_TYPE_MAP.get(r["item_type"], "Misc")}, Name = "{r["item_name"]}", Description = "{r["description"] or ""}" }} }},'
    for r in items
)

weapon_entries_cs = "\n".join(
    f'        {{ {r["item_id"]}, new WeaponSpec {{ BaseDamage = {r["base_damage"]}, Rpm = {r["rpm"]}, MaxAmmo = {r["ammo_max"]}, Moa = {r["moa"]}, VRecoilMin = {r["v_recoil_min"]}, VRecoilMax = {r["v_recoil_max"]}, HRecoilMax = {r["h_recoil_max"]}, SpreadBase = {r["spread_base"]}, SpreadMax = {r["spread_max"]}, SpreadIncreasePerShot = {r["spread_increase_per_shot"]}, SpreadRecoveryRate = {r["spread_recovery_rate"]}, AmmoType = {r["ammo_type"]} }} }},'
    for r in weapon_specs
)

armor_entries_cs = "\n".join(
    f'        {{ {r["item_id"]}, new ArmorSpec {{ MaxShieldPoint = {r["max_shield_point"]}, DamageReductionRate = {r["damage_reduction_rate"]}, RegenerationPerSecond = {r["regeneration_per_second"]} }} }},'
    for r in armor_specs
)

cs_code = f"""using System.Collections.Generic;

public enum ItemType {{ None, Weapon, Armor, Ammo, Misc }}

public class ItemDB {{
    public int Id {{ get; set; }}
    public ItemType Type {{ get; set; }}
    public string Name {{ get; set; }}
    public string Description {{ get; set; }}
}}

public struct WeaponSpec {{
    public int BaseDamage {{ get; set; }}
    public int Rpm {{ get; set; }}
    public int MaxAmmo {{ get; set; }}
    public int Moa {{ get; set; }}
    public int VRecoilMin {{ get; set; }}
    public int VRecoilMax {{ get; set; }}
    public int HRecoilMax {{ get; set; }}
    public int SpreadBase {{ get; set; }}
    public int SpreadMax {{ get; set; }}
    public int SpreadIncreasePerShot {{ get; set; }}
    public int SpreadRecoveryRate {{ get; set; }}
    public int AmmoType {{ get; set; }}
}}

public struct ArmorSpec {{
    public int MaxShieldPoint {{ get; set; }}
    public int DamageReductionRate {{ get; set; }}
    public int RegenerationPerSecond {{ get; set; }}
}}

public static class ItemDBHelper {{
    private static readonly Dictionary<int, ItemDB> _items = new() {{
{item_entries_cs}
    }};

    private static readonly Dictionary<int, WeaponSpec> _weaponSpecs = new() {{
{weapon_entries_cs}
    }};

    private static readonly Dictionary<int, ArmorSpec> _armorSpecs = new() {{
{armor_entries_cs}
    }};

    public static ItemDB GetItem(int id) =>
        _items.TryGetValue(id, out var data) ? data : null;

    public static bool TryGetWeaponSpec(int id, out WeaponSpec spec) =>
        _weaponSpecs.TryGetValue(id, out spec);

    public static bool TryGetArmorSpec(int id, out ArmorSpec spec) =>
        _armorSpecs.TryGetValue(id, out spec);

    public static ItemType GetType(int id) => GetItem(id)?.Type ?? ItemType.Misc;
    public static string GetName(int id) => GetItem(id)?.Name ?? "알 수 없는 아이템";
}}
"""

with open("ItemDBHelper.cs", "w", encoding="utf-8") as f:
    f.write(cs_code)

print(f"{len(items)}개 아이템 → ItemDBHelper.cs 생성 완료")


# ==========================================
# 2. C++ (ItemDataManager.h) 생성
# ==========================================
CPP_TYPE_MAP = {
    "WEAPON": "WEAPON",
    "ARMOR":  "ARMOR",
    "AMMO":   "AMMO",
    "MISC":   "MISC",
}

type_entries_cpp = "\n".join(f"        {{ {r['item_id']}, ItemType::{CPP_TYPE_MAP.get(r['item_type'], 'MISC')} }}," for r in items)
name_entries_cpp = "\n".join(f"        {{ {r['item_id']}, \"{r['item_name']}\" }}," for r in items)

# C++ 구조체 초기화 리스트 생성 (C# 구조체 순서와 완벽히 일치시킴)
weapon_entries_cpp = "\n".join(
    f"        {{ {r['item_id']}, {{ {r['base_damage']}, {r['rpm']}, {r['ammo_max']}, {r['moa']}, {r['v_recoil_min']}, {r['v_recoil_max']}, {r['h_recoil_max']}, {r['spread_base']}, {r['spread_max']}, {r['spread_increase_per_shot']}, {r['spread_recovery_rate']}, {r['ammo_type']} }} }},"
    for r in weapon_specs
)

armor_entries_cpp = "\n".join(
    f"        {{ {r['item_id']}, {{ {r['max_shield_point']}, {r['damage_reduction_rate']}, {r['regeneration_per_second']} }} }},"
    for r in armor_specs
)

cpp_code = f"""#pragma once

#include <string>
#include <string_view>
#include "absl/container/flat_hash_map.h"
#include "Items.h"

struct WeaponSpec {{
    int32_t baseDamage;
    int32_t RPM;
    int32_t maxAmmo;
    int32_t MOA;
    int32_t vRecoilMin;
    int32_t vRecoilMax;
    int32_t hRecoilMax;
    int32_t spreadBase;
    int32_t spreadMax;
    int32_t spreadIncreasePerShot;
    int32_t spreadRecoveryRate;
    int32_t ammoType;
}};

struct ArmorSpec {{
    int32_t maxShieldPoint;
    int32_t DamageReductionRate;
    int32_t regenerationPerSecond;
}};

class ItemDataManager {{
public:
    ItemDataManager() = delete;
    ItemDataManager(const ItemDataManager&) = delete;
    ItemDataManager& operator=(const ItemDataManager&) = delete;

    static ItemType GetType(int32_t id) {{
        auto it = _typeMap.find(id);
        return (it != _typeMap.end()) ? it->second : ItemType::NONE;
    }}

    static std::string_view GetName(int32_t id) {{
        auto it = _nameMap.find(id);
        return (it != _nameMap.end()) ? std::string_view(it->second) : "알 수 없는 아이템";
    }}

    static const WeaponSpec* GetWeaponSpec(uint32_t itemId) {{
        auto it = _weaponSpecs.find(itemId);
        if (it != _weaponSpecs.end()) {{
            return &(it->second);
        }}
        return nullptr;
    }}

    static const ArmorSpec* GetArmorSpec(uint32_t itemId) {{
        auto it = _armorSpecs.find(itemId);
        if (it != _armorSpecs.end()) {{
            return &(it->second);
        }}
        return nullptr;
    }}

private:
    inline static const absl::flat_hash_map<int32_t, ItemType> _typeMap = {{
{type_entries_cpp}
    }};

    inline static const absl::flat_hash_map<int32_t, std::string> _nameMap = {{
{name_entries_cpp}
    }};

    inline static const absl::flat_hash_map<uint32_t, WeaponSpec> _weaponSpecs = {{
{weapon_entries_cpp}
    }};

    inline static const absl::flat_hash_map<uint32_t, ArmorSpec> _armorSpecs = {{
{armor_entries_cpp}
    }};
}};
"""

with open("ItemDataManager.h", "w", encoding="utf-8") as f:
    f.write(cpp_code)

print(f"{len(items)}개 아이템 → ItemDataManager.h 생성 완료")