const express = require('express');
const crypto = require('crypto');
const bcrypt = require('bcrypt');
const { redisClient } = require('../config/redisClient');
const { pool } = require('../config/mysqlClient');
const { makeResponse } = require('../utils/response');
const { getActiveShopItems } = require('../config/shopCache');
const { requireAuth } = require('../middleware/auth');

const router = express.Router();
const saltRounds = 11;

// users.aggression_level 의 DB 기본값과 같아야 한다. INSERT 에 명시해 둘이 갈라지지 않게 한다
const DEFAULT_AGGRESSION = 5;

const scripts = {
    logout: `
        local sessionId = KEYS[1]
        local userId = redis.call('HGET', sessionId, 'user_id')
        
        -- 이미 세션이 없으면 0 반환 (성공 처리용)
        if not userId then
            return 0
        end

        -- 인증 세션 파기
        redis.call('DEL', sessionId)

        -- 중복 방지 키 파기
        local userSessKey = 'user_sess:' .. userId
        if redis.call('GET', userSessKey) == sessionId then
            redis.call('DEL', userSessKey)
        end

        return 1
    `
};

router.post('/signup', async (req, res) => {
    const { id, password } = req.body;
    if (!id || !password) return res.status(400).json(makeResponse(false, 400, null, { message: "ID와 Password가 필요합니다.", code: "ERR_BAD_REQUEST" }));

    const idRegex = /^[a-zA-Z0-9]{4,16}$/;
    const pwRegex = /^[a-zA-Z0-9!@#$%^&*()]{4,16}$/;

    if (!idRegex.test(id)) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "ID는 4~16자의 영문, 숫자 조합이어야 합니다.", code: "ERR_INVALID_ID" }));
    }

    if (!pwRegex.test(password)) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "비밀번호는 4~16자의 영문, 숫자, 특수문자(!@#$%^&*())로만 구성되어야 합니다.", code: "ERR_INVALID_PW" }));
    }

    try {
        const [rows] = await pool.query('SELECT login_id FROM users WHERE login_id = ?', [id]);
        if (rows.length > 0) {
            return res.status(409).json(makeResponse(false, 409, null, { message: "이미 존재하는 ID입니다.", code: "ERR_CONFLICT" }));
        }

        const hashedPassword = await bcrypt.hash(password, saltRounds);
        const [result] = await pool.query('INSERT INTO users (login_id, password, aggression_level) VALUES (?, ?, ?)', [id, hashedPassword, DEFAULT_AGGRESSION]);
        const newUid = result.insertId;

        const sessionId = "sess_" + crypto.randomUUID();

        await redisClient.hSet(sessionId, {
            user_id: id,
            db_id: newUid.toString(),
            user_type: "1",
            rating: "1500",
            aggression: DEFAULT_AGGRESSION.toString()
        });
        await redisClient.expire(sessionId, 3600);
        await redisClient.set(`user_sess:${id}`, sessionId, { EX: 3600 });

        res.status(201).json(makeResponse(true, 201, { sessionId, uid: newUid, money: 0, shopItems: getActiveShopItems() }));
    } catch (error) {
        console.error("[Auth] Signup Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    }
});

router.post('/login', async (req, res) => {
    const { id, password } = req.body;

    if (!id || !password) return res.status(400).json(makeResponse(false, 400, null, { message: "ID와 Password가 필요합니다.", code: "ERR_BAD_REQUEST" }));

    const idRegex = /^[a-zA-Z0-9]{4,16}$/;
    const pwRegex = /^[a-zA-Z0-9!@#$%^&*()]{4,16}$/;
    
    if (!idRegex.test(id) || !pwRegex.test(password)) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "ID 또는 비밀번호의 형식이 올바르지 않습니다.", code: "ERR_INVALID_FORMAT" }));
    }

    try {
        const [rows] = await pool.query('SELECT uid, login_id, password, rating, aggression_level, money FROM users WHERE login_id = ?', [id]);
        if (rows.length === 0) {
            return res.status(401).json(makeResponse(false, 401, null, { message: "존재하지 않는 ID입니다." }));
        }

        const user = rows[0];
        const isMatch = await bcrypt.compare(password, user.password);
        if (!isMatch) {
            return res.status(401).json(makeResponse(false, 401, null, { message: "비밀번호가 틀렸습니다." }));
        }

        const oldSessionId = await redisClient.get(`user_sess:${id}`);
        if (oldSessionId) {
            await redisClient.del(oldSessionId);
            console.log(`[Auth] 기존 접속 끊기: ${id}`);
        }

        const sessionId = "sess_" + crypto.randomUUID();

        const [, , [inventory]] = await Promise.all([
            redisClient.hSet(sessionId, {
                user_id: id,
                db_id: user.uid.toString(),
                user_type: "1",
                rating: user.rating.toString(),
                aggression: user.aggression_level.toString()
            }),
            redisClient.set(`user_sess:${id}`, sessionId, { EX: 3600 }),
            pool.query(
                `SELECT item_id, slot_index, quantity FROM user_inventory WHERE uid = ?`,
                [user.uid]
            )
        ]);
        await redisClient.expire(sessionId, 3600);

        res.status(200).json(makeResponse(true, 200, { sessionId, uid: user.uid, money: user.money, inventory, shopItems: getActiveShopItems() }));
    } catch (error) {
        console.error("[Auth] Login Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    }
});

router.post('/guest', async (req, res) => {
    try {
        const guestId = "guest_" + crypto.randomUUID().split('-')[0];
        const sessionId = "sess_" + crypto.randomUUID();

        const guestDbId = await redisClient.decr('guest_uid_counter');

        await redisClient.hSet(sessionId, {
            user_id: guestId,
            db_id: guestDbId.toString(), // 음수 ID (-1, -2 ...) 할당
            user_type: "0",
            rating: "1500",
            aggression: "4"
        });
        await redisClient.expire(sessionId, 3600); 

        await redisClient.set(`user_sess:${guestId}`, sessionId, { EX: 3600 });

        console.log(`[Auth] 게스트 접속: ${guestId} (임시 UID: ${guestDbId})`);
        
        res.status(200).json(makeResponse(true, 200, { sessionId, guestId, uid:guestDbId }));
    } catch (error) {
        console.error("[Auth] Guest Login Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    }
});

router.post('/logout', async (req, res) => {
    const sessionId = req.headers['x-session-id'];
    if (!sessionId) return res.status(400).json(makeResponse(false, 400, null, { message: "세션 ID가 없습니다." }));

    try {
        const result = await redisClient.eval(scripts.logout, { keys: [sessionId] });
        res.status(200).json(makeResponse(true, 200, { message: "로그아웃 되었습니다." }));
    } catch (error) {
        console.error("[Auth] Logout Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    }
});

// 매치 종료 후 로비 복귀 시 세션 유지 확인 + 로비 데이터 재조회 (클라이언트 Login 스킵용).
// 세션 검증과 TTL 슬라이딩 갱신은 requireAuth 미들웨어가 수행하므로 여기 도달하면 세션은 유효하다.
router.post('/session/resume', requireAuth, async (req, res) => {
    const { db_id, user_type } = req.sessionData;
    const uid = parseInt(db_id, 10);

    // 손상된 세션 데이터는 재사용 불가 — 클라이언트를 일반 로그인 플로우로 돌린다.
    if (Number.isNaN(uid)) {
        return res.status(401).json(makeResponse(false, 401, null, { message: "손상된 세션입니다." }));
    }

    try {
        // 게스트는 MySQL 행이 없다(uid 음수).
        if (user_type !== "1") {
            return res.status(200).json(makeResponse(true, 200, { uid, money: 0, inventory: [] }));
        }

        const [[userRows], [inventory]] = await Promise.all([
            pool.query('SELECT money FROM users WHERE uid = ?', [uid]),
            pool.query('SELECT item_id, slot_index, quantity FROM user_inventory WHERE uid = ?', [uid])
        ]);

        if (userRows.length === 0) {
            return res.status(401).json(makeResponse(false, 401, null, { message: "존재하지 않는 계정입니다." }));
        }

        res.status(200).json(makeResponse(true, 200, { uid, money: userRows[0].money, inventory }));
    } catch (error) {
        console.error("[Auth] Session Resume Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    }
});

module.exports = router;