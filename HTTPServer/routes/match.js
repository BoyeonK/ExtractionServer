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

// DBProxyRequest.h 의 ACTIVE_MATCH_TTL_SEC 와 같아야 한다
const ACTIVE_MATCH_TTL_SEC = 900;

// equipmentSlotId: 0=주무기, 1=보조무기, 2=방어구 / inventorySlotId: 0~24 상대 인덱스
// 탄약 90발 중 30발(maxAmmo)은 입장 시 LoadMagazineFromInventory 가 탄창으로 옮긴다
const FREE_LOADOUT_PRESETS = [
    // 0 : AK-47(주) + M4A1(부) + 경량조끼
    {
        inventory: [
            { itemId: 6, quantity: 90, inventorySlotId: 0 },
            { itemId: 5, quantity: 90, inventorySlotId: 1 }
        ],

        equipment: [
            { itemId: 1, equipmentSlotId: 0 },
            { itemId: 2, equipmentSlotId: 1 },
            { itemId: 4, equipmentSlotId: 2 }
        ]
    },
    // 1 : M4A1(주) + AK-47(부) + 경량조끼
    {
        inventory: [
            { itemId: 5, quantity: 90, inventorySlotId: 0 },
            { itemId: 6, quantity: 90, inventorySlotId: 1 }
        ],

        equipment: [
            { itemId: 2, equipmentSlotId: 0 },
            { itemId: 1, equipmentSlotId: 1 },
            { itemId: 4, equipmentSlotId: 2 }
        ]
    },
];

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

        -- 티켓이 없음 (만료됐거나 이미 처리됨)
        --
        -- 티켓 TTL(300초)이 락 TTL(900초)보다 짧아 "티켓은 없는데 락은 남은" 구간이 생긴다.
        -- 그 구간이 두 가지 경우로 갈리는데 아래 상태 검사로는 구분할 수 없다 (티켓이 없으니까).
        --   ① 매칭이 성사되지 않고 티켓만 만료 → 락을 풀어줘야 한다
        --   ② 게임이 300초를 넘겨 진행 중 → 락을 풀면 게임 중에 새 매칭을 걸 수 있다
        -- 그래서 게임 시작 시점에 Main 이 락 값을 'INGAME:<ticketId>' 로 덮어쓴다
        -- (UpdateEntryTokenRequest::Execute). 그 표식이 있으면 ②이므로 거부한다.
        if not ticketUid then
            local lockValue = redis.call('GET', 'active_match:' .. reqUid)
            if lockValue and string.sub(lockValue, 1, 7) == 'INGAME:' then
                return 3
            end
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
            const preset = FREE_LOADOUT_PRESETS[Math.floor(Math.random() * FREE_LOADOUT_PRESETS.length)];
            inventoryItemsJson = JSON.stringify(preset.inventory);
            equipmentItemsJson = JSON.stringify(preset.equipment);
        } else if (loadoutType === 'CUSTOM') {
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
                    throw new Error("ERR_SNAPSHOT_MISMATCH");
                }
                for (const [itemId, total] of dbTotals) {
                    if (snapTotals.get(itemId) !== total) {
                        throw new Error("ERR_SNAPSHOT_MISMATCH");
                    }
                }

                await conn.query(`DELETE FROM user_inventory WHERE uid = ?`, [db_id]);
                if (inventory.length > 0) {
                    const placeholders = inventory.map(() => '(?, ?, ?, ?)').join(', ');
                    const values = inventory.flatMap(e => [db_id, e.item_id, e.slot_index, e.quantity]);
                    await conn.query(
                        `INSERT INTO user_inventory (uid, item_id, slot_index, quantity) VALUES ${placeholders}`,
                        values
                    );
                }

                const inventoryRaw = inventory.filter(
                    e => e.slot_index >= INVENTORY_SLOT_MIN && e.slot_index < LOADOUT_SLOT_MIN
                );
                const equipmentRaw = inventory.filter(
                    e => e.slot_index >= LOADOUT_SLOT_MIN && e.slot_index <= LOADOUT_SLOT_MAX
                );

                // 슬롯 105=주무기, 106=보조무기 — 최소 1개는 WEAPON 이어야 한다
                const weaponSlotItems = equipmentRaw.filter(
                    e => e.slot_index === LOADOUT_SLOT_MIN || e.slot_index === LOADOUT_SLOT_MIN + 1
                );

                if (weaponSlotItems.length === 0) {
                    throw new Error("ERR_NO_WEAPON_EQUIPPED");
                }

                const weaponItemIds = weaponSlotItems.map(e => e.item_id);
                const [itemTypeRows] = await conn.query(
                    `SELECT item_id, item_type FROM items WHERE item_id IN (?)`,
                    [weaponItemIds]
                );
                const hasWeapon = itemTypeRows.some(row => row.item_type === 'WEAPON');

                if (!hasWeapon) {
                    throw new Error("ERR_NO_WEAPON_EQUIPPED");
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
                await conn.rollback().catch(() => {});
                
                if (txError.message === "ERR_SNAPSHOT_MISMATCH") {
                    return res.status(409).json(makeResponse(false, 409, null, { message: "인벤토리 스냅샷이 일치하지 않습니다.", code: "ERR_SNAPSHOT_MISMATCH" }));
                }
                if (txError.message === "ERR_NO_WEAPON_EQUIPPED") {
                    return res.status(400).json(makeResponse(false, 400, null, { message: "무기를 최소 하나 장착해야 합니다.", code: "ERR_NO_WEAPON_EQUIPPED" }));
                }
                
                throw txError;
            } finally {
                conn.release();
            }
        }

        // TTL 은 이탈 통보 유실에 대비한 백스톱 — 최대 게임 길이보다 길어야 한다.
        const activeMatchKey = `active_match:${db_id}`;
        const ticketId = "ticket_" + crypto.randomUUID();
        const lockAcquired = await redisClient.set(activeMatchKey, ticketId, { NX: true, EX: ACTIVE_MATCH_TTL_SEC });
        if (!lockAcquired) {
            return res.status(409).json(makeResponse(false, 409, null, {
                message: "이미 매칭 중입니다.",
                code: "ERR_ALREADY_IN_MATCH"
            }));
        }

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
            await redisClient.del(activeMatchKey);
            throw ticketError;
        }

        // TEMP : 빌드할 때 로그 지워야함
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

router.get('/status', requireAuth, async (req, res) => {
    const { ticketId } = req.query;

    if (!ticketId) {
        return res.status(400).json(makeResponse(false, 400, null, { message: "티켓 ID가 필요합니다." }));
    }

    try {
        const ticketData = await redisClient.hGetAll(ticketId);

        if (Object.keys(ticketData).length === 0) {
            return res.status(404).json(makeResponse(false, 404, null, { message: "존재하지 않거나 만료된 티켓입니다.", status: "EXPIRED" }));
        }

        if (ticketData.uid !== req.sessionData.db_id) {
            return res.status(403).json(makeResponse(false, 403, null, { message: "본인의 티켓이 아닙니다." }));
        }

        if (ticketData.status === "WAITING" || ticketData.status === "INPROGRESS") {
            return res.status(200).json(makeResponse(true, 200, { status: "WAITING" }));
            
        } else if (ticketData.status === "SUCCESS") {
            console.log(`매치 테스트 10 - O : /status요청에 의해 토큰 전송 Token: ${ticketData.token}`);
            // 토큰 수신 완료 — 재전송 여유만 남기고 TTL 단축
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

function getServerDetectedIp(req) {
    let ip = req.headers['x-forwarded-for'] || req.socket.remoteAddress;
    if (ip && ip.includes(',')) ip = ip.split(',')[0].trim();
    if (ip && ip.startsWith('::ffff:')) ip = ip.substring(7);
    return ip || "0.0.0.0";
}

router.post('/connect', requireAuth, async (req, res) => {
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

        } else if (result === 3) {
            // 티켓은 만료됐지만 락에 INGAME 표식이 있음 → 진행 중인 게임이다
            return res.status(409).json(makeResponse(false, 409, null, {
                message: "게임이 진행 중이어서 취소할 수 없습니다.",
                code: "ERR_MATCH_IN_PROGRESS"
            }));

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