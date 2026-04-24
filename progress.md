# 진행 상황 정리 (2026-04-23)

## 완료된 것들

### 아이템 시스템 스키마 & API 응답
- [x] (2026-04-20 00) `config/shopCache.js` 신규 생성 — 서버 시작 시 `shop_items` JOIN `items` 캐싱, Map value `{ isActive, price }`
- [x] (2026-04-20 00) `POST /api/items/purchase` — 캐시로 판매 여부 검증, `ERR_ITEM_NOT_FOR_SALE`(403) 추가 (`items.js`)
- [x] (2026-04-20 00) 로그인 응답에 `shopItems` 추가, 게스트 로그인은 미포함 (`auth.js`)
- [x] (2026-04-20 00) `http-api-spec.yaml` — `AuthData`에 `shopItems` 필드, `/purchase`에 403 응답 추가
- [x] (2026-04-20 00) `shopCache.js` — `getActiveShopItems()` 추가, price 함께 캐싱 (JOIN 쿼리로 확장)
- [x] (2026-04-20 00) 로그인 응답 `shopItems` → `[{ item_id, price }]` 형태로 변경 (`auth.js`)
- [x] (2026-04-20 00) 구매 API — `SELECT price FROM items` DB 쿼리 제거, 캐시에서 price 읽기 (`items.js`)
- [x] (2026-04-20 00) `http-api-spec.yaml` — `ShopItem` 스키마 추가, `shopItems` 배열 타입 변경
- [x] (2026-04-20 00) `shopCache.js` — `item_type` 캐싱 추가, `WEAPON`/`ARMOR` 구매 시 수량 1 강제 검증 (`ERR_INVALID_QUANTITY` 400) 추가 (`items.js`)
- [x] (2026-04-23 04) `POST /api/game/match/start` 재설계 — `inventory` 전체 스냅샷 수신, DB 대조·갱신 후 warehouse(slot 0~79) 제외한 loadout 추출 (`match.js`, `http-api-spec.yaml`)

---

## 진행 중 / 다음 할 것들

> 전체 흐름: **[로비]** 로그인 → 아이템 확인 → 매치메이킹 → **(room 할당)** → **[인게임]** UDP 접속 → External_Protocol 통신 → 게임 로직
>
> 현재는 로비 단계 마무리에 집중한다.

### 1순위 - `/start` API 응답 로직 완성하기
- [ ] mapId == 0이 아닌 경우 동작하지 않는 부분 해결하기
- [ ] inventory가 올바르게 전달되는지 재확인하기

### 2순위 - '/status' API 응답 완성하기
- [ ] 현재 Eqiupment와 Inventory 구분이 없음. redis_keys.md파일을 참고하여 두가지를 구분할 수 있도록 flow 재설계하기.

### 3순위 - '/connect' API 응답 flow완성하기
- [ ] Bitfield ACK가 가능한 구조로 UDP패킷 재설계하기
- [ ] Bitfield ACK구현하기
- [ ] DedicateProcess에 Player객체 할당하고, 최초 WelcomePacket 기다리기.
- [ ] WelcomePacket을 받았을 경우, Scene에서 다루어야 할 Object정보를 넘겨주기. (서버의 GameRoom과 클라이언트 Scene의 동기화 진행)
- [ ] 이 플레이어의 매칭에 사용됬던 Redis의 ticket 및 token 파기하기.
- [ ] Client의 동기화가 제대로 되었는지 검증하기

---

## 알려진 버그
- [ ]  Redis의 ticket의 status가 SUCCESS인 경우에 파기가 가능함. WAITING이 아닌 모든 경우에서 파기 불가능해야 함.

---

