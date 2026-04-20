# 진행 상황 정리 (2026-04-20)

## 완료된 것들

### 아이템 시스템 스키마 & API 응답
- [x] (2026-04-20) `schema.sql`에 `shop_items` 테이블 추가 (`item_id`, `is_active`) — `items` 테이블 FK 참조
- [x] (2026-04-20) `config/shopCache.js` 신규 생성 — 서버 시작 시 `shop_items` JOIN `items` 캐싱, Map value `{ isActive, price }`
- [x] (2026-04-20) `POST /api/items/purchase` — 캐시로 판매 여부 검증, `ERR_ITEM_NOT_FOR_SALE`(403) 추가 (`items.js`)
- [x] (2026-04-20) 로그인 응답에 `shopItems` 추가, 게스트 로그인은 미포함 (`auth.js`)
- [x] (2026-04-20) `http-api-spec.yaml` — `AuthData`에 `shopItems` 필드, `/purchase`에 403 응답 추가
- [x] (2026-04-20) `shopCache.js` — `getActiveShopItems()` 추가, price 함께 캐싱 (JOIN 쿼리로 확장)
- [x] (2026-04-20) 로그인 응답 `shopItems` → `[{ item_id, price }]` 형태로 변경 (`auth.js`)
- [x] (2026-04-20) 구매 API — `SELECT price FROM items` DB 쿼리 제거, 캐시에서 price 읽기 (`items.js`)
- [x] (2026-04-20) `http-api-spec.yaml` — `ShopItem` 스키마 추가, `shopItems` 배열 타입 변경
- [x] (2026-04-20) `shopCache.js` — `item_type` 캐싱 추가, `WEAPON`/`ARMOR` 구매 시 수량 1 강제 검증 (`ERR_INVALID_QUANTITY` 400) 추가 (`items.js`)

---

## 진행 중 / 다음 할 것들

> 전체 흐름: **[로비]** 로그인 → 아이템 확인 → 매치메이킹 → **(room 할당)** → **[인게임]** UDP 접속 → External_Protocol 통신 → 게임 로직
>
> 현재는 로비 단계 마무리에 집중한다.

### 1순위 — Player 클래스 완성 (인게임 진입 준비)

### 2순위 — 인게임 단계
- [ ] `Player.h` — 좌표(`Vector3`), 체력, 상태 필드 추가
- [ ] 클라이언트와 맞춰보며 필드 확정 (실제 클라이언트 연결 후 조정)
- [ ] 인벤토리 — 아이템 테이블 확정 후, 인게임 메모리로서 아이템 관리하기

### 3순위 이후
- [ ] `External_Protocol.proto`에 실제 게임 패킷 추가 (이동, 총알 발사, 피격/체력 동기화)
- [ ] 추가된 패킷에 대응하는 `ClientPacketHandler` 핸들러 등록
- [ ] `/connect` 완료 후 `active_match:<db_id>` 값을 인게임 세션 연결 상태로 업데이트 (삭제 X, 재매칭 방지)
- [ ] 몬스터 어그로 시스템 (클라이언트 사이드 AI 예측 방식)

---

## 알려진 버그
- 없음

---

## 메모
- 총알 발사 및 플레이어 이동은 실제 클라이언트 연결 후 프로토콜 맞추면서 테스트 예정
- DedicateProcess 내 `_gameRooms` 컨테이너는 선언되어 있으나 방 생성/조회 로직은 아직 미완
