// routes/match.js
const express = require('express');
const crypto = require('crypto');
const { redisClient } = require('../config/redisClient');
const { pool } = require('../config/mysqlClient');
const { makeResponse } = require('../utils/response');
const { sendHttpMatchMake, sendHttpMatchMakeCancel, sendH2M2DBindClientIpToSession } = require('../ipc/ipcManager');
const { requireAuth } = require('../middleware/auth');

const WAREHOUSE_SLOT_MAX = 79;
const INVENTORY_SLOT_MIN = 80;
const LOADOUT_SLOT_MIN = 105;
const LOADOUT_SLOT_MAX = 107;
const VALID_MAP_IDS = new Set([0, 1]); // 0: MAP_TUTORIAL, 1: MAP_WINCHESTER
const VALID_CHARACTER_TYPES = new Set([0, 1, 2]);

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

        -- 티켓이 없음 (만료됐거나 이미 처리됨) → active_match만 정리, IPC 취소는 보내지 않음
        -- 이것은 테스트용이고, 실제로직이 완료된 이후에는 active_match를 정리하면 안됨. 
        if not ticketUid then
            return 2
        end

        -- 남의 티켓을 취소하려고 시도함
        if ticketUid ~= reqUid then
            return -1
        end

        -- WAITING 상태일 때만 삭제 허용
        local currentStatus = redis.call('HGET', ticketId, 'status')
        if currentStatus == 'WAITING' then
            redis.call('DEL', ticketId)
            return 1
        else
            -- INPROGRESS / SUCCESS 등 → 파기 불가
            return 0
        end
    `
};

// ==========================================================
// 매치메이킹 시작 API
// ==========================================================
router.post('/start', requireAuth, async (req, res) => {
    const { mapId, loadoutType, inventory, characterType } = req.body;
    const { user_id, db_id, rating, aggression } = req.sessionData;

    try {
        if (!Number.isInteger(mapId) || !VALID_MAP_IDS.has(mapId)) {
            return res.status(400).json(makeResponse(false, 400, null, { message: "유효하지 않은 mapId 입니다.", code: "ERR_INVALID_MAP_ID" }));
        }

        if (!Number.isInteger(characterType) || !VALID_CHARACTER_TYPES.has(characterType)) {
            return res.status(400).json(makeResponse(false, 400, null, { message: "유효하지 않은 characterType 입니다.", code: "ERR_INVALID_CHARACTER_TYPE" }));
        }

        if (loadoutType !== 'FREE' && loadoutType !== 'CUSTOM') {
            return res.status(400).json(makeResponse(false, 400, null, { message: "잘못된 loadoutType 입니다." }));
        }

        let inventoryItemsJson = "[]";
        let equipmentItemsJson = "[]";

        if (loadoutType === 'FREE') {
            // 기본값 "[]" 유지
        } else if (loadoutType === 'CUSTOM') {
            // ── [1] 입력 검증 ──────────────────────────────────────────────
            if (!Array.isArray(inventory)) {
                return res.status(400).json(makeResponse(false, 400, null, { message: "inventory 배열이 필요합니다.", code: "ERR_BAD_REQUEST" }));
            }

            const snapshotSlots = new Set();
            for (const entry of inventory) {
                if (
                    !Number.isInteger(entry.item_id) || entry.item_id <= 0 ||
                    !Number.isInteger(entry.slot_index) || entry.slot_index < 0 ||
                    !Number.isInteger(entry.quantity) || entry.quantity < 1
                ) {
                    return res.status(400).json(makeResponse(false, 400, null, { message: "inventory 형식이 올바르지 않습니다.", code: "ERR_BAD_REQUEST" }));
                }
                if (snapshotSlots.has(entry.slot_index)) {
                    return res.status(400).json(makeResponse(false, 400, null, { message: "중복된 슬롯이 있습니다.", code: "ERR_DUPLICATE_SLOT" }));
                }
                snapshotSlots.add(entry.slot_index);
            }

            const conn = await pool.getConnection();
            try {
                await conn.beginTransaction();

                // ── [2] DB 대조 (배타 락 적용) ──────────────────────────────
                const [dbRows] = await conn.query(
                    `SELECT item_id, quantity FROM user_inventory WHERE uid = ? FOR UPDATE`,
                    [db_id]
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
                    throw new Error("ERR_SNAPSHOT_MISMATCH"); // 트랜잭션 롤백을 위해 throw
                }
                for (const [itemId, total] of dbTotals) {
                    if (snapTotals.get(itemId) !== total) {
                        throw new Error("ERR_SNAPSHOT_MISMATCH"); // 트랜잭션 롤백을 위해 throw
                    }
                }

                // ── [3] DB 갱신 ────────────────────────────────────────────
                await conn.query(`DELETE FROM user_inventory WHERE uid = ?`, [db_id]);
                if (inventory.length > 0) {
                    const placeholders = inventory.map(() => '(?, ?, ?, ?)').join(', ');
                    const values = inventory.flatMap(e => [db_id, e.item_id, e.slot_index, e.quantity]);
                    await conn.query(
                        `INSERT INTO user_inventory (uid, item_id, slot_index, quantity) VALUES ${placeholders}`,
                        values
                    );
                }

                // ── [4] inventory / equipment 분리 추출 ───────────────────
                const inventoryRaw = inventory.filter(
                    e => e.slot_index >= INVENTORY_SLOT_MIN && e.slot_index < LOADOUT_SLOT_MIN
                );
                const equipmentRaw = inventory.filter(
                    e => e.slot_index >= LOADOUT_SLOT_MIN && e.slot_index <= LOADOUT_SLOT_MAX
                );

                // ── [5] 무기 슬롯 검증 (슬롯 105=주무기, 106=보조무기 중 최소 1개 WEAPON 필요)
                const weaponSlotItems = equipmentRaw.filter(
                    e => e.slot_index === LOADOUT_SLOT_MIN || e.slot_index === LOADOUT_SLOT_MIN + 1
                );

                if (weaponSlotItems.length === 0) {
                    throw new Error("ERR_NO_WEAPON_EQUIPPED"); // 트랜잭션 롤백을 위해 throw
                }

                const weaponItemIds = weaponSlotItems.map(e => e.item_id);
                const [itemTypeRows] = await conn.query(
                    `SELECT item_id, item_type FROM items WHERE item_id IN (?)`,
                    [weaponItemIds]
                );
                const hasWeapon = itemTypeRows.some(row => row.item_type === 'WEAPON');

                if (!hasWeapon) {
                    throw new Error("ERR_NO_WEAPON_EQUIPPED"); // 트랜잭션 롤백을 위해 throw
                }

                inventoryItemsJson = JSON.stringify(
                    inventoryRaw.map(e => ({
                        itemId: e.item_id,
                        quantity: e.quantity,
                        inventorySlotId: e.slot_index - INVENTORY_SLOT_MIN  // 상대 인덱스 0~24
                    }))
                );

                equipmentItemsJson = JSON.stringify(
                    equipmentRaw.map(e => ({
                        itemId: e.item_id,
                        equipmentSlotId: e.slot_index - LOADOUT_SLOT_MIN    // 상대 인덱스 0~2
                    }))
                );

                await conn.commit();

            } catch (txError) {
                // 커스텀 에러 처리 및 롤백
                await conn.rollback().catch(() => {});
                
                if (txError.message === "ERR_SNAPSHOT_MISMATCH") {
                    return res.status(409).json(makeResponse(false, 409, null, { message: "인벤토리 스냅샷이 일치하지 않습니다.", code: "ERR_SNAPSHOT_MISMATCH" }));
                }
                if (txError.message === "ERR_NO_WEAPON_EQUIPPED") {
                    return res.status(400).json(makeResponse(false, 400, null, { message: "무기를 최소 하나 장착해야 합니다.", code: "ERR_NO_WEAPON_EQUIPPED" }));
                }
                
                throw txError; // 예상치 못한 DB 에러는 최상단 catch로 넘김
            } finally {
                conn.release();
            }
        }

        // 중복 매칭 요청 방지: 유저당 하나의 활성 티켓만 허용 (SET NX는 원자적)
        const activeMatchKey = `active_match:${db_id}`;
        const ticketId = "ticket_" + crypto.randomUUID();
        const lockAcquired = await redisClient.set(activeMatchKey, ticketId, { NX: true, EX: 300 });
        if (!lockAcquired) {
            return res.status(409).json(makeResponse(false, 409, null, {
                message: "이미 매칭 중입니다.",
                code: "ERR_ALREADY_IN_MATCH"
            }));
        }

        // 유효성 검사 통과 시 Redis에 매칭 티켓 발급
        try {
            await redisClient.hSet(ticketId, {
                uid: db_id,
                user_id: user_id,
                rating: rating.toString(),
                aggression: aggression.toString(),
                loadout_type: loadoutType,
                status: "WAITING",
                map_id: mapId.toString(),
                character_type: characterType.toString(),
                inventory_items: inventoryItemsJson,
                equipment_items: equipmentItemsJson,
            });
            await redisClient.expire(ticketId, 300);
        } catch (ticketError) {
            // 티켓 생성 실패 시 락을 즉시 해제해서 유저가 재시도 가능하게 함
            await redisClient.del(activeMatchKey);
            throw ticketError;
        }

        // TODO : 빌드할 때 로그 지워야함
        console.log(`매치 테스트 1 - O : 최초 Redis티켓 생성 ID: ${user_id}, Ticket: ${ticketId}, Inventory: ${inventoryItemsJson}, Equipment: ${equipmentItemsJson}`);

        sendHttpMatchMake(ticketId);

        res.status(200).json(makeResponse(true, 200, { ticketId }));
    } catch (error) {
        console.error("매치 테스트 1 - X : 최초 매치 요청 처리부분에서 에러.", error);
        
        if (db_id) {
            await redisClient.del(`active_match:${db_id}`).catch(() => {});
        }

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
            // 클라이언트가 토큰을 수신했으므로 ticket TTL을 60초로 단축 (재전송 여유)
            await redisClient.expire(ticketId, 60);
            return res.status(200).json(makeResponse(true, 200, {
                status: "SUCCESS",
                roomToken: ticketData.token,
                mapId: parseInt(ticketData.map_id, 10)
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
        console.log(`매치 테스트 11-2 : [HTTPS 프로세스] ip, port, sKey, sId를 클라이언트에 전달 ${tokenData.udp_server_ip}, ${parseInt(tokenData.port, 10)}, ${tokenData.security_key}, ${parseInt(tokenData.session_id, 10)}`);

        if (tokenData.loadout_type === 'CUSTOM') {
            const uid = parseInt(req.sessionData.db_id, 10);
            await pool.query(
                'DELETE FROM user_inventory WHERE uid = ? AND slot_index BETWEEN ? AND ?',
                [uid, INVENTORY_SLOT_MIN, LOADOUT_SLOT_MAX]
            );
        }

        return res.status(200).json(makeResponse(true, 200, {
            ip: tokenData.udp_server_ip,
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
            // WAITING 티켓을 직접 삭제함 → active_match 정리 + C++ 취소 IPC 전송
            console.log(`매치 취소 테스트 1 - O : Ticket: ${ticketId}에 대응하는 Redis의 티켓 파기`);
            await redisClient.del(`active_match:${db_id}`);
            sendHttpMatchMakeCancel(ticketId);
            return res.status(200).json(makeResponse(true, 200, { message: "매칭이 취소되었습니다." }));

        } else if (result === 2) {
            // 티켓이 이미 없음 (만료) → active_match만 정리, IPC는 보내지 않음
            console.log(`매치 취소 테스트 1 - O : Ticket: ${ticketId} 이미 없음 (만료), active_match만 정리`);
            await redisClient.del(`active_match:${db_id}`);
            return res.status(200).json(makeResponse(true, 200, { message: "매칭이 취소되었습니다." }));

        } else if (result === 0) {
            // INPROGRESS / SUCCESS → 파기 불가
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