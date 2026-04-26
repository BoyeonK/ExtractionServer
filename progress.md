# 진행 상황 정리 (2026-04-26)

## 완료된 것들

### 아이템 시스템 스키마 & API 응답
- [x] (2026-04-20 #6) 구매 API — `SELECT price FROM items` DB 쿼리 제거, 캐시에서 price 읽기 (`items.js`)
- [x] (2026-04-20 #7) `http-api-spec.yaml` — `ShopItem` 스키마 추가, `shopItems` 배열 타입 변경
- [x] (2026-04-20 #8) `shopCache.js` — `item_type` 캐싱 추가, `WEAPON`/`ARMOR` 구매 시 수량 1 강제 검증 (`ERR_INVALID_QUANTITY` 400) 추가 (`items.js`)

### 매치메이킹
- [x] (2026-04-23 #0) `POST /api/game/match/start` 재설계 — `inventory` 전체 스냅샷 수신, DB 대조·갱신 후 warehouse(slot 0~79) 제외한 loadout 추출 (`match.js`, `http-api-spec.yaml`)
- [x] (2026-04-26 #0) `/start` API `mapId` 유효성 검사 추가 — `VALID_MAP_IDS = new Set([0, 1])`, 유효하지 않으면 400 `ERR_INVALID_MAP_ID` 반환, `(mapId ?? 0)` 묵시적 처리 제거 (`match.js`)
- [x] (2026-04-26 #1) `MAP_WINCHESTER = 1` 추가 (`DediManager.h`) — `MAP_MAX = 2`로 자동 확장, mapId 1 매치메이킹 라우팅 가능
- [x] (2026-04-26 #2) `http-api-spec.yaml` — `GameReadyRequest.mapId`에 `enum: [0, 1]` 및 400 응답에 `ERR_INVALID_MAP_ID` 명세 추가
- [x] (2026-04-26 #3) Redis 티켓 `items` 필드를 `inventory_items`(slot 80~104, 상대 인덱스 `inventorySlotId` 0~24, quantity 포함)와 `equipment_items`(slot 105~107, 상대 인덱스 `equipmentSlotId` 0~2, quantity 없음)로 분리 (`match.js`)
- [x] (2026-04-26 #4) Redis 키 변경이 C++ 세션 생성에 영향 없음 검증 — `PacketHandler.cpp`는 `uid`/`aggression`/`map_id`만 읽어 `inventory_items`/`equipment_items` 무관. `PlayerSession._inventory`는 `/connect` 단계에서 채워야 함
- [x] (2026-04-26 #5) `/cancel` Lua 반환값 분리 — 티켓 없음 시 `return 2`로 분리, SUCCESS 티켓 만료 후 취소 요청 시 `sendHttpMatchMakeCancel` IPC가 잘못 전송되던 버그 수정 (`match.js`)

---

## 진행 중 / 다음 할 것들

> 전체 흐름: **[로비]** 로그인 → 아이템 확인 → 매치메이킹 → **(room 할당)** → **[인게임]** UDP 접속 → External_Protocol 통신 → 게임 로직
>
> 현재는 로비 단계 마무리에 집중한다.

### 1순위 - '/status' API 응답 완성하기
- [ ] Equipment와 Inventory 구분이 없음. redis_keys.md 참고하여 두 가지를 구분할 수 있도록 flow 재설계하기.

### 2순위 - '/connect' API 응답 flow 완성하기
- [ ] Bitfield ACK가 가능한 구조로 UDP패킷 재설계하기
- [ ] Bitfield ACK구현하기
- [ ] DedicateProcess에 Player객체 할당하고, 최초 WelcomePacket 기다리기.
- [ ] WelcomePacket을 받았을 경우, Scene에서 다루어야 할 Object정보를 넘겨주기. (서버의 GameRoom과 클라이언트 Scene의 동기화 진행)
- [ ] `PacketHandler.cpp`에서 티켓의 `inventory_items`/`equipment_items` JSON 파싱 후 `PlayerSession._inventory` 초기화하기
- [ ] 이 플레이어의 매칭에 사용됬던 Redis의 ticket 및 token 파기하기.
- [ ] Client의 동기화가 제대로 되었는지 검증하기

---

