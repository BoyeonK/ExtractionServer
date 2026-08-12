const { redisClient, refreshSession } = require('../config/redisClient');
const { makeResponse } = require('../utils/response');

// 세션 검증 후 req.sessionData 에 Redis 세션 데이터를 통째로 첨부한다.
async function requireAuth(req, res, next) {
    const sessionId = req.headers['x-session-id'];
    if (!sessionId) return res.status(401).json(makeResponse(false, 401, null, { message: "세션 ID가 없습니다." }));

    try {
        const sessionData = await redisClient.hGetAll(sessionId);

        if (Object.keys(sessionData).length === 0) {
            return res.status(401).json(makeResponse(false, 401, null, { message: "만료된 세션입니다." }));
        }

        req.sessionData = sessionData;
        refreshSession(sessionId);
        next();
    } catch (error) {
        console.error("[Middleware] Auth Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류" }));
    }
}

module.exports = { requireAuth };
