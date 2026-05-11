# 진행 상황 정리 (2026-05-12 업데이트)

## 완료된 것들

### 개발 환경
- [x] (2026-05-06 #1) CLAUDE.md 서브파일 통합 — `src/`, `src/DedicateProcess/`, `HTTPServer/` 의 CLAUDE.md를 루트 CLAUDE.md에 인라인 합산 후 서브파일 삭제. 토큰 과다 소모 방지

### 매치메이킹 / IPC
- [x] (2026-05-10 #0) PlayerSession 생성 시 플레이어 정보 전달 구현 — `IPC_Dedicate.proto`에 `PlayerInfo` 메시지 추가 및 `M2DMakeRoomForThisGroup`의 `ticket_id` → `repeated PlayerInfo player_infos` 교체. `MakeM2DMakeRoomForThisGroup`에서 Redis `hgetall`로 uid/user_id/rating/inventory_items/equipment_items 조회, character_type은 MatchTicket에서 직접 읽음. `PlayerSession` 생성자·private 필드·getter 6개 추가. `DediServerService::MakeRoomForThisGroup` 시그니처 및 구현을 `vector<PlayerInfo>` 기반으로 변경. `Handle_M2D_MakeRoomForThisGroup`도 player_infos 기반으로 수정 (`IPC_Dedicate.proto`, `DediSessions.h`, `PlayerSession.h/cpp`, `DediServerService.h/cpp`, `PacketHandler.cpp`)
- [x] (2026-05-10 #1) `character_type` 검증 방식 Set 화이트리스트로 통일 — `VALID_CHARACTER_TYPES = new Set([0, 1, 2])` 상수 추가, 범위 체크(`< 0 || > 2`) → `has()` 방식으로 교체. `map_id`와 동일한 패턴으로 일관성 확보 (`HTTPServer/routes/match.js`)
- [x] (2026-05-11 #2) `MakeRoomForThisGroup` 시그니처를 패킷 참조로 단순화 — `PacketHandler.cpp`에서 불필요한 `vector<PlayerInfo>` 복사 제거. `DediServerService::MakeRoomForThisGroup` 시그니처를 `(const IPC_Protocol::M2DMakeRoomForThisGroup& pkt)`로 변경하여 `pkt.map_id()`, `pkt.player_infos()` 직접 참조 (`DediServerService.h/cpp`, `PacketHandler.cpp`)
- [x] (2026-05-12 #0) `/status` 응답에 `mapId` 필드 추가 — 매칭 성공(`SUCCESS`) 시 `ticketData.map_id`를 `parseInt`로 변환해 `mapId`로 포함. `MatchStatusData` 스키마에도 반영 (`match.js`, `http-api-spec.yaml`)

### 플레이어 세션 / 데이터 구조
- [x] (2026-05-11 #0) 플레이어 데이터를 `Player` 클래스로 분리 — `Player.h/cpp` 재작성(생성자·getter 추가). `PlayerSession`에서 `_uid`, `_userId`, `_rating`, `_inventoryItems`, `_equipmentItems`, `_characterType` 6개 필드를 `Player _player` 멤버로 교체. getter 6개는 `_player`에 위임. `GameRoom.cpp`의 미사용 `#include "Player.h"` 제거 (`Player.h/cpp`, `PlayerSession.h/cpp`, `GameRoom.cpp`)
- [x] (2026-05-11 #1) PlayerInfo 아이템 데이터 ItemSlot protobuf 구조화 — `inventory_items`·`equipment_items`를 JSON string에서 `repeated ItemSlot`으로 교체. `Player` 생성자에서 `INVENTORY_SLOT_COUNT=25` 고정 초기화 및 `slotIndex` 기반 배치, equipment `quantity=1` 수정. `nlohmann/json` FetchContent 추가 (`IPC_Dedicate.proto`, `DediSessions.h`, `DediServerService.cpp`, `PlayerSession.h/cpp`, `Player.h/cpp`, `CMakeLists.txt`)

### 인프라 / 전역 변수
- [x] (2026-05-11 #3) DedicateProcess 전역 변수를 `DedicateGlobalVariable.h/cpp`로 통합 — `DedicateGlobalVariable.h/cpp` 신설(메인의 `GlobalVariable.h/cpp` 패턴 동일 적용). `pDediServer`, `pTimerExecuter` extern을 `DediServerService.h`, `TimerExecuter.h`에서 제거하고 `DedicateGlobalVariable`로 이전. `pItemDataManager(ItemDataManager*)` 신규 추가. `DedicateMain.cpp`에서 `pItemDataManager` 생성 및 `Init()` 호출(D3 단계). `ClientPacketHandler.h`, `PlayerSession.cpp`, `GameRoom.cpp` include 경로 정리 (`DedicateGlobalVariable.h/cpp`, `DedicateMain.cpp`, `DediServerService.h`, `TimerExecuter.h/cpp`, `ClientPacketHandler.h`, `PlayerSession.cpp`, `GameRoom.cpp`, `CMakeLists.txt`)

### 메인루프 / 스케줄링
- [x] (2026-05-12 #1) 데디프로세스 메인루프 sleep 로직 재설계 — `CheckRetransmits`(void→bool, 50ms 타이밍 체크 내부화), `Tick`(void→bool), `hasWork` 플래그로 세 작업 누적 후 모두 false일 때만 sleep. `lastRetransmit` 외부 타이밍 변수 제거 (`DediServerService.h/cpp`, `TimerExecuter.h/cpp`, `DedicateMain.cpp`)

### 클라이언트 패킷 핸들러 / GameRoom
- [x] (2026-05-12 #2) `Handle_C2D_RequestSpawnMe` 구현 및 `D2CResponseSpawnMeDynamicObjects` 구조 변경 — 스폰 요청 시 `D2CResponseSpawnMeSpawnSpot`(스폰위치) + `D2CResponseSpawnMeDynamicObjects`(동적 오브젝트 청크) 두 패킷으로 응답. `D2CResponseSpawnMeDynamicObjects` proto에 `index`·`is_last` 추가·`ingame_objects` 필드 번호 3으로 변경. `GameRoom::FillDynamicObjects` 추가(979 byte 청크 분할). `MakeD2CResponseSpawnMeDynamicObjectsReliable` 헬퍼 추가. **proto 재생성 필요: `bash Protocol/compileProto.sh`** (`External_Protocol.proto`, `GameRoom.h/cpp`, `ClientPacketHandler.h/cpp`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
0. **[현재] HeartBeat / RequestBlueprint / RequestSpawnMe 클라이언트 연동 테스트 필요** — 프로토콜·직렬화 수정 완료, 빌드 통과. 클라이언트 측 구현 필요
    - `[DROP] 서명 불일치` 출력 → 클라이언트 서명 계산 로직 점검
    - `[DROP] unreliable 시퀀스 중복` 출력 → 클라이언트 uSeqNum 증가 로직 점검
    - StaticObjects 역직렬화 시 `TransformInfo` 구조 변경 반영 여부 클라이언트 측 확인 필요
    - `TestGameRoom::InitTestGameRoom()`에서 TestItemBox 등록 → Blueprint 응답(StaticObjects 청크)에 포함되는지 확인
1. `GameRoom::Update()` 가상 메서드 구현 연결 — `virtual void Update() {}` 추가됨, 각 맵 GameRoom 서브클래스에서 게임 루프 훅 구현 필요 (`GameRoom.h`)
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
        - C2DRequestSpawnMe → `D2CResponseSpawnMeSpawnSpot`(스폰위치) + `D2CResponseSpawnMeDynamicObjects`(동적 오브젝트 청크) 두 패킷으로 응답
        - **서버 측 `Handle_C2D_RequestSpawnMe` 구현 완료. `bash Protocol/compileProto.sh` 재생성 후 빌드 필요. 클라이언트 측 수신·역직렬화 구현 필요.**
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
