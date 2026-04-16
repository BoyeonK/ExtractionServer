# 진행 상황 정리 (2026-04-17)

## 완료된 것들

### 아이템 시스템 스키마 & API 응답
- [x] (2026-04-17) `user_inventory` 테이블에 `slot_index` 컬럼 추가 (`schema.sql`)
- [x] (2026-04-17) `users` 테이블에 `money` 컬럼 추가 (`schema.sql`)
- [x] (2026-04-17) 로그인·회원가입 응답에 `money` 반영, 인벤토리 응답에 `slot_index` 반영 (`auth.js`, `items.js`)
- [x] (2026-04-17) `http-api-spec.yaml` 명세 업데이트 (`AuthData.money`, `InventoryItem.slot_index`)

### 인프라
- [x] (2026-04-16) Redis 키 구조 확정 (`redis_keys.md`)
- [x] (2026-04-16) MySQL 스키마 (`schema.sql`)
- [x] (2026-04-16) CMake 빌드 설정
- [x] (2026-04-16) WSL 환경 UDP 처리 (루프백 불일치 → 노트북에서 서버 실행으로 해결)

---

## 진행 중 / 다음 할 것들

> 전체 흐름: **[로비]** 로그인 → 아이템 확인 → 매치메이킹 → **(room 할당)** → **[인게임]** UDP 접속 → External_Protocol 통신 → 게임 로직
>
> 현재는 로비 단계 마무리에 집중한다.

### 1순위 — 아이템 시스템 (로비 단계)
- [ ] 아이템 테이블 데이터 채우기 (DB)
- [x] 로그인 응답에 인벤토리 포함 (`auth.js`) — `{ item_id, slot_index, quantity }` 배열. 아이템 메타(이름/설명)는 클라이언트 에셋에서 참조
- [x] `GET /api/items/inventory` 엔드포인트 — `items.js`로 분리, `/api/items`에 마운트 (기존 `/api/inventory`에서 경로 변경)
- [x] 로그인·회원가입 응답에 `money` 포함

### 2순위 — Player 클래스 완성 (인게임 진입 준비)
- [ ] `Player.h` — 좌표(`Vector3`), 체력, 상태 필드 추가
- [ ] 클라이언트와 맞춰보며 필드 확정 (실제 클라이언트 연결 후 조정)
- [ ] 인벤토리 — 아이템 테이블 확정 후, 인게임 메모리로서 아이템 관리하기

### 3순위 이후 — 인게임 단계 (2순위 완료 후)
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
