const { pool } = require('./mysqlClient');

const shopMap = new Map();

async function loadShopCache() {
    const [rows] = await pool.query('SELECT item_id, is_active FROM shop_items');
    for (const row of rows) {
        shopMap.set(row.item_id, row.is_active);
    }
    console.log(`[ShopCache] ${shopMap.size}개 아이템 캐싱 완료`);
}

function getShopItem(itemId) {
    return shopMap.get(itemId);
}

function getActiveItemIds() {
    const result = [];
    for (const [itemId, isActive] of shopMap) {
        if (isActive) result.push(itemId);
    }
    return result;
}

module.exports = { loadShopCache, getShopItem, getActiveItemIds };
