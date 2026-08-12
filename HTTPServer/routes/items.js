const express = require('express');
const { pool } = require('../config/mysqlClient');
const { makeResponse } = require('../utils/response');
const { getShopItem } = require('../config/shopCache');
const { requireAuth } = require('../middleware/auth');

const router = express.Router();

router.post('/purchase', requireAuth, async (req, res) => {
    const { item_id, slot_index, quantity, inventory } = req.body;
    const uid = parseInt(req.sessionData.db_id, 10);

    if (
        !Number.isInteger(item_id) || item_id <= 0 ||
        !Number.isInteger(slot_index) || slot_index < 0 ||
        !Number.isInteger(quantity) || quantity < 1 || quantity > 99 ||
        !Array.isArray(inventory)
    ) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "요청 형식이 올바르지 않습니다.", code: "ERR_BAD_REQUEST" }));
    }

    const snapshotSlots = new Set();
    for (const entry of inventory) {
        if (
            !Number.isInteger(entry.item_id) || entry.item_id <= 0 ||
            !Number.isInteger(entry.slot_index) || entry.slot_index < 0 ||
            !Number.isInteger(entry.quantity) || entry.quantity < 1
        ) {
            return res.status(400).json(makeResponse(false, 400, null, { message: "인벤토리 스냅샷 형식이 올바르지 않습니다.", code: "ERR_BAD_REQUEST" }));
        }
        if (snapshotSlots.has(entry.slot_index)) {
            return res.status(400).json(makeResponse(false, 400, null, { message: "스냅샷에 중복된 슬롯이 있습니다.", code: "ERR_DUPLICATE_SLOT" }));
        }
        snapshotSlots.add(entry.slot_index);
    }

    if (snapshotSlots.has(slot_index)) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "구매 슬롯이 이미 사용 중입니다.", code: "ERR_SLOT_OCCUPIED" }));
    }

    const conn = await pool.getConnection();
    try {
        const [dbRows] = await conn.query(
            `SELECT item_id, quantity FROM user_inventory WHERE uid = ?`,
            [uid]
        );

        const dbTotals = new Map();
        for (const row of dbRows) {
            dbTotals.set(row.item_id, (dbTotals.get(row.item_id) ?? 0) + row.quantity);
        }

        const snapTotals = new Map();
        for (const entry of inventory) {
            snapTotals.set(entry.item_id, (snapTotals.get(entry.item_id) ?? 0) + entry.quantity);
        }

        if (dbTotals.size !== snapTotals.size) {
            return res.status(409).json(makeResponse(false, 409, null, { message: "인벤토리 스냅샷이 일치하지 않습니다.", code: "ERR_SNAPSHOT_MISMATCH" }));
        }
        for (const [itemId, total] of dbTotals) {
            if (snapTotals.get(itemId) !== total) {
                return res.status(409).json(makeResponse(false, 409, null, { message: "인벤토리 스냅샷이 일치하지 않습니다.", code: "ERR_SNAPSHOT_MISMATCH" }));
            }
        }

        const shopItem = getShopItem(item_id);
        if (shopItem === undefined) {
            return res.status(404).json(makeResponse(false, 404, null, { message: "존재하지 않는 아이템입니다.", code: "ERR_ITEM_NOT_FOUND" }));
        }
        if (!shopItem.isActive) {
            return res.status(403).json(makeResponse(false, 403, null, { message: "판매 중인 아이템이 아닙니다.", code: "ERR_ITEM_NOT_FOR_SALE" }));
        }

        if ((shopItem.itemType === 'WEAPON' || shopItem.itemType === 'ARMOR') && quantity !== 1) {
            return res.status(400).json(makeResponse(false, 400, null, { message: "해당 아이템은 1개만 구매할 수 있습니다.", code: "ERR_INVALID_QUANTITY" }));
        }

        const totalCost = shopItem.price * quantity;

        await conn.beginTransaction();

        const [[userRow]] = await conn.query(
            `SELECT money FROM users WHERE uid = ? FOR UPDATE`,
            [uid]
        );
        if (userRow.money < totalCost) {
            await conn.rollback();
            return res.status(402).json(makeResponse(false, 402, null, { message: "잔액이 부족합니다.", code: "ERR_INSUFFICIENT_FUNDS" }));
        }

        await conn.query(`DELETE FROM user_inventory WHERE uid = ?`, [uid]);

        const newInventory = [...inventory, { item_id, slot_index, quantity }];
        if (newInventory.length > 0) {
            const placeholders = newInventory.map(() => '(?, ?, ?, ?)').join(', ');
            const values = newInventory.flatMap(e => [uid, e.item_id, e.slot_index, e.quantity]);
            await conn.query(
                `INSERT INTO user_inventory (uid, item_id, slot_index, quantity) VALUES ${placeholders}`,
                values
            );
        }

        await conn.query(
            `UPDATE users SET money = money - ? WHERE uid = ?`,
            [totalCost, uid]
        );

        await conn.commit();

        res.status(200).json(makeResponse(true, 200, {
            money: userRow.money - totalCost,
            inventory: newInventory
        }));
    } catch (error) {
        await conn.rollback().catch(() => {});
        console.error("[Items] Purchase Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    } finally {
        conn.release();
    }
});

router.get('/inventory', requireAuth, async (req, res) => {
    const uid = req.sessionData.db_id;

    try {
        const [inventory] = await pool.query(
            `SELECT item_id, slot_index, quantity FROM user_inventory WHERE uid = ?`,
            [uid]
        );

        res.status(200).json(makeResponse(true, 200, { inventory }));
    } catch (error) {
        console.error("[Items] Inventory Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    }
});

module.exports = router;
