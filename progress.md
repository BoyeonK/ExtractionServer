# 진행 상황 정리 (2026-05-05)

## 완료된 것들

### 인게임 UDP 통신
- [x] (2026-05-01 #3) Blueprint PKT_ID 분리 + PacketHandler 정리 — `D2C_RESPONSE_BLUEPRINT` → `_SPAWN_POINT(5)` / `_STATIC_OBJECTS(6)` 두 개로 분리, PKT_ID_MAX=9. .proto 문법 오류 수정, MakeD2C 임시 구현 주석처리 (`enum.h`, `External_Protocol.proto`, `ClientPacketHandler.h/cpp`)
- [x] (2026-05-01 #4) `PlayerSession::Send()` + `SessionState` 추가 + 빌드 수정 — `Send(SendBuffer*)` 구현(`pDediServer->Send(buffer, GetAddress())`), `SessionState` enum(INIT/CONNECTED) 및 `_sessionState` 멤버 추가 (`PlayerSession.h/cpp`, `ClientPacketHandler.h/cpp`)
- [x] (2026-05-01 #5) `Handle_C2D_HeartBeat` / `Handle_C2D_RequestBlueprint` 핸들러 구현 — HeartBeat: CONNECTED 상태 확인 후 D2CHeartBeat unreliable 응답. RequestBlueprint: GameRoom에서 `SetSpawnSpot` + `FillStaticObjects` 호출 후 SpawnPoint·StaticObjects reliable 전송. `MakeD2CHeartBeat`, `MakeD2CResponseBlueprintSpawnPoint`, `MakeD2CResponseBlueprintStaticObjects` 헬퍼 추가, `Init()`에 핸들러 등록 (`ClientPacketHandler.h/cpp`, `enum.h`)
- [x] (2026-05-02 #0) `MakeD2CHeartBeat` 패킷 ID 버그 수정 + 수신 드롭 디버그 로그 추가 — 응답 ID가 `PKT_ID_D2C_RESPONSE_CHANNEL_OPEN(1)` → `PKT_ID_D2C_HEART_BEAT(3)`으로 수정. `HandleClientPacket()` 내 서명 불일치·시퀀스 중복 감지 시 `[DROP]` 로그 출력하여 서버 vs 클라이언트 원인 판별 가능하게 함 (`ClientPacketHandler.h`)

### 게임 오브젝트 / GameRoom
- [x] (2026-05-01 #1) TimerExecuter 개선 — delay 타입을 `std::chrono::duration` → `uint32_t`(ms 정수)로 단순화, `priority_queue` → `vector + push_heap/pop_heap`으로 교체(원소 직접 접근 가능) (`TimerExecuter.h/cpp`)
- [x] (2026-05-01 #2) GameRoom Blueprint 직렬화 + UnityGameObject 확장 — `FillStaticObjects()` 추가(StaticObject 목록을 MTU 1024B 이하 청크로 분할해 `D2CResponseBlueprintStaticObjects` 벡터로 직렬화), `UnityGameObject` 구조체 필드 확장 (`GameRoom.h/cpp`, `UnityGameObject.h/cpp`, `External_Unity_Object.proto`)
- [x] (2026-05-04 #0) 회전값 직렬화 방식 변경 — `TransformInfo` 메시지 추가, `position + front` → `position + oneof rotation(compressed_quat | yaw_angle)` 구조로 변경. `UnityGameObject`·`GameObjectMovementInfo` 둘 다 `TransformInfo`로 통일 (`External_Unity_Object.proto`)
- [x] (2026-05-05 #0) `ObjectType` enum class 적용 및 signed 직렬화 — `ObjectType : int16_t` enum class 추가(None=-1, Player=0, TestItemBox=1), `UnityGameObject::objectType` 타입 변경, proto에 sint32 적용 (`UnityGameObject.h/cpp`, `ClientPacketHandler.h`, `External_Unity_Object.proto`)
- [x] (2026-05-05 #1) `Quaternion` 구현체 정의 및 직렬화/역직렬화 메서드 추가 — `Quaternion` 구조체 정의, `compressed_quat` 방식 직렬화/역직렬화 메서드 구현 (`UnityGameObject.h/cpp`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
0. **[현재] HeartBeat / RequestBlueprint 디버그 진행 중** — 직렬화 수정(TransformInfo, ObjectType, Quaternion) 완료. 재빌드 후 클라이언트 연동 테스트 필요
    - `[DROP] 서명 불일치` 출력 → 클라이언트 서명 계산 로직 점검
    - `[DROP] unreliable 시퀀스 중복` 출력 → 클라이언트 uSeqNum 증가 로직 점검
    - 아무 출력 없음 → 패킷이 서버에 미도달 (포트/네트워크 문제)
    - StaticObjects 역직렬화 시 `TransformInfo` 구조 변경 반영 여부 클라이언트 측 확인 필요
1. `GameRoom::Update()` 가상 메서드 구현 연결 — `virtual void Update() {}` 추가됨, 각 맵 GameRoom 서브클래스에서 게임 루프 훅 구현 필요 (`GameRoom.h`)
2. /connect요청을 통해서 ip와 port를 받았을 경우 동작 플로우 구현
    1. workerThread를 살려내고 루프 작동. (HeartBeat 작동)
        - workerThread내에서 ReliableFlag로 C2DHeartBeat전송, D2CHeartBeat로 응답 받음.
        - **서버 측 `Handle_C2D_HeartBeat` 완료. 클라이언트 측 루프 구현 필요.**
    2. Scene을 LoadingScene으로 변경하고, GameScene의 비동기 로딩 시작.
    3. 비동기 로딩 완료되었을 경우, GameScene의 현재 정적인 내용을 요구하는 패킷 전송.
        - C2DRequestBluePrint 전송
    4. 3의 패킷의 응답을 받았을 경우, 해당 내용을 역직렬화해서 보관하고 Scene교체 진행.
        - **서버 측 `Handle_C2D_RequestBlueprint` 완료** (`D2CResponseBlueprintSpawnPoint` + `D2CResponseBlueprintStaticObjects` 청크 전송). 클라이언트 측 수신·역직렬화 구현 필요.
    5. 교체된 Scene의 Init() 함수에서 C2DRequestBluePrint에서 받아온 친구들 까지 포함해서 그려냄
    6. Init함수가 실행된 이후, 서버에 Scene 로딩 완료됬음을 알려줌과 동시에 동적인 정보를 다시 요청.
        - C2DRequestSpawnMe
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
