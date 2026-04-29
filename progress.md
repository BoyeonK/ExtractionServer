# 진행 상황 정리 (2026-04-29)

## 완료된 것들

### 매치메이킹
- [x] (2026-04-26 #7) `http-api-spec.yaml` — `/api/items/inventory` 태그 `Auth` → `Items` 오탈자 수정

### 인게임 UDP 통신
- [x] (2026-04-27 #0) Bitfield ACK 기반 RUDP 구조 전환 — UDPHeader 29B 교체, 전역 단일 시퀀스(`_sendSeq`), ACK 피기백, `PendingPacket` 재전송 큐, EWMA RTT 추정 (`PlayerSession.h/cpp`, `ClientPacketHandler.h`)
- [x] (2026-04-27 #1) `CheckRetransmits` 50ms 2-phase 분할 처리 — `_retransmitPhase` 토글로 짝수/홀수 인덱스 세션 교대 처리, 실질 주기 100ms (`DediServerService.h/cpp`, `DedicateMain.cpp`)
- [x] (2026-04-27 #2) reliable/unreliable 시퀀스 채널 분리 — `rSeqNum`(4B, reliable 전용, 비트필드 ACK 추적), `uSeqNum`(2B, unreliable 전용, signed 차 비교 dedup), UDPHeader 29B→31B (`ClientPacketHandler.h`, `PlayerSession.h/cpp`)
- [x] (2026-04-28 #0) PendingPacket ObjectPool 완성 — 오타(`PendingPakcet`), 잘못된 풀 타입(512/1024분기), 추상 클래스 직접 인스턴스화, `memcpy` buf 복사, `_pendingReliable.emplace` TODO, `ReleaseThis()` 반환타입/호출 누락 전부 수정 (`PlayerSession.h/cpp`)
- [x] (2026-04-28 #1) RUDP 전체 검토 및 버그 수정 — `CheckRetransmits`의 `pending->data` 멤버 접근 오류(`allocSize`/`GetData()`로 수정), `PlayerSession::PendingPacket*` 스코프 오류, `PlayerSession` 소멸자 추가(세션 종료 시 `_pendingReliable` 전체 `ReleaseThis()`), 중복 `FLAG_HAS_ACK` 설정 정리 (`DediServerService.cpp`, `PlayerSession.h`, `ClientPacketHandler.h`)
- [x] (2026-04-29 #0) xxHash64 기반 UDPHeader 35B 전환 — `securityKey(4B)` 평문 필드 제거, `signature(uint64_t 8B)` MAC 필드 추가(헤더 맨 앞). 송수신 양방향에 xxHash64(전체패킷₀ + secKey) 서명 검증 적용 (`ClientPacketHandler.h`)
- [x] (2026-04-29 #1) HeartBeat/Blueprint/SpawnMe 패킷 등록 — `enum.h` PKT_ID 6개 추가(PKT_ID_MAX=8), `ClientPacketHandler::Init()` 핸들러 등록, MakeD2C 함수 3개 추가, 핸들러 구현(Blueprint·SpawnMe는 GameRoom 연동 TODO) (`enum.h`, `ClientPacketHandler.h/cpp`)
- [x] (2026-04-29 #2) `C2DTestPkt`/`D2CTestPkt` → `C2DChannelOpen`/`D2CResponseChannelOpen` 리네임 — proto PktId enum, 메시지명, PKT_ID 상수, 핸들러명, MakeD2C 함수명 전체 갱신 (`External_Protocol.proto`, `enum.h`, `ClientPacketHandler.h/cpp`)
- [x] (2026-04-29 #3) FLAG_HAS_ACK 상시 포함 정리 — 수신 측 `if (flags & FLAG_HAS_ACK)` 조건 제거(항상 `ProcessIncomingAck` 호출), `HasRRecv()` dead code 제거, ACK 필드·enum 주석 갱신 (`ClientPacketHandler.h`, `PlayerSession.h`)

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

### 진행 우선사항
1. /connect요청을 통해서 ip와 port를 받았을 경우 동작 플로우 구현
    1. workerThread를 살려내고 루프 작동. (HeartBeat 작동)
        - workerThread내에서 ReliableFlag로 C2DHeartBeat전송, D2CHeartBeat로 응답 받음.
    2. Scene을 LoadingScene으로 변경하고, GameScene의 비동기 로딩 시작.
    3. 비동기 로딩 완료되었을 경우, GameScene의 현재 정적인 내용을 요구하는 패킷 전송.
        - C2DRequestBluePrint 전송
    4. 3의 패킷의 응답을 받았을 경우, 해당 내용을 역직렬화해서 보관하고 Scene교체 진행.
        - D2CResponseBlueprint, 여기서 Spawn위치 결정됨.
    5. 교체된 Scene의 Init() 함수에서 C2DRequestBluePrint에서 받아온 친구들 까지 포함해서 그려냄
    6. Init함수가 실행된 이후, 서버에 Scene 로딩 완료됬음을 알려줌과 동시에 동적인 정보를 다시 요청.
        - C2DRequestSpawnMe
2. D2MUpdateEntryToken 로직 검토
3. 서버 RUDP 작동 검증
    - 헤더 크기 확인: `static_assert(sizeof(UDPHeader) == 35, ...)` (이미 ClientPacketHandler.h에 추가됨)
    - 에코 테스트: `C2DChannelOpen` → `D2CResponseChannelOpen` 흐름이 여전히 동작하는지 확인 (unreliable 경로)
    - reliable 경로 테스트: 테스트 패킷 하나를 FLAG_RELIABLE로 전송 → `_pendingReliable`에 등록되는지 확인
    - ACK 처리 확인: 클라이언트가 응답 패킷 전송 → `_pendingReliable`에서 해당 seqNum 제거되는지 확인
    - 재전송 확인: 클라이언트 ACK 없이 100ms 경과 → `CheckRetransmits()`가 재전송 패킷 송신하는지 로그 확인
4. 데디프로세스 메인루프의 '할일 없을 경우 sleep' 로직 검토
5. PlayerSession에 Send()를 따로 만드는 것을 검토
6. **PlayerSession을 풀에 반납할 때 반드시 ACK Bitfield 관련 멤버변수를 초기화할 것**
7. `DisconnectSession` 구현 — MAX_RETRY 초과 시 세션 강제 종료 (`DediServerService.cpp:179` TODO)


### 진행 고려사항
1. 패킷 난독화 로직 검토

---
