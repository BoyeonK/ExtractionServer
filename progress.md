# 진행 상황 정리 (2026-04-30)

## 완료된 것들

### 인게임 UDP 통신
- [x] (2026-04-28 #0) PendingPacket ObjectPool 완성 — 오타(`PendingPakcet`), 잘못된 풀 타입(512/1024분기), 추상 클래스 직접 인스턴스화, `memcpy` buf 복사, `_pendingReliable.emplace` TODO, `ReleaseThis()` 반환타입/호출 누락 전부 수정 (`PlayerSession.h/cpp`)
- [x] (2026-04-28 #1) RUDP 전체 검토 및 버그 수정 — `CheckRetransmits`의 `pending->data` 멤버 접근 오류(`allocSize`/`GetData()`로 수정), `PlayerSession::PendingPacket*` 스코프 오류, `PlayerSession` 소멸자 추가(세션 종료 시 `_pendingReliable` 전체 `ReleaseThis()`), 중복 `FLAG_HAS_ACK` 설정 정리 (`DediServerService.cpp`, `PlayerSession.h`, `ClientPacketHandler.h`)
- [x] (2026-04-29 #0) xxHash64 기반 UDPHeader 35B 전환 — `securityKey(4B)` 평문 필드 제거, `signature(uint64_t 8B)` MAC 필드 추가(헤더 맨 앞). 송수신 양방향에 xxHash64(전체패킷₀ + secKey) 서명 검증 적용 (`ClientPacketHandler.h`)
- [x] (2026-04-29 #1) HeartBeat/Blueprint/SpawnMe 패킷 등록 — `enum.h` PKT_ID 6개 추가(PKT_ID_MAX=8), `ClientPacketHandler::Init()` 핸들러 등록, MakeD2C 함수 3개 추가, 핸들러 구현(Blueprint·SpawnMe는 GameRoom 연동 TODO) (`enum.h`, `ClientPacketHandler.h/cpp`)
- [x] (2026-04-29 #2) `C2DTestPkt`/`D2CTestPkt` → `C2DChannelOpen`/`D2CResponseChannelOpen` 리네임 — proto PktId enum, 메시지명, PKT_ID 상수, 핸들러명, MakeD2C 함수명 전체 갱신 (`External_Protocol.proto`, `enum.h`, `ClientPacketHandler.h/cpp`)
- [x] (2026-04-29 #3) FLAG_HAS_ACK 상시 포함 정리 — 수신 측 `if (flags & FLAG_HAS_ACK)` 조건 제거(항상 `ProcessIncomingAck` 호출), `HasRRecv()` dead code 제거, ACK 필드·enum 주석 갱신 (`ClientPacketHandler.h`, `PlayerSession.h`)

### 게임 오브젝트 / GameRoom
- [x] (2026-04-30 #0) Unity의 GameObject와 Vector3를 표현할 구조체 생성 — 게임 오브젝트 기본 표현 구조체 정의
- [x] (2026-04-30 #1) Unity 객체 추상화 클래스에 직렬화 메서드 주입 및 필드 int32→uint32 변경 — 직렬화 인터페이스 정립, 부호 없는 타입으로 통일 (`GameObject.h/cpp`)
- [x] (2026-04-30 #2) GameObject 선언부 구현부 분리 및 GameRoom에 object 컨테이너 생성 — `.h`/`.cpp` 분리, `GameRoom`에 오브젝트 목록 컨테이너 추가 (`GameRoom.h/cpp`)
- [x] (2026-04-30 #3) TimerExecuter 구현 — 실행 시각 기반 함수자 스케줄러. 람다 직접 등록 + 멤버함수·Alive Token(`weak_ptr<bool>`) 등록 두 가지 API, min-heap으로 실행 순서 보장, 메인루프 `Tick()`으로 소비 (`TimerExecuter.h/cpp`, `DedicateMain.cpp`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
0. `GameRoom::Update()` 가상 메서드 구현 연결 — `virtual void Update() {}` 추가됨(미커밋), 각 맵 GameRoom 서브클래스에서 게임 루프 훅 구현 필요 (`GameRoom.h`)
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

---
