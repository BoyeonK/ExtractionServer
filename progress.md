# 진행 상황 정리 (2026-04-27)

## 완료된 것들

### 매치메이킹
- [x] (2026-04-26 #1) `MAP_WINCHESTER = 1` 추가 (`DediManager.h`) — `MAP_MAX = 2`로 자동 확장, mapId 1 매치메이킹 라우팅 가능
- [x] (2026-04-26 #2) `http-api-spec.yaml` — `GameReadyRequest.mapId`에 `enum: [0, 1]` 및 400 응답에 `ERR_INVALID_MAP_ID` 명세 추가
- [x] (2026-04-26 #3) Redis 티켓 `items` 필드를 `inventory_items`(slot 80~104, 상대 인덱스 `inventorySlotId` 0~24, quantity 포함)와 `equipment_items`(slot 105~107, 상대 인덱스 `equipmentSlotId` 0~2, quantity 없음)로 분리 (`match.js`)
- [x] (2026-04-26 #4) Redis 키 변경이 C++ 세션 생성에 영향 없음 검증 — `PacketHandler.cpp`는 `uid`/`aggression`/`map_id`만 읽어 `inventory_items`/`equipment_items` 무관. `PlayerSession._inventory`는 `/connect` 단계에서 채워야 함
- [x] (2026-04-26 #5) `/cancel` Lua 반환값 분리 — 티켓 없음 시 `return 2`로 분리, SUCCESS 티켓 만료 후 취소 요청 시 `sendHttpMatchMakeCancel` IPC가 잘못 전송되던 버그 수정 (`match.js`)
- [x] (2026-04-26 #6) `requireAuth` 미들웨어를 `HTTPServer/middleware/auth.js`로 분리 — `match.js` 로컬 정의 제거, `items.js` 인라인 세션 검증 제거 후 양쪽에서 공유 참조
- [x] (2026-04-26 #7) `http-api-spec.yaml` — `/api/items/inventory` 태그 `Auth` → `Items` 오탈자 수정

### 인게임 UDP 통신
- [x] (2026-04-27 #0) Bitfield ACK 기반 RUDP 구조 전환 — UDPHeader 29B 교체, 전역 단일 시퀀스(`_sendSeq`), ACK 피기백, `PendingPacket` 재전송 큐, EWMA RTT 추정 (`PlayerSession.h/cpp`, `ClientPacketHandler.h`)
- [x] (2026-04-27 #1) `CheckRetransmits` 50ms 2-phase 분할 처리 — `_retransmitPhase` 토글로 짝수/홀수 인덱스 세션 교대 처리, 실질 주기 100ms (`DediServerService.h/cpp`, `DedicateMain.cpp`)
- [x] (2026-04-27 #2) reliable/unreliable 시퀀스 채널 분리 — `rSeqNum`(4B, reliable 전용, 비트필드 ACK 추적), `uSeqNum`(2B, unreliable 전용, signed 차 비교 dedup), UDPHeader 29B→31B (`ClientPacketHandler.h`, `PlayerSession.h/cpp`)

> **RUDP 설계 전제**
> - ACK는 모든 아웃고잉 패킷(unreliable 포함)에 피기백된다.
> - **unreliable 패킷(이동/입력 등)은 게임 루프 동안 항상 활발하게 송수신된다고 가정한다.**
>   이 가정 하에 ACK-only 패킷이나 heartbeat 없이도 reliable ACK 흐름이 보장된다.
>   unreliable이 침묵한 상태 = 세션 타임아웃 대상이므로 별도 처리 불필요.

---

## 진행 중 / 다음 할 것들

> 전체 흐름: **[로비]** 로그인 → 아이템 확인 → 매치메이킹 → **(room 할당)** → **[인게임]** UDP 접속 → External_Protocol 통신 → 게임 로직
>
> 현재는 로비 단계 마무리에 집중한다.

### 1순위 - '/connect' API 응답 flow 완성하기
- [ ] DedicateProcess에 Player객체 할당하고, 최초 WelcomePacket 기다리기.
- [ ] WelcomePacket을 받았을 경우, Scene에서 다루어야 할 Object정보를 넘겨주기. (서버의 GameRoom과 클라이언트 Scene의 동기화 진행)
- [ ] `PacketHandler.cpp`에서 티켓의 `inventory_items`/`equipment_items` JSON 파싱 후 `PlayerSession._inventory` 초기화하기
- [ ] 이 플레이어의 매칭에 사용됬던 Redis의 ticket 및 token 파기하기.
- [ ] Client의 동기화가 제대로 되었는지 검증하기

### 2순위 - 인게임 안정성
- [ ] `DisconnectSession` 구현 — `CheckRetransmits` MAX_RETRY(10회) 초과 시 세션 정리

---

