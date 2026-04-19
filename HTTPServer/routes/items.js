const express = require('express');
const { redisClient } = require('../config/redisClient');
const { pool } = require('../config/mysqlClient');
const { makeResponse } = require('../utils/response');
const { getShopItem } = require('../config/shopCache');

const router = express.Router();

// ==========================================================
// 아이템 구매 API
// ==========================================================
router.post('/purchase', async (req, res) => {
    const sessionId = req.headers['x-session-id'];
    if (!sessionId) return res.status(401).json(makeResponse(false, 401, null, { message: "세션 ID가 없습니다." }));

    const { item_id, slot_index, quantity, inventory } = req.body;

    // ── [1] 입력 검증 ──────────────────────────────────────────────────────
    if (
        !Number.isInteger(item_id) || item_id <= 0 ||
        !Number.isInteger(slot_index) || slot_index < 0 ||
        !Number.isInteger(quantity) || quantity < 1 || quantity > 99 ||
        !Array.isArray(inventory)
    ) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "요청 형식이 올바르지 않습니다.", code: "ERR_BAD_REQUEST" }));
    }

    // 스냅샷 내 slot_index 중복 검사
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

    // 구매 슬롯이 스냅샷에 이미 존재하는지 확인
    if (snapshotSlots.has(slot_index)) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "구매 슬롯이 이미 사용 중입니다.", code: "ERR_SLOT_OCCUPIED" }));
    }

    // ── [2] 세션 검증 ──────────────────────────────────────────────────────
    let uid;
    try {
        const sessionData = await redisClient.hGetAll(sessionId);
        if (Object.keys(sessionData).length === 0) {
            return res.status(401).json(makeResponse(false, 401, null, { message: "유효하지 않은 세션입니다.", code: "ERR_UNAUTHORIZED" }));
        }
        uid = parseInt(sessionData.db_id, 10);
    } catch (error) {
        console.error("[Items] Purchase Session Error:", error);
        return res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    }

    const conn = await pool.getConnection();
    try {
        // ── [3] 스냅샷 대조 (item_id별 수량 합계 비교) ────────────────────
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

        // ── [4] 판매 여부 검증 (캐시) + 가격 조회 ───────────────────────
        const shopItem = getShopItem(item_id);
        if (shopItem === undefined) {
            return res.status(404).json(makeResponse(false, 404, null, { message: "존재하지 않는 아이템입니다.", code: "ERR_ITEM_NOT_FOUND" }));
        }
        if (!shopItem) {
            return res.status(403).json(makeResponse(false, 403, null, { message: "판매 중인 아이템이 아닙니다.", code: "ERR_ITEM_NOT_FOR_SALE" }));
        }

        const [[itemRow]] = await conn.query(
            `SELECT price FROM items WHERE item_id = ?`,
            [item_id]
        );

        const totalCost = itemRow.price * quantity;

        await conn.beginTransaction();

        const [[userRow]] = await conn.query(
            `SELECT money FROM users WHERE uid = ? FOR UPDATE`,
            [uid]
        );
        if (userRow.money < totalCost) {
            await conn.rollback();
            return res.status(402).json(makeResponse(false, 402, null, { message: "잔액이 부족합니다.", code: "ERR_INSUFFICIENT_FUNDS" }));
        }

        // ── [5] 트랜잭션: 인벤토리 덮어쓰기 + 새 아이템 INSERT + 머니 차감 ──
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

// ==========================================================
// 인벤토리 조회 API
// ==========================================================
router.get('/inventory', async (req, res) => {
    const sessionId = req.headers['x-session-id'];
    if (!sessionId) return res.status(401).json(makeResponse(false, 401, null, { message: "세션 ID가 없습니다." }));

    try {
        const sessionData = await redisClient.hGetAll(sessionId);
        if (Object.keys(sessionData).length === 0) {
            return res.status(401).json(makeResponse(false, 401, null, { message: "유효하지 않은 세션입니다.", code: "ERR_UNAUTHORIZED" }));
        }

        const uid = sessionData.db_id;
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
