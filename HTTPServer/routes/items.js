const express = require('express');
const { pool } = require('../config/mysqlClient');
const { redisClient } = require('../config/redisClient');
const { makeResponse } = require('../utils/response');
const { getShopItem } = require('../config/shopCache');
const { requireAuth } = require('../middleware/auth');
const { WAREHOUSE_SLOT_MIN, WAREHOUSE_SLOT_MAX } = require('../utils/slotLayout');
const { validateSnapshot, totalsMatch } = require('../utils/inventorySnapshot');

const router = express.Router();

router.post('/purchase', requireAuth, async (req, res) => {
    const { item_id, slot_index, quantity, inventory } = req.body;
    const uid = parseInt(req.sessionData.db_id, 10);

    if (
        !Number.isInteger(item_id) || item_id <= 0 ||
        !Number.isInteger(slot_index) ||
        !Number.isInteger(quantity) || quantity < 1 || quantity > 99 ||
        !Array.isArray(inventory)
    ) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "요청 형식이 올바르지 않습니다.", code: "ERR_BAD_REQUEST" }));
    }

    if (slot_index < WAREHOUSE_SLOT_MIN || slot_index > WAREHOUSE_SLOT_MAX) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "구매한 아이템은 창고에만 넣을 수 있습니다.", code: "ERR_INVALID_SLOT" }));
    }

    const snapshot = validateSnapshot(inventory);
    if (snapshot.error) {
        return res.status(400).json(makeResponse(false, 400, null, snapshot.error));
    }

    if (snapshot.slots.has(slot_index)) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "구매 슬롯이 이미 사용 중입니다.", code: "ERR_SLOT_OCCUPIED" }));
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

    const conn = await pool.getConnection();
    try {
        await conn.beginTransaction();

        const [dbRows] = await conn.query(
            `SELECT item_id, quantity FROM user_inventory WHERE uid = ? FOR UPDATE`,
            [uid]
        );

        if (!totalsMatch(dbRows, inventory)) {
            await conn.rollback();
            return res.status(409).json(makeResponse(false, 409, null, { message: "인벤토리 스냅샷이 일치하지 않습니다.", code: "ERR_SNAPSHOT_MISMATCH" }));
        }

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

router.post('/sell', requireAuth, async (req, res) => {
    const { item_id, slot_index, quantity, inventory } = req.body;
    const uid = parseInt(req.sessionData.db_id, 10);

    if (
        !Number.isInteger(item_id) || item_id <= 0 ||
        !Number.isInteger(slot_index) ||
        !Number.isInteger(quantity) || quantity < 1 ||
        !Array.isArray(inventory)
    ) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "요청 형식이 올바르지 않습니다.", code: "ERR_BAD_REQUEST" }));
    }

    const snapshot = validateSnapshot(inventory);
    if (snapshot.error) {
        return res.status(400).json(makeResponse(false, 400, null, snapshot.error));
    }

    const soldEntry = snapshot.slots.get(slot_index);
    if (soldEntry === undefined) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "판매할 아이템이 없는 슬롯입니다.", code: "ERR_SLOT_EMPTY" }));
    }
    if (soldEntry.item_id !== item_id || soldEntry.quantity !== quantity) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "판매 대상이 인벤토리와 일치하지 않습니다.", code: "ERR_ITEM_MISMATCH" }));
    }

    const conn = await pool.getConnection();
    try {
        await conn.beginTransaction();

        const [dbRows] = await conn.query(
            `SELECT item_id, quantity FROM user_inventory WHERE uid = ? FOR UPDATE`,
            [uid]
        );

        if (!totalsMatch(dbRows, inventory)) {
            await conn.rollback();
            return res.status(409).json(makeResponse(false, 409, null, { message: "인벤토리 스냅샷이 일치하지 않습니다.", code: "ERR_SNAPSHOT_MISMATCH" }));
        }

        const [[userRow]] = await conn.query(
            `SELECT money FROM users WHERE uid = ? FOR UPDATE`,
            [uid]
        );

        // 총량 대조를 통과했으므로 이 item_id 는 items 에 실재한다 — 캐시에 없으면 캐시 쪽 문제다
        const cachedPrice = await redisClient.hGet(`item_meta:${item_id}`, 'price');
        const price = Number.parseInt(cachedPrice, 10);
        if (!Number.isInteger(price) || price < 0) {
            await conn.rollback();
            console.error(`[Items] Sell - item_meta 가격 조회 실패 (item_id=${item_id}, price=${cachedPrice})`);
            return res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
        }

        const payout = quantity * price;

        await conn.query(`DELETE FROM user_inventory WHERE uid = ?`, [uid]);

        const newInventory = inventory.filter(e => e.slot_index !== slot_index);
        if (newInventory.length > 0) {
            const placeholders = newInventory.map(() => '(?, ?, ?, ?)').join(', ');
            const values = newInventory.flatMap(e => [uid, e.item_id, e.slot_index, e.quantity]);
            await conn.query(
                `INSERT INTO user_inventory (uid, item_id, slot_index, quantity) VALUES ${placeholders}`,
                values
            );
        }

        await conn.query(
            `UPDATE users SET money = money + ? WHERE uid = ?`,
            [payout, uid]
        );

        await conn.commit();

        res.status(200).json(makeResponse(true, 200, {
            money: userRow.money + payout,
            inventory: newInventory
        }));
    } catch (error) {
        await conn.rollback().catch(() => {});
        console.error("[Items] Sell Error:", error);
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
