UPDATE weapon_specs
	SET spread_max=800, spread_recovery_rate=400, spread_base=40
	WHERE item_id IN (1, 2, 3);