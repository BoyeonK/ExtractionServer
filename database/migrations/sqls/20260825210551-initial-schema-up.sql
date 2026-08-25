CREATE TABLE `items` (
  `item_id` int NOT NULL AUTO_INCREMENT,
  `item_name` varchar(16) NOT NULL,
  `item_type` enum('WEAPON','ARMOR','AMMO','MISC') NOT NULL,
  `description` text,
  `price` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE `users` (
  `uid` int NOT NULL AUTO_INCREMENT,
  `login_id` varchar(16) NOT NULL,
  `password` char(60) NOT NULL,
  `rating` int DEFAULT '1500',
  `aggression_level` int DEFAULT '5',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `money` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`uid`),
  UNIQUE KEY `login_id` (`login_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE `shop_items` (
  `item_id` int NOT NULL,
  `is_active` tinyint(1) DEFAULT '1',
  PRIMARY KEY (`item_id`),
  CONSTRAINT `shop_items_ibfk_1` FOREIGN KEY (`item_id`) REFERENCES `items` (`item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE `armor_specs` (
  `item_id` int NOT NULL,
  `max_shield_point` int unsigned NOT NULL,
  `damage_reduction_rate` int unsigned NOT NULL,
  `regeneration_per_second` int unsigned NOT NULL,
  PRIMARY KEY (`item_id`),
  CONSTRAINT `armor_specs_ibfk_1` FOREIGN KEY (`item_id`) REFERENCES `items` (`item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE `weapon_specs` (
  `item_id` int NOT NULL,
  `base_damage` int unsigned NOT NULL,
  `rpm` int unsigned NOT NULL,
  `moa` int unsigned NOT NULL,
  `v_recoil_min` int unsigned NOT NULL,
  `v_recoil_max` int unsigned NOT NULL,
  `h_recoil_max` int unsigned NOT NULL,
  `spread_base` int unsigned NOT NULL DEFAULT '0',
  `spread_max` int unsigned NOT NULL DEFAULT '0',
  `spread_increase_per_shot` int unsigned NOT NULL DEFAULT '0',
  `spread_recovery_rate` int unsigned NOT NULL DEFAULT '0',
  `ammo_type` int NOT NULL,
  `ammo_max` int unsigned NOT NULL,
  PRIMARY KEY (`item_id`),
  KEY `ammo_type` (`ammo_type`),
  CONSTRAINT `weapon_specs_ibfk_1` FOREIGN KEY (`item_id`) REFERENCES `items` (`item_id`),
  CONSTRAINT `weapon_specs_ibfk_2` FOREIGN KEY (`ammo_type`) REFERENCES `items` (`item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE `user_inventory` (
  `inventory_id` bigint NOT NULL AUTO_INCREMENT,
  `uid` int NOT NULL,
  `item_id` int NOT NULL,
  `slot_index` tinyint unsigned NOT NULL,
  `quantity` int DEFAULT '1',
  `obtained_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`inventory_id`),
  UNIQUE KEY `uq_uid_slot` (`uid`,`slot_index`),
  KEY `item_id` (`item_id`),
  CONSTRAINT `user_inventory_ibfk_1` FOREIGN KEY (`uid`) REFERENCES `users` (`uid`) ON DELETE CASCADE,
  CONSTRAINT `user_inventory_ibfk_2` FOREIGN KEY (`item_id`) REFERENCES `items` (`item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;