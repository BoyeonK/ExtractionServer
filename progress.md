# 진행 상황 정리 (2026-05-10 업데이트)

## 완료된 것들

### 게임 오브젝트 / GameRoom
- [x] (2026-05-04 #0) 회전값 직렬화 방식 변경 — `TransformInfo` 메시지 추가, `position + front` → `position + oneof rotation(compressed_quat | yaw_angle)` 구조로 변경. `UnityGameObject`·`GameObjectMovementInfo` 둘 다 `TransformInfo`로 통일 (`External_Unity_Object.proto`)
- [x] (2026-05-05 #0) `ObjectType` enum class 적용 및 signed 직렬화 — `ObjectType : int16_t` enum class 추가(None=-1, Player=0, TestItemBox=1), `UnityGameObject::objectType` 타입 변경, proto에 sint32 적용 (`UnityGameObject.h/cpp`, `ClientPacketHandler.h`, `External_Unity_Object.proto`)
- [x] (2026-05-05 #1) `Quaternion` 구현체 정의 및 직렬화/역직렬화 메서드 추가 — `Quaternion` 구조체 정의, `compressed_quat` 방식 직렬화/역직렬화 메서드 구현 (`UnityGameObject.h/cpp`)
- [x] (2026-05-05 #2) GameRoom Object등록 테스트 코드 및 폴더구조 변경 — `UnityGameObjects/` 서브폴더 신설, `UnityGameObject.h/cpp` 이동, `TestItemBox` 클래스 추가(`TestGameObjects.h/cpp`). `Spawn()` → `SpawnStaticObject()` / `SpawnDynamicObject()` 분리, `_nxtObjectId` + `GetNewObjectId()` 추가, `TestGameRoom::InitTestGameRoom()` 추가 (`GameRoom.h/cpp`, `UnityGameObjects/*`)
- [x] (2026-05-05 #3) 스폰 요청 시점에 스폰 위치 결정 및 프로토콜 변경 — Blueprint 응답에서 스폰위치 제거(`D2CResponseBlueprintSpawnPoint` 삭제). `D2C_RESPONSE_SPAWN_ME` → `_SPAWN_SPOT(7)` + `_DYNAMIC_OBJECTS(8)`로 분리, PKT_ID 재번호(`D2C_RESPONSE_BLUEPRINT_STATIC_OBJECTS`=5, `C2D_REQUEST_SPAWN_ME`=6). `SetSpawnSpot()` 인자 타입·`SpawnStaticObject/DynamicObject()`에 `UnityGameObject*` 인자 추가 (`External_Protocol.proto`, `GameRoom.h/cpp`, `ClientPacketHandler.h/cpp`, `enum.h`)
- [x] (2026-05-05 #4) `std::unordered_map` → `absl::flat_hash_map` 교체 — `GameRoom.h/cpp` `_staticObjects`·`_dynamicObjects` 맵 타입 변경, `CMakeLists.txt`에 absl 링크 추가
- [x] (2026-05-06 #0) 전처리기문 누락 수정 및 빌드 버그 수정 — `UnityGameObject.h` include guard 누락 수정, `CMakeLists.txt`·`GameRoom.cpp` 빌드 버그 수정

### 개발 환경
- [x] (2026-05-06 #1) CLAUDE.md 서브파일 통합 — `src/`, `src/DedicateProcess/`, `HTTPServer/` 의 CLAUDE.md를 루트 CLAUDE.md에 인라인 합산 후 서브파일 삭제. 토큰 과다 소모 방지

### 매치메이킹 / IPC
- [x] (2026-05-10 #0) PlayerSession 생성 시 플레이어 정보 전달 구현 — `IPC_Dedicate.proto`에 `PlayerInfo` 메시지 추가 및 `M2DMakeRoomForThisGroup`의 `ticket_id` → `repeated PlayerInfo player_infos` 교체. `MakeM2DMakeRoomForThisGroup`에서 Redis `hgetall`로 uid/user_id/rating/inventory_items/equipment_items 조회, character_type은 MatchTicket에서 직접 읽음. `PlayerSession` 생성자·private 필드·getter 6개 추가. `DediServerService::MakeRoomForThisGroup` 시그니처 및 구현을 `vector<PlayerInfo>` 기반으로 변경. `Handle_M2D_MakeRoomForThisGroup`도 player_infos 기반으로 수정 (`IPC_Dedicate.proto`, `DediSessions.h`, `PlayerSession.h/cpp`, `DediServerService.h/cpp`, `PacketHandler.cpp`)
- [x] (2026-05-10 #1) `character_type` 검증 방식 Set 화이트리스트로 통일 — `VALID_CHARACTER_TYPES = new Set([0, 1, 2])` 상수 추가, 범위 체크(`< 0 || > 2`) → `has()` 방식으로 교체. `map_id`와 동일한 패턴으로 일관성 확보 (`HTTPServer/routes/match.js`)

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
        - C2DRequestSpawnMe → `D2CResponseSpawnMeSpawnSpot`(스폰위치) + `D2CResponseSpawnMeDynamicObjects`(동적 오브젝트 목록) 두 패킷으로 응답
        - **서버 측 `Handle_C2D_RequestSpawnMe` 미구현**
3. D2MUpdateEntryToken 로직 검토
4. 서버 RUDP 작동 검증
    - 헤더 크기 확인: `static_assert(sizeof(UDPHeader) == 35, ...)` (이미 ClientPacketHandler.h에 추가됨)
    - 에코 테스트: `C2DChannelOpen` → `D2CResponseChannelOpen` 흐름이 여전히 동작하는지 확인 (unreliable 경로)
    - reliable 경로 테스트: 테스트 패킷 하나를 FLAG_RELIABLE로 전송 → `_pendingReliable`에 등록되는지 확인
    - ACK 처리 확인: 클라이언트가 응답 패킷 전송 → `_pendingReliable`에서 해당 seqNum 제거되는지 확인
    - 재전송 확인: 클라이언트 ACK 없이 100ms 경과 → `CheckRetransmits()`가 재전송 패킷 송신하는지 로그 확인
5. 데디프로세스 메인루프의 '할일 없을 경우 sleep' 로직 검토
6. **PlayerSession을 풀에 반납할 때 반드시 ACK Bitfield 관련 멤버변수를 초기화할 것**
7. `DisconnectSession` 구현 — MAX_RETRY 초과 시 세션 강제 종료 (`DediServerService.cpp:179` TODO)


### 진행 고려사항

---
