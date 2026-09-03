--  Auto-generated SQL script #202609030924
UPDATE items
    SET description='훌륭한 대화수단입니다.'
    WHERE item_id=1;
UPDATE items
    SET description='검증된 성능의 소총입니다.'
    WHERE item_id=2;
UPDATE items
    SET item_name='SCAR'
    WHERE item_id=3;
UPDATE items
    SET description='없는 것 보다는 낫지만, 성능을 기대하긴 힘듭니다.',price=10000
    WHERE item_id=4;
UPDATE items
    SET description='5.56mm 탄환',price=10
    WHERE item_id=5;
UPDATE items
    SET description='7.62mm 탄환',price=10
    WHERE item_id=6;
UPDATE weapon_specs
    SET spread_recovery_rate=1200,spread_base=30
    WHERE item_id=1;
UPDATE weapon_specs
    SET spread_recovery_rate=1200,spread_base=30
    WHERE item_id=2;
UPDATE weapon_specs
    SET spread_recovery_rate=1200,spread_base=30
    WHERE item_id=3;