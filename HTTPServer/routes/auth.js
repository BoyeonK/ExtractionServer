const express = require('express');
const crypto = require('crypto');
const bcrypt = require('bcrypt');
const { redisClient } = require('../config/redisClient');
const { pool } = require('../config/mysqlClient');
const { makeResponse } = require('../utils/response');
const { getActiveItemIds } = require('../config/shopCache');

const router = express.Router();
const saltRounds = 11;

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

// ==========================================================
// 회원가입 API
// ==========================================================
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
        // INSERT 후 생성된 uid를 가져오기 위해 result 객체를 받습니다.
        const [result] = await pool.query('INSERT INTO users (login_id, password) VALUES (?, ?)', [id, hashedPassword]);
        const newUid = result.insertId;

        const sessionId = "sess_" + crypto.randomUUID();

        await redisClient.hSet(sessionId, {
            user_id: id,
            db_id: newUid.toString(),
            user_type: "1",
            rating: "1500",
            aggression: "7"
        });
        await redisClient.expire(sessionId, 3600);
        await redisClient.set(`user_sess:${id}`, sessionId, { EX: 3600 });

        res.status(201).json(makeResponse(true, 201, { sessionId, uid: newUid, money: 0 }));
    } catch (error) {
        console.error("[Auth] Signup Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    }
});

// ==========================================================
// 로그인 API
// ==========================================================
router.post('/login', async (req, res) => {
    const { id, password } = req.body;

    if (!id || !password) return res.status(400).json(makeResponse(false, 400, null, { message: "ID와 Password가 필요합니다.", code: "ERR_BAD_REQUEST" }));

    // 유효성 검사
    const idRegex = /^[a-zA-Z0-9]{4,16}$/;
    const pwRegex = /^[a-zA-Z0-9!@#$%^&*()]{4,16}$/;
    
    if (!idRegex.test(id) || !pwRegex.test(password)) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "ID 또는 비밀번호의 형식이 올바르지 않습니다.", code: "ERR_INVALID_FORMAT" }));
    }

    try {
        // 매치메이킹을 위해 rating과 aggression_level을 DB에서 가져옴.
        const [rows] = await pool.query('SELECT uid, login_id, password, rating, aggression_level, money FROM users WHERE login_id = ?', [id]);
        if (rows.length === 0) {
            return res.status(401).json(makeResponse(false, 401, null, { message: "존재하지 않는 ID입니다." }));
        }

        const user = rows[0];
        const isMatch = await bcrypt.compare(password, user.password);
        if (!isMatch) {
            return res.status(401).json(makeResponse(false, 401, null, { message: "비밀번호가 틀렸습니다." }));
        }

        // 중복 로그인 방지
        const oldSessionId = await redisClient.get(`user_sess:${id}`);
        if (oldSessionId) {
            await redisClient.del(oldSessionId);
            console.log(`[Auth] 기존 접속 끊기: ${id}`);
        }

        const sessionId = "sess_" + crypto.randomUUID();

        // DB에서 가져온 최신 rating과 aggression 캐싱, 인벤토리 조회를 병렬 처리
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

        res.status(200).json(makeResponse(true, 200, { sessionId, uid: user.uid, money: user.money, inventory, shopItems: getActiveItemIds() }));
    } catch (error) {
        console.error("[Auth] Login Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    }
});

// ==========================================================
// 게스트 로그인 API
// ==========================================================
router.post('/guest', async (req, res) => {
    try {
        const guestId = "guest_" + crypto.randomUUID().split('-')[0];
        const sessionId = "sess_" + crypto.randomUUID();

        const guestDbId = await redisClient.decr('guest_uid_counter');

        // 게스트 Hash 저장
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
        // result가 0이든 1이든, 결과적으로 로그아웃 상태가 된 것이니 모두 200 성공 응답
        res.status(200).json(makeResponse(true, 200, { message: "로그아웃 되었습니다." }));
    } catch (error) {
        console.error("[Auth] Logout Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류", code: "ERR_INTERNAL" }));
    }
});

module.exports = router;