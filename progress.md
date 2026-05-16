# 진행 상황 정리 (2026-05-17 업데이트)

## 완료된 것들

### 클라이언트 패킷 핸들러 / GameRoom
- [x] (2026-05-16 #0) `C2DNotifyLoadingComplete` 핸들러 구현 — `PKT_ID_C2D_NOTIFY_LOADING_COMPLETE=16` 추가·`PKT_ID_MAX=17`로 증가. `Handle_C2D_NotifyLoadingComplete` 선언·등록·구현. CONNECTED 상태일 때만 INPLAY로 전환, 빈 메시지·응답 없음(Notify 패턴) (`enum.h`, `ClientPacketHandler.h/cpp`)
- [x] (2026-05-16 #1) `GameRoom::Update()` 순수 가상 함수 전환 — `virtual void Update() {}` → `= 0`으로 변경. `TestGameRoom`·`WinchesterGameRoom`에 `void Update() override` 추가. 향후 파생 클래스는 반드시 Update 구현 강제 (`GameRoom.h/cpp`)
- [x] (2026-05-16 #2) `TestGameRoom::Update()`에서 `D2CUpdatePlayerStates` 브로드캐스트 구현 — `GameRoom::FillPlayerStates()` 추가(`_playerObjects` 순회·`PlayerObject::FillState` 호출). `MakeD2CUpdatePlayerStatesUnreliable` 헬퍼 추가. Update()에서 INPLAY 세션별 개별 SendBuffer 생성·전송(헤더의 sessionId·seqNum·ack·signature가 세션마다 다르므로 단일 버퍼 공유 불가) (`GameRoom.h/cpp`, `ClientPacketHandler.h`)
- [x] (2026-05-16 #3) `GameRoom::Broadcast(SendBuffer*)` 제거 — 호출처 0개, UDP 헤더 세션별 고유값 문제·SendBuffer 수명 문제(use-after-free)로 구조적 사용 불가. 선언·구현 삭제 (`GameRoom.h/cpp`)

### 코드 품질 / 리팩토링
- [x] (2026-05-17 #2) RTO 상한 1000ms·RTT 하한 20ms 적용 — `GetRetransmitCandidates`에 `std::min(..., 1000u)` 추가, `UpdateRtt` 하한 10→20ms 변경 (`PlayerSession.cpp`)
- [x] (2026-05-16 #4) `DedicateMain.cpp` Redis 연결 코드 제거 — DedicateProcess는 Redis를 직접 사용하지 않고 MainProcess에 IPC로 위탁하므로 dead code. 환경변수 로드·`pRedis = new sw::redis::Redis(redis_url)` 삭제 (`DedicateMain.cpp`)
- [x] (2026-05-16 #5) 실행 프로세스 기준 디렉토리 재구조화 — `Matchmaker.h/cpp`를 `src/DedicateProcess/`→`src/`로 이동. `DediSessions.h/cpp`에서 Main 전용 클래스(`M2DSession`, `M2DTempSession`)를 `src/M2DSessions.h/cpp`로 분리. `DediSessions`는 Dedicate 전용(`D2MSession`, `D2CSession`)만 보유. include 경로 갱신(`DediManager.h`, `PacketHandler.cpp`, `main.cpp`, `CMakeLists.txt`)
- [x] (2026-05-15 #0) 패킷 타입 재편성 — `UpdatePlayerState` → `PlayerState`로 이름 변경·`External_Unity_Object.proto`로 이동. `C2DUpdatePlayerState { PlayerState state = 1 }`로 래핑. `D2CUpdatePlayerStates { repeated PlayerState player_states = 1 }` 신규 추가(PktId 15). `enum.h`에 `PKT_ID_D2C_UPDATE_PLAYER_STATES = 15`·`PKT_ID_MAX = 16` 추가. **proto 재생성 필요: `bash Protocol/compileProto.sh`** (`External_Unity_Object.proto`, `External_Protocol.proto`, `enum.h`, `ClientPacketHandler.h/cpp`)
- [x] (2026-05-15 #1) `PlayerObject`에 `ApplyState`/`FillState` 추가 — `ApplyState(const PlayerState&)`: proto → C++ 개별 필드(position·rotation·state·pitch·velocity) 갱신. `FillState(PlayerState*)`: C++ 필드 → proto 직렬화(objectId 포함, `IsYFixed`에 따라 yaw/quaternion 분기). `Handle_C2D_UpdatePlayerState` 내 개별 갱신 코드 → `pPlayerObj->ApplyState(state)` 한 줄로 교체 (`PlayerObject.h/cpp`, `ClientPacketHandler.cpp`)

### HTTPServer / 매치메이킹
- [x] (2026-05-17 #0) `/status` SUCCESS 시 `ticket_` TTL 60초 단축 — 클라이언트 토큰 수신 확인 후 `expire(ticketId, 60)`으로 재전송 여유 확보. `ticket_`+`token_`의 최종 파기는 기존대로 C++ DediManager(`BindClientIpToSession` IPC 처리 시 `del({tokenKey, ticketKey})`)에서 일괄 수행. `/connect`에서의 중복 삭제는 IPC 실패 시 불일치 상태 방지를 위해 제외 (`HTTPServer/routes/match.js`)
- [x] (2026-05-17 #1) `D2MUpdateEntryToken` → Redis 키 라이프사이클 검토 완료 — `token_` 해시의 `ticket` 필드(back-reference)는 DediManager에서 cascade 삭제에 사용 중이므로 유지 확정. 삭제 책임은 C++ MainProcess 단일 지점에 집중

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
0. **[현재] HeartBeat / RequestBlueprint / RequestSpawnMe 클라이언트 연동 테스트 필요** — 프로토콜·직렬화 수정 완료, 빌드 통과. 클라이언트 측 구현 필요
    - `[DROP] 서명 불일치` 출력 → 클라이언트 서명 계산 로직 점검
    - `[DROP] unreliable 시퀀스 중복` 출력 → 클라이언트 uSeqNum 증가 로직 점검
    - StaticObjects 역직렬화 시 `TransformInfo` 구조 변경 반영 여부 클라이언트 측 확인 필요
    - `TestGameRoom::InitTestGameRoom()`에서 TestItemBox 등록 → Blueprint 응답(StaticObjects 청크)에 포함되는지 확인
1. `GameRoom::Update()` 추가 게임 로직 구현 — `TestGameRoom::Update()`의 PlayerState 브로드캐스트 완료. `WinchesterGameRoom::Update()` 로직 미구현(빈 함수). AI, 이벤트 등 추가 게임 루프 로직 필요 (`GameRoom.h/cpp`)
    - 새 플레이어 스폰 시 `D2CSpawnPlayerObject`를 다른 INPLAY 세션에게 개별 전송하는 로직 미구현 — `Handle_C2D_RequestSpawnMe`에서 세션별 SendBuffer 생성·전송 방식 필요 (기존 `Broadcast` 제거됨)
2. /connect요청을 통해서 ip와 port를 받았을 경우 동작 플로우 구현
    1. workerThread를 살려내고 루프 작동. (HeartBeat 작동)
        - workerThread내에서 ReliableFlag로 C2DHeartBeat전송, D2CHeartBeat로 응답 받음.
        - **서버 측 `Handle_C2D_HeartBeat` 완료. 클라이언트 측 루프 구현 필요.**
    2. Scene을 LoadingScene으로 변경하고, GameScene의 비동기 로딩 시작.
    3. 비동기 로딩 완료되었을 경우, GameScene의 현재 정적인 내용을 요구하는 패킷 전송.
        - C2DRequestBluePrint 전송
    4. 3의 패킷의 응답을 받았을 경우, 해당 내용을 역직렬화해서 보관하고 Scene교체 진행.
        - **서버 측 `Handle_C2D_RequestBlueprint` 완료** (`D2CResponseBlueprintStaticObjects` 청크만 전송, 스폰위치는 제거됨). 클라이언트 측 수신·역직렬화 구현 필요.
    5. 교체된 Scene의 Init() 함수에서 C2DRequestBluePrint에서 받아온 친구들 까지 포함해서 그려냄
    6. Init함수가 실행된 이후, 서버에 Scene 로딩 완료됬음을 알려줌과 동시에 동적인 정보를 다시 요청.
        - C2DRequestSpawnMe → `D2CResponseSpawnMeSpawnSpot`(스폰위치 + `character_type` + `object_id`) + `D2CResponseSpawnMeDynamicObjects`(동적 오브젝트 청크) 두 패킷으로 응답
        - **서버 측 `Handle_C2D_RequestSpawnMe` 구현 완료 (`character_type`·`object_id` 포함, `PlayerObject` 생성·GameRoom 등록·`objectId` 저장 포함). `bash Protocol/compileProto.sh` 재생성 후 빌드 필요. 클라이언트 측 수신·역직렬화 구현 필요.**
        - `C2DRequestSpawnPlayerObjects` 서버 핸들러 구현 완료(`D2CSpawnPlayerObjects` 단일 패킷 응답). 클라이언트 측 요청·수신 구현 필요.
        - **미구현: `Handle_C2D_RequestSpawnMe`에서 `GameRoom::RegisterPlayerSession` 미호출** — 브로드캐스트 기능 구현 전 반드시 연결 필요
3. 서버 RUDP 작동 검증
    - 헤더 크기 확인: `static_assert(sizeof(UDPHeader) == 35, ...)` (이미 ClientPacketHandler.h에 추가됨)
    - 에코 테스트: `C2DChannelOpen` → `D2CResponseChannelOpen` 흐름이 여전히 동작하는지 확인 (unreliable 경로)
    - reliable 경로 테스트: 테스트 패킷 하나를 FLAG_RELIABLE로 전송 → `_pendingReliable`에 등록되는지 확인
    - ACK 처리 확인: 클라이언트가 응답 패킷 전송 → `_pendingReliable`에서 해당 seqNum 제거되는지 확인
    - 재전송 확인: 클라이언트 ACK 없이 100ms 경과 → `CheckRetransmits()`가 재전송 패킷 송신하는지 로그 확인
4. **PlayerSession을 풀에 반납할 때 반드시 ACK Bitfield 관련 멤버변수를 초기화할 것**
5. `DisconnectSession` 구현 — MAX_RETRY 초과 시 세션 강제 종료 (`DediServerService.cpp` TODO)


### 진행 고려사항

---
