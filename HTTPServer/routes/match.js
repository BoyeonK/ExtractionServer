// routes/match.js
const express = require('express');
const crypto = require('crypto');
const { redisClient } = require('../config/redisClient');
const { pool } = require('../config/mysqlClient');
const { makeResponse } = require('../utils/response');
const { sendHttpMatchMake, sendHttpMatchMakeCancel, sendH2M2DBindClientIpToSession } = require('../ipc/ipcManager');

const router = express.Router();

const scripts = {
    matchSuccess: `
        local currentStatus = redis.call('HGET', KEYS[1], 'status')
        if currentStatus == 'WAITING' then
            redis.call('HSET', KEYS[1], 'status', 'SUCCESS', 'udpServerIp', ARGV[1], 'udpServerPort', ARGV[2], 'roomToken', ARGV[3])
            return 1
        else
            return 0
        end
    `,
    matchCancel: `
        local ticketId = KEYS[1]
        local reqUid = ARGV[1]
        
        local ticketUid = redis.call('HGET', ticketId, 'uid')
        
        -- 이미 큐에서 빠졌거나 없는 티켓 (취소 성공으로 간주)
        if not ticketUid then
            return 1
        end
        
        -- 남의 티켓을 취소하려고 시도함
        if ticketUid ~= reqUid then
            return -1
        end

        -- 상태 확인 및 삭제
        local currentStatus = redis.call('HGET', ticketId, 'status')
        if currentStatus == 'WAITING' then
            redis.call('DEL', ticketId)
            return 1
        else
            return 0
        end
    `
};

// ==========================================================
// 인증 미들웨어. Express에서 미들웨어의 동작방식 이해할 것.
// ==========================================================
async function requireAuth(req, res, next) {
    const sessionId = req.headers['x-session-id'];
    if (!sessionId) return res.status(401).json(makeResponse(false, 401, null, { message: "세션 ID가 없습니다." }));

    try {
        const sessionData = await redisClient.hGetAll(sessionId);
        
        if (Object.keys(sessionData).length === 0) {
            return res.status(401).json(makeResponse(false, 401, null, { message: "만료된 세션입니다." }));
        }

        // 라우터에서 쓸 수 있게 req 객체에 세션 데이터를 통째로 달아줍니다
        req.sessionData = sessionData; 
        next();
    } catch (error) {
        console.error("[Middleware] Auth Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류" }));
    }
}

// ==========================================================
// 매치메이킹 시작 API
// ==========================================================
router.post('/start', requireAuth, async (req, res) => {
    // 클라이언트 데이터 예시:
    // equippedItems: [{ itemId: 101, quantity: 5 }, { itemId: 105, quantity: 1 }]
    const { mapId, loadoutType, equippedItems } = req.body;
    const { user_id, db_id, rating, aggression } = req.sessionData;

    try {
        // TODO : 매칭 시도한 순간, 인벤토리를 잠궈야 할듯?
        if (loadoutType !== 'FREE' && loadoutType !== 'CUSTOM') {
            return res.status(400).json(makeResponse(false, 400, null, { message: "잘못된 loadoutType 입니다." }));
        }

        let finalItems = "[]";

        if (loadoutType === 'FREE') {
            finalItems = "[]"; 
            
        } else if (loadoutType === 'CUSTOM') {
            // 야끼런이어도 빈 배열은 들고 오셔야 됩니다. null인 경우 분기처리
            if (!equippedItems || !Array.isArray(equippedItems)) {
                return res.status(400).json(makeResponse(false, 400, null, { message: "CUSTOM 모드에서는 equippedItems 배열이 필수입니다." }));
            }

            if (equippedItems && Array.isArray(equippedItems) && equippedItems.length > 0) {
                const requestedItemIds = equippedItems.map(item => item.itemId);
                // Set으로 바꿨는데 수량 다르면, 중복있음. 클라이언트에서 중복제거 하고 올 것
                const uniqueItemIds = new Set(requestedItemIds);
                if (uniqueItemIds.size !== requestedItemIds.length) {
                    return res.status(400).json(makeResponse(false, 400, null, { message: "중복된 아이템 ID가 포함되어 있습니다." }));
                }
                
                const placeholders = requestedItemIds.map(() => '?').join(',');
                
                // IN 쿼리로 요청한 아이템들의 '현재 수량'을 DB에서 싹 긁어옵니다.
                const [rows] = await pool.query(
                    `SELECT item_id, quantity FROM user_inventory WHERE uid = ? AND item_id IN (${placeholders})`,
                    [db_id, ...requestedItemIds]
                );

                // DB 결과를 object로 변환
                // 예: ownedItems = { "101": 10, "105": 1 }
                const ownedItems = {};
                rows.forEach(row => {
                    ownedItems[row.item_id] = row.quantity;
                });

                // 클라이언트가 요청한 아이템 수량이 검사
                //TODO : 해당 구간에서 오류가 반복될 경우, cheat의심
                for (const reqItem of equippedItems) {
                    const ownedQty = ownedItems[reqItem.itemId] || 0; // DB에 아예 없으면 0개로 취급
  
                    if (ownedQty < reqItem.quantity) {
                        return res.status(400).json(makeResponse(false, 400, null, { 
                            message: `아이템(ID: ${reqItem.itemId})의 보유 수량이 부족하거나 없습니다. (요청: ${reqItem.quantity}, 보유: ${ownedQty})` 
                        }));
                    }
                    
                    if (reqItem.quantity <= 0) {
                        return res.status(400).json(makeResponse(false, 400, null, { message: "아이템 수량은 1개 이상이어야 합니다." }));
                    }
                }
            }
            
            finalItems = JSON.stringify(equippedItems);
        }

        // 유효성 검사 통과 시 Redis에 매칭 티켓 발급
        const ticketId = "ticket_" + crypto.randomUUID(); 

        await redisClient.hSet(ticketId, {
            uid: db_id,
            user_id: user_id,
            rating: rating.toString(),
            aggression: aggression.toString(),
            loadout_type: loadoutType,
            status: "WAITING",
            map_id: (mapId ?? 0).toString(),
            items: finalItems  // C++ 서버에서 추후 이 JSON을 파싱해서 수량만큼 스폰과 동시에 DB에서 그 만큼을 제거.
        });
        await redisClient.expire(ticketId, 300);

        // TODO : 빌드할 때 로그 ㄷ지워야댐
        console.log(`매치 테스트 1 - O : 최초 Redis티켓 생성 ID: ${user_id}, Ticket: ${ticketId}, Items: ${finalItems}`);

        sendHttpMatchMake(ticketId);

        res.status(200).json(makeResponse(true, 200, { ticketId }));
    } catch (error) {
        console.error("매치 테스트 1 - X : 최초 매치 요청 처리부분에서 에러.", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류" }));
    }
});

// ==========================================================
// 매치메이킹 상태 확인 (Polling) API
// ==========================================================
router.get('/status', requireAuth, async (req, res) => {
    const { ticketId } = req.query;

    if (!ticketId) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "티켓 ID가 필요합니다." }));
    }

    try {
        const ticketData = await redisClient.hGetAll(ticketId);

        // 티켓이 없을 수도 잇음 (5분 지났거나, 서버가 튕겼거나, 요청 처리 시점에 이미 취소됨)
        if (Object.keys(ticketData).length === 0) {
            return res.status(404).json(makeResponse(false, 404, null, { message: "존재하지 않거나 만료된 티켓입니다.", status: "EXPIRED" }));
        }

        // 본인 티켓이 맞는지
        if (ticketData.uid !== req.sessionData.db_id) {
            return res.status(403).json(makeResponse(false, 403, null, { message: "본인의 티켓이 아닙니다." }));
        }

        // 상태에 따른 응답 분기
        if (ticketData.status === "WAITING" || ticketData.status === "INPROGRESS") {
            return res.status(200).json(makeResponse(true, 200, { status: "WAITING" }));
            
        } else if (ticketData.status === "SUCCESS") {
            console.log(`매치 테스트 10 - O : /status요청에 의해 토큰 전송 Token: ${ticketData.token}`);
            return res.status(200).json(makeResponse(true, 200, { 
                status: "SUCCESS",
                roomToken: ticketData.token
            }));
        } else {
            return res.status(500).json(makeResponse(false, 500, null, { message: "알 수 없는 상태입니다." }));
        }

    } catch (error) {
        console.error("[Match] Status Check Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류" }));
    }
});

// ==========================================================
// 데디케이티드 서버 접속 정보 획득 API (토큰 교환)
// ==========================================================
function getServerDetectedIp(req) {
    let ip = req.headers['x-forwarded-for'] || req.socket.remoteAddress;
    if (ip && ip.includes(',')) ip = ip.split(',')[0].trim();
    if (ip && ip.startsWith('::ffff:')) ip = ip.substring(7);
    return ip || "0.0.0.0";
}

router.post('/connect', requireAuth, async (req, res) => {
    // 이제 클라이언트는 ticketId 없이 순수하게 토큰만 보냅니다.
    const { roomToken } = req.body;

    if (!roomToken) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "방 입장 토큰이 필요합니다." }));
    }

    try {
        const tokenData = await redisClient.hGetAll(roomToken);

        if (Object.keys(tokenData).length === 0) {
            return res.status(404).json(makeResponse(false, 404, null, { message: "유효하지 않거나 만료된 입장 토큰입니다." }));
        }

        const clientPublicIp = getServerDetectedIp(req);

        sendH2M2DBindClientIpToSession(roomToken, clientPublicIp)
        console.log("매치 테스트 11 : ip, port, sKey, sId를 클라이언트에 전달");

        return res.status(200).json(makeResponse(true, 200, {
            ip: tokenData.udpServerIp,
            port: parseInt(tokenData.port, 10),
            securityKey: tokenData.security_key,
            ingameSessionId: parseInt(tokenData.session_id, 10),
        }));

    } catch (error) {
        console.error("[Match] Connect Info Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류" }));
    }
});

// ==========================================================
// 매치메이킹 취소 API
// ==========================================================
router.post('/cancel', requireAuth, async (req, res) => {
    const { ticketId } = req.body;
    const { db_id } = req.sessionData;

    if (!ticketId) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "티켓 ID가 필요합니다." }));
    }

    try {
        // KEYS[1] 에는 ticketId를, ARGV[1] 에는 내 db_id를 넘겨줍니다.
        const result = await redisClient.eval(scripts.matchCancel, {
            keys: [ticketId],
            arguments: [db_id.toString()]
        });

        if (result === 1) {
            // 성공
            console.log(`매치 취소 테스트 1 - O : Ticket: ${ticketId}에 대응하는 Redis의 티켓 파기`);
            sendHttpMatchMakeCancel(ticketId);
            return res.status(200).json(makeResponse(true, 200, { message: "매칭이 취소되었습니다." }));
            
        } else if (result === 0) {
            // 이미 매칭 잡힘 ㅅㄱ
            return res.status(409).json(makeResponse(false, 409, null, { 
                message: "이미 매칭이 성사되어 취소할 수 없습니다.",
                code: "ERR_MATCH_ALREADY_SUCCESS" 
            }));
            
        } else if (result === -1) {
            // 권한 없음 (핵, 클라변조)
            return res.status(403).json(makeResponse(false, 403, null, { message: "본인의 티켓만 취소할 수 있습니다." }));
        }

    } catch (error) {
        console.error("[Match] Cancel Error:", error);
        res.status(500).json(makeResponse(false, 500, null, { message: "서버 내부 오류" }));
    }
});

module.exports = router;