# 진행 상황 정리 (2026-07-07 업데이트)


## 완료된 것들

### 무기 시스템 / 장비

### 인벤토리 / Player 상태
- [x] (2026-07-06 #0) UnequipWeaponToSlot/UnequipWeaponToInventory 탄창 유실 버그 수정 — 모든 거부 가능한 검증(맨손 금지·타입 불일치)을 `UnloadMagazineToInventory` 이전으로 이동. dstSlot이 비어있지 않은데 무기가 아닌 경우에도 탄창 언로드 후 거부되던 문제 포함 (`PlayerInventory.cpp`)
- [x] (2026-07-06 #1) InteractContainerObject 동일 슬롯 조작 차단 — `startSlot == endSlot`(같은 object_id + 같은 slot_idx)일 때 get/swap/merge가 데이터 오염 또는 불필요한 버전 증가를 유발하던 문제. 포인터 비교로 조기 거부 추가 (`ClientPacketHandler.cpp`)
- [x] (2026-07-06 #2) InteractContainerObject 성공 시 `_firstEmptySlotIndex` 미갱신 수정 — get/merge로 플레이어 인벤토리 슬롯이 비거나 채워질 때 `_firstEmptySlotIndex`가 갱신되지 않아 이후 `UnloadMagazineToInventory`에서 탄창이 파기될 수 있던 문제. `UpdateFirstEmptySlotIndex()`를 public으로 이동 후 성공 경로에서 호출 추가 (`PlayerInventory.h`, `ClientPacketHandler.cpp`)
- [x] (2026-07-07 #0) PlayerState에 action_state 필드 추가 반영 — proto에 `uint32 action_state = 4` 추가(0=NONE, 1=SHOOTING). `PlayerObject`에 `actionState` 멤버 추가, `ApplyState()`/`FillState()`에서 읽기·쓰기 처리. 이동 상태와 독립적인 행동 상태 브로드캐스트 지원 (`External_Unity_Object.proto`, `PlayerObject.h/cpp`)
- [x] (2026-07-07 #1) 인벤토리 slotIndex 미초기화 버그 수정 — `PlayerInventory` 생성자에서 `_inventorySlots` 25개 전체에 `slotIndex = i` 선행 초기화 추가. DB에서 아이템이 로드되지 않았던 빈 슬롯의 `slotIndex`가 `-1`로 남아, 이후 `UnloadMagazineToInventory`·`InteractContainerObject` 등으로 아이템이 배치될 때 직렬화 시 잘못된 `slot_index`가 전달되어 아이템이 유실되던 문제 (`PlayerInventory.cpp`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
1. **플레이어 장비 변화 브로드캐스팅** — EquipItem 성공 시 같은 방의 다른 플레이어에게 장비 변경 사항을 전송하는 로직 구현 필요 (`ClientPacketHandler.cpp` TODO)
2. **HeartBeat / RequestBlueprint / RequestSpawnMe 클라이언트 연동 테스트 필요** — 프로토콜·직렬화 수정 완료, 빌드 통과. 클라이언트 측 구현 필요
    - `[DROP] 서명 불일치` 출력 → 클라이언트 서명 계산 로직 점검
    - `[DROP] unreliable 시퀀스 중복` 출력 → 클라이언트 uSeqNum 증가 로직 점검
    - StaticObjects 역직렬화 시 `TransformInfo` 구조 변경 반영 여부 클라이언트 측 확인 필요
    - `TestGameRoom::InitTestGameRoom()`에서 TestItemBox 등록 → Blueprint 응답(StaticObjects 청크)에 포함되는지 확인
3. `GameRoom::Update()` 추가 게임 로직 구현 — `TestGameRoom::Update()`의 PlayerState 브로드캐스트 완료. `WinchesterGameRoom::Update()` 로직 미구현(빈 함수). AI, 이벤트 등 추가 게임 루프 로직 필요 (`GameRoom.h/cpp`)
    - 새 플레이어 스폰 시 `D2CSpawnPlayerObject`를 다른 INPLAY 세션에게 개별 전송하는 로직 미구현 — `Handle_C2D_RequestSpawnMe`에서 세션별 SendBuffer 생성·전송 방식 필요 (기존 `Broadcast` 제거됨)
4. /connect요청을 통해서 ip와 port를 받았을 경우 동작 플로우 구현
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
5. 서버 RUDP 작동 검증
    - 헤더 크기 확인: `static_assert(sizeof(UDPHeader) == 35, ...)` (이미 ClientPacketHandler.h에 추가됨)
    - 에코 테스트: `C2DChannelOpen` → `D2CResponseChannelOpen` 흐름이 여전히 동작하는지 확인 (unreliable 경로)
    - reliable 경로 테스트: 테스트 패킷 하나를 FLAG_RELIABLE로 전송 → `_pendingReliable`에 등록되는지 확인
    - ACK 처리 확인: 클라이언트가 응답 패킷 전송 → `_pendingReliable`에서 해당 seqNum 제거되는지 확인
    - 재전송 확인: 클라이언트 ACK 없이 100ms 경과 → `CheckRetransmits()`가 재전송 패킷 송신하는지 로그 확인
6. **PlayerSession을 풀에 반납할 때 반드시 ACK Bitfield 관련 멤버변수를 초기화할 것**
7. `DisconnectSession` 구현 — MAX_RETRY 초과 시 세션 강제 종료 (`DediServerService.cpp` TODO)


### 진행 고려사항

---
