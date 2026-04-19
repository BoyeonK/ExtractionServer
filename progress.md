# 진행 상황 정리 (2026-04-20)

## 완료된 것들

### 아이템 시스템 스키마 & API 응답
- [x] (2026-04-17) 로그인 응답에 인벤토리 포함 (`auth.js`) — `{ item_id, slot_index, quantity }` 배열. 아이템 메타(이름/설명)는 클라이언트 에셋에서 참조
- [x] (2026-04-17) 로그인·회원가입 응답에 `money` 포함
- [x] (2026-04-17) `POST /api/items/purchase` 구매 API 구현 — 인벤토리 스냅샷(item_id별 수량 합산) 검증, 트랜잭션으로 스냅샷 덮어쓰기 + 신규 슬롯 INSERT + money 차감 (`items.js`)
- [x] (2026-04-17) `items` 테이블에 `price INT UNSIGNED` 컬럼 추가 (`schema.sql`)
- [x] (2026-04-17) `http-api-spec.yaml`에 `POST /api/items/purchase` 명세 및 `PurchaseRequest`, `PurchaseData` 스키마 추가
- [x] (2026-04-20) `schema.sql`에 `shop_items` 테이블 추가 (`item_id`, `is_active`) — `items` 테이블 FK 참조
- [x] (2026-04-20) `config/shopCache.js` 신규 생성 — 서버 시작 시 `shop_items` 전체를 Map에 캐싱 (`loadShopCache`, `getShopItem`, `getActiveItemIds`)
- [x] (2026-04-20) `POST /api/items/purchase` — DB 조회 대신 캐시로 판매 여부 검증, `ERR_ITEM_NOT_FOR_SALE`(403) 추가 (`items.js`)
- [x] (2026-04-20) 로그인 응답에 `shopItems`(구매 가능 아이템 ID 리스트) 추가, 게스트 로그인은 미포함 (`auth.js`)
- [x] (2026-04-20) `http-api-spec.yaml` — `AuthData`에 `shopItems` 필드, `/purchase`에 403 응답 추가

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
