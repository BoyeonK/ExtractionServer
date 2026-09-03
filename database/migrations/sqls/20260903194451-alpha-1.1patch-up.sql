UPDATE armor_specs
    SET regeneration_per_second=800,max_shield_point=10000
    WHERE item_id=4;
UPDATE weapon_specs
    SET spread_recovery_rate=600
    WHERE item_id IN (1, 2, 3);
INSERT INTO items (item_id,item_name,item_type,description,price)
    VALUES (8,'전술 방탄 조끼','ARMOR','괜찮은 성능을 보여주는 방어구입니다.',40000);
INSERT INTO armor_specs (item_id,max_shield_point,damage_reduction_rate,regeneration_per_second)
    VALUES (8,16000,5000,800);