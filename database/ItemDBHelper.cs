using System.Collections.Generic;

public enum ItemType { None, Weapon, Armor, Ammo, Misc }

public class ItemDB {
    public int Id { get; set; }
    public ItemType Type { get; set; }
    public string Name { get; set; }
    public string Description { get; set; }
}

public struct WeaponSpec {
    public int BaseDamage { get; set; }
    public int Rpm { get; set; }
    public int MaxAmmo { get; set; }
    public int Moa { get; set; }
    public int VRecoilMin { get; set; }
    public int VRecoilMax { get; set; }
    public int HRecoilMax { get; set; }
    public int SpreadBase { get; set; }
    public int SpreadMax { get; set; }
    public int SpreadIncreasePerShot { get; set; }
    public int SpreadRecoveryRate { get; set; }
    public int AmmoType { get; set; }
}

public struct ArmorSpec {
    public int MaxShieldPoint { get; set; }
    public int DamageReductionRate { get; set; }
    public int RegenerationPerSecond { get; set; }
}

public static class ItemDBHelper {
    private static readonly Dictionary<int, ItemDB> _items = new() {
        { 1, new ItemDB { Id = 1, Type = ItemType.Weapon, Name = "AK-47", Description = "훌륭한 대화수단입니다." } },
        { 2, new ItemDB { Id = 2, Type = ItemType.Weapon, Name = "M4A1", Description = "검증된 성능의 소총입니다." } },
        { 3, new ItemDB { Id = 3, Type = ItemType.Weapon, Name = "SCAR", Description = "테스트용 임시데이터" } },
        { 4, new ItemDB { Id = 4, Type = ItemType.Armor, Name = "경량 조끼", Description = "없는 것 보다는 낫지만, 성능을 기대하긴 힘듭니다." } },
        { 5, new ItemDB { Id = 5, Type = ItemType.Ammo, Name = "5.56mm", Description = "5.56mm 탄환" } },
        { 6, new ItemDB { Id = 6, Type = ItemType.Ammo, Name = "7.62mm", Description = "7.62mm 탄환" } },
        { 7, new ItemDB { Id = 7, Type = ItemType.Misc, Name = "돌맹이", Description = "테스트용 임시데이터" } },
        { 8, new ItemDB { Id = 8, Type = ItemType.Armor, Name = "전술 방탄 조끼", Description = "괜찮은 성능을 보여주는 방어구입니다." } },
    };

    private static readonly Dictionary<int, WeaponSpec> _weaponSpecs = new() {
        { 1, new WeaponSpec { BaseDamage = 4800, Rpm = 600, MaxAmmo = 30, Moa = 0, VRecoilMin = 200, VRecoilMax = 220, HRecoilMax = 50, SpreadBase = 30, SpreadMax = 800, SpreadIncreasePerShot = 120, SpreadRecoveryRate = 600, AmmoType = 6 } },
        { 2, new WeaponSpec { BaseDamage = 4000, Rpm = 700, MaxAmmo = 30, Moa = 0, VRecoilMin = 150, VRecoilMax = 165, HRecoilMax = 40, SpreadBase = 30, SpreadMax = 800, SpreadIncreasePerShot = 100, SpreadRecoveryRate = 600, AmmoType = 5 } },
        { 3, new WeaponSpec { BaseDamage = 4200, Rpm = 650, MaxAmmo = 30, Moa = 0, VRecoilMin = 200, VRecoilMax = 220, HRecoilMax = 50, SpreadBase = 30, SpreadMax = 800, SpreadIncreasePerShot = 120, SpreadRecoveryRate = 600, AmmoType = 5 } },
    };

    private static readonly Dictionary<int, ArmorSpec> _armorSpecs = new() {
        { 4, new ArmorSpec { MaxShieldPoint = 10000, DamageReductionRate = 5000, RegenerationPerSecond = 800 } },
        { 8, new ArmorSpec { MaxShieldPoint = 16000, DamageReductionRate = 5000, RegenerationPerSecond = 800 } },
    };

    public static ItemDB GetItem(int id) =>
        _items.TryGetValue(id, out var data) ? data : null;

    public static bool TryGetWeaponSpec(int id, out WeaponSpec spec) =>
        _weaponSpecs.TryGetValue(id, out spec);

    public static bool TryGetArmorSpec(int id, out ArmorSpec spec) =>
        _armorSpecs.TryGetValue(id, out spec);

    public static ItemType GetType(int id) => GetItem(id)?.Type ?? ItemType.Misc;
    public static string GetName(int id) => GetItem(id)?.Name ?? "알 수 없는 아이템";
}
