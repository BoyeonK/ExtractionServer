-- 유저 기본 정보 테이블
CREATE TABLE users (
    uid INT AUTO_INCREMENT PRIMARY KEY,
    login_id VARCHAR(16) NOT NULL UNIQUE,
    password CHAR(60) NOT NULL,
    rating INT DEFAULT 1500,
    aggression_level INT DEFAULT 7,
    money INT UNSIGNED NOT NULL DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE items (
    item_id INT AUTO_INCREMENT PRIMARY KEY,
    item_name VARCHAR(16) NOT NULL,
    item_type ENUM('WEAPON', 'ARMOR', 'AMMO', 'MISC') NOT NULL,
    price INT UNSIGNED NOT NULL DEFAULT 0,
    description TEXT
);

CREATE TABLE user_inventory (
    inventory_id BIGINT AUTO_INCREMENT PRIMARY KEY,
    uid INT NOT NULL,
    item_id INT NOT NULL,
    slot_index TINYINT UNSIGNED NOT NULL,
    quantity INT DEFAULT 1,
    obtained_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_uid_slot (uid, slot_index),
    FOREIGN KEY (uid) REFERENCES users(uid) ON DELETE CASCADE,
    FOREIGN KEY (item_id) REFERENCES items(item_id)
);

CREATE TABLE weapon_specs (
    item_id INT NOT NULL PRIMARY KEY,
    base_damage INT UNSIGNED NOT NULL,
    rpm INT UNSIGNED NOT NULL,
    moa INT UNSIGNED NOT NULL,
    v_recoil_min INT UNSIGNED NOT NULL,
    v_recoil_max INT UNSIGNED NOT NULL,
    h_recoil_min INT UNSIGNED NOT NULL,
    h_recoil_max INT UNSIGNED NOT NULL,
    ammo_type INT UNSIGNED NOT NULL,
    FOREIGN KEY (item_id) REFERENCES items(item_id),
    FOREIGN KEY (ammo_type) REFERENCES items(item_id)
);

CREATE TABLE armor_specs (
    item_id INT NOT NULL PRIMARY KEY,
    max_shield_point INT UNSIGNED NOT NULL,
    damage_reduction_rate INT UNSIGNED NOT NULL,
    regeneration_per_second INT UNSIGNED NOT NULL,
    FOREIGN KEY (item_id) REFERENCES items(item_id)
);

CREATE TABLE shop_items (
    item_id INT NOT NULL PRIMARY KEY,
    is_active TINYINT(1) DEFAULT 1,
    FOREIGN KEY (item_id) REFERENCES items(item_id)
);