# 진행 상황 정리 (2026-05-14 업데이트 #2)

## 완료된 것들

### 매치메이킹 / IPC
- [x] (2026-05-12 #0) `/status` 응답에 `mapId` 필드 추가 — 매칭 성공(`SUCCESS`) 시 `ticketData.map_id`를 `parseInt`로 변환해 `mapId`로 포함. `MatchStatusData` 스키마에도 반영 (`match.js`, `http-api-spec.yaml`)

### 인프라 / 전역 변수
- [x] (2026-05-11 #3) DedicateProcess 전역 변수를 `DedicateGlobalVariable.h/cpp`로 통합 — `DedicateGlobalVariable.h/cpp` 신설(메인의 `GlobalVariable.h/cpp` 패턴 동일 적용). `pDediServer`, `pTimerExecuter` extern을 `DediServerService.h`, `TimerExecuter.h`에서 제거하고 `DedicateGlobalVariable`로 이전. `pItemDataManager(ItemDataManager*)` 신규 추가. `DedicateMain.cpp`에서 `pItemDataManager` 생성 및 `Init()` 호출(D3 단계). `ClientPacketHandler.h`, `PlayerSession.cpp`, `GameRoom.cpp` include 경로 정리 (`DedicateGlobalVariable.h/cpp`, `DedicateMain.cpp`, `DediServerService.h`, `TimerExecuter.h/cpp`, `ClientPacketHandler.h`, `PlayerSession.cpp`, `GameRoom.cpp`, `CMakeLists.txt`)

### 메인루프 / 스케줄링
- [x] (2026-05-12 #1) 데디프로세스 메인루프 sleep 로직 재설계 — `CheckRetransmits`(void→bool, 50ms 타이밍 체크 내부화), `Tick`(void→bool), `hasWork` 플래그로 세 작업 누적 후 모두 false일 때만 sleep. `lastRetransmit` 외부 타이밍 변수 제거 (`DediServerService.h/cpp`, `TimerExecuter.h/cpp`, `DedicateMain.cpp`)
- [x] (2026-05-13 #0) `GameRoom::Update()` 100ms 주기 분산 실행 — 방별 독립 타이머 + 랜덤 초기 오프셋(0~99ms)으로 피크 분산. `_roomAliveTokens(unordered_map<int32_t, shared_ptr<bool>>)` 추가, 방 생성 시 `ScheduleRoomUpdate()` 등록, 이후 100ms마다 재귀 재등록. `_roomAliveTokens.erase(roomId)` 호출만으로 타이머 자동 취소 (`DediServerService.h/cpp`)
- [x] (2026-05-13 #1) `GameRoom::Update()` 실행 방식을 재귀 타이머에서 메인루프 직접 실행으로 교체 — `ScheduleRoomUpdate()`, `_roomAliveTokens`, `weak_ptr` 재귀 타이머 삭제. `UpdateGameRooms()` 추가(25ms 주기, `roomId % 4` 페이즈 분산으로 방 하나당 실질 100ms 주기 유지). `DedicateMain` 메인루프에 호출 추가 (`DediServerService.h/cpp`, `DedicateMain.cpp`)

### 클라이언트 패킷 핸들러 / GameRoom
- [x] (2026-05-12 #2) `Handle_C2D_RequestSpawnMe` 구현 및 `D2CResponseSpawnMeDynamicObjects` 구조 변경 — 스폰 요청 시 `D2CResponseSpawnMeSpawnSpot`(스폰위치) + `D2CResponseSpawnMeDynamicObjects`(동적 오브젝트 청크) 두 패킷으로 응답. `D2CResponseSpawnMeDynamicObjects` proto에 `index`·`is_last` 추가·`ingame_objects` 필드 번호 3으로 변경. `GameRoom::FillDynamicObjects` 추가(979 byte 청크 분할). `MakeD2CResponseSpawnMeDynamicObjectsReliable` 헬퍼 추가. **proto 재생성 필요: `bash Protocol/compileProto.sh`** (`External_Protocol.proto`, `GameRoom.h/cpp`, `ClientPacketHandler.h/cpp`)
- [x] (2026-05-12 #3) `D2CResponseSpawnMeSpawnSpot`에 `character_type` 필드 추가 — proto에 `int32 character_type = 2` 추가. `Handle_C2D_RequestSpawnMe` 핸들러에서 `spawnSpotPkt.set_character_type(pSession->GetCharacterType())` 호출 추가. **proto 재생성 필요: `bash Protocol/compileProto.sh`** (`External_Protocol.proto`, `ClientPacketHandler.cpp`)
- [x] (2026-05-14 #0) `Handle_C2D_RequestSpawnMe`에서 `PlayerObject` 생성·`GameRoom` 등록·`objectId` 저장 — `Player._objectId(=-1)` 필드 및 getter/setter 추가. `PlayerSession`에 프록시 `GetObjectId`/`SetObjectId` 추가. `GameRoom::GetNewObjectId()` public 이동. 핸들러에서 spawn 위치로 `PlayerObject` 생성 후 `SpawnPlayerObject` 등록, `pSession->SetObjectId()` 호출 (`Player.h`, `PlayerSession.h`, `GameRoom.h`, `ClientPacketHandler.cpp`)
- [x] (2026-05-14 #1) `C2DRequestSpawnByObjectId` / `D2CResponseSpawnByObjectId` 추가 및 핸들러 구현 — 클라이언트가 누락된 오브젝트 재동기화 요청 시 서버가 `UnityGameObject` 단건 응답. proto에 `C2DRequestSpawnByObjectId(int32 object_id)` · `D2CResponseSpawnByObjectId(UnityGameObject)` 추가(PktId 9·10). `GameRoom::FindObject(uint32_t)` 추가(세 컨테이너 순차 탐색). 핸들러는 CONNECTED + 스폰 완료 검증 후 `FindObject` 결과가 없으면 응답 없이 `true` 반환(ACK로 클라이언트 재전송 종료). **proto 재생성 필요: `bash Protocol/compileProto.sh`** (`External_Protocol.proto`, `enum.h`, `GameRoom.h/cpp`, `ClientPacketHandler.h/cpp`)

### 코드 품질 / 리팩토링
- [x] (2026-05-14 #2) 지역변수 `unordered_map` → `absl::flat_hash_map` 교체 — 포인터 무효화 위험이 없는 단발성 지역변수 3곳 교체. `DediManager.h`·`PlayerSession.h` 멤버는 반복 중 erase/insert 패턴으로 보류 (`DediSessions.h`, `PacketHandler.cpp`, `RedisHandler.cpp`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
0. **[현재] HeartBeat / RequestBlueprint / RequestSpawnMe 클라이언트 연동 테스트 필요** — 프로토콜·직렬화 수정 완료, 빌드 통과. 클라이언트 측 구현 필요
    - `[DROP] 서명 불일치` 출력 → 클라이언트 서명 계산 로직 점검
    - `[DROP] unreliable 시퀀스 중복` 출력 → 클라이언트 uSeqNum 증가 로직 점검
    - StaticObjects 역직렬화 시 `TransformInfo` 구조 변경 반영 여부 클라이언트 측 확인 필요
    - `TestGameRoom::InitTestGameRoom()`에서 TestItemBox 등록 → Blueprint 응답(StaticObjects 청크)에 포함되는지 확인
1. `GameRoom::Update()` 게임 로직 구현 — 메인루프 직접 실행(25ms×4-phase) 등록 완료. `TestGameRoom`, `WinchesterGameRoom` 서브클래스에서 실제 게임 루프 로직(AI, 이벤트 등) 구현 필요 (`GameRoom.h/cpp`)
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
        - C2DRequestSpawnMe → `D2CResponseSpawnMeSpawnSpot`(스폰위치 + `character_type`) + `D2CResponseSpawnMeDynamicObjects`(동적 오브젝트 청크) 두 패킷으로 응답
        - **서버 측 `Handle_C2D_RequestSpawnMe` 구현 완료 (`character_type` 포함, `PlayerObject` 생성·GameRoom 등록·`objectId` 저장 포함). `bash Protocol/compileProto.sh` 재생성 후 빌드 필요. 클라이언트 측 수신·역직렬화 구현 필요.**
3. D2MUpdateEntryToken 로직 검토
4. 서버 RUDP 작동 검증
    - 헤더 크기 확인: `static_assert(sizeof(UDPHeader) == 35, ...)` (이미 ClientPacketHandler.h에 추가됨)
    - 에코 테스트: `C2DChannelOpen` → `D2CResponseChannelOpen` 흐름이 여전히 동작하는지 확인 (unreliable 경로)
    - reliable 경로 테스트: 테스트 패킷 하나를 FLAG_RELIABLE로 전송 → `_pendingReliable`에 등록되는지 확인
    - ACK 처리 확인: 클라이언트가 응답 패킷 전송 → `_pendingReliable`에서 해당 seqNum 제거되는지 확인
    - 재전송 확인: 클라이언트 ACK 없이 100ms 경과 → `CheckRetransmits()`가 재전송 패킷 송신하는지 로그 확인
5. **PlayerSession을 풀에 반납할 때 반드시 ACK Bitfield 관련 멤버변수를 초기화할 것**
6. `DisconnectSession` 구현 — MAX_RETRY 초과 시 세션 강제 종료 (`DediServerService.cpp` TODO)


### 진행 고려사항

---
