const { pool } = require('./mysqlClient');

const shopMap = new Map();

async function loadShopCache() {
    const [rows] = await pool.query(
        `SELECT s.item_id, s.is_active, i.price, i.item_type
         FROM shop_items s JOIN items i ON s.item_id = i.item_id`
    );
    for (const row of rows) {
        shopMap.set(row.item_id, { isActive: row.is_active, price: row.price, itemType: row.item_type });
    }
    console.log(`[ShopCache] ${shopMap.size}개 아이템 캐싱 완료`);
}

function getShopItem(itemId) {
    return shopMap.get(itemId); // { isActive, price } | undefined
}

function getActiveShopItems() {
    const result = [];
    for (const [itemId, { isActive, price }] of shopMap) {
        if (isActive) result.push({ item_id: itemId, price });
    }
    return result;
}

module.exports = { loadShopCache, getShopItem, getActiveShopItems };
