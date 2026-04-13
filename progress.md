# 진행 상황 정리 (2026-04-11)

## 완료된 것들

### 인증 & 매치메이킹 (Node.js HTTPServer)
- [x] 회원가입 / 로그인 / 세션 관리 (`auth.js`)
- [x] 매치메이킹 시작 API (`POST /match/start`) — Redis 티켓 발급, active_match 락
- [x] 매치메이킹 상태 폴링 API (`GET /match/status`)
- [x] 매치메이킹 취소 API (`POST /match/cancel`) — Lua 스크립트 원자적 처리
- [x] 데디케이트 서버 접속 정보 교환 API (`POST /match/connect`) — roomToken → IP/Port/SecurityKey 응답
- [x] IPC 매니저 (Node.js ↔ Main C++ 간 IPC 소켓 통신)

### 매치메이커 (C++ Main Process)
- [x] MatchMaker 알고리즘 — aggression 버킷 기반 그룹 매칭
- [x] MatchTicket 구조체 (ticketId, uid, aggression, mapId, 대기 시간 측정)
- [x] 매칭 그룹 확정 및 상태 변경 (`VerifyAndSetMatchStatus`)
- [x] 최초 aggression 수치 기준값 = 7

### DedicateProcess 기반 구조
- [x] DedicateProcess 초기화 (Main C++ ↔ Dedi IPC 연결, UDP 소켓 bind)
- [x] IPC 세션 (`M2DSession`, `D2MSession`, `M2DTempSession`)
- [x] UDP 세션 (`D2CSession`) — io_uring 기반 비동기 recvfrom/sendto
- [x] UDP 패킷 헤더 파싱 (`UDPHeader`: 2B PacketId, 2B SessionId, 4B Sequence, 4B SecurityKey, 1B Flags)
- [x] PlayerSession 관리 — sessionId, securityKey, per-packet sequenceNum 검증
- [x] ClientPacketHandler 초기화 (`GClientPacketHandler` 테이블)
- [x] GameRoom 기본 구조 (mapId, PlayerSession 컨테이너)
- [x] H2M2DBindClientIpToSession 처리 — 클라이언트 IP를 PlayerSession에 바인딩

### 프로토콜 정의
- [x] UDP 패킷 에코 테스트 (`C2DTestPkt` / `D2CTestPkt`) — 기본 송수신 흐름 검증 완료
- [x] `Vector3` 메시지 타입 정의 (`External_Unity_Object.proto`)
- [x] IPC 프로토콜 (`IPC_HTTP.proto`, `IPC_Dedicate.proto`, `IPC_enum.proto`)

### 인프라
- [x] Redis 키 구조 확정 (`redis_keys.md`)
- [x] MySQL 스키마 (`schema.sql`)
- [x] CMake 빌드 설정
- [x] WSL 환경 UDP 처리 (루프백 불일치 → 노트북에서 서버 실행으로 해결)

---

## 진행 중 / 다음 할 것들

> 전체 흐름: **[로비]** 로그인 → 아이템 확인 → 매치메이킹 → **(room 할당)** → **[인게임]** UDP 접속 → External_Protocol 통신 → 게임 로직
>
> 현재는 로비 단계 마무리에 집중한다.

### 1순위 — 아이템 시스템 (로비 단계)
- [ ] 아이템 테이블 데이터 채우기 (DB)
- [x] 로그인 응답에 인벤토리 포함 (`auth.js`) — `{ item_id, quantity }` 배열. 아이템 메타(이름/설명)는 클라이언트 에셋에서 참조
- [x] `GET /api/items/inventory` 엔드포인트 — `items.js`로 분리, `/api/items`에 마운트 (기존 `/api/inventory`에서 경로 변경)

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
