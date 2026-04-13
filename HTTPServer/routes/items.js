const express = require('express');
const { redisClient } = require('../config/redisClient');
const { pool } = require('../config/mysqlClient');
const { makeResponse } = require('../utils/response');

const router = express.Router();

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
            `SELECT item_id, quantity FROM user_inventory WHERE uid = ?`,
            [uid]
        );

        res.status(200).json(makeResponse(true, 200, { inventory }));
    } catch (error) {
        console.error("[Items] Inventory Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    }
});

module.exports = router;
