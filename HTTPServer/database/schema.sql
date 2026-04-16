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
    item_type ENUM('WEAPON', 'ARMOR', 'AMMO', 'MISC'),
    description TEXT
);

CREATE TABLE user_inventory (
    inventory_id INT AUTO_INCREMENT PRIMARY KEY,
    uid INT NOT NULL,
    item_id INT NOT NULL,
    quantity INT DEFAULT 1,
    obtained_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (uid) REFERENCES users(uid) ON DELETE CASCADE,
    FOREIGN KEY (item_id) REFERENCES items(item_id)
);