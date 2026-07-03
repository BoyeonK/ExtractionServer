# 진행 상황 정리 (2026-07-03 업데이트)


## 완료된 것들

### 무기 시스템 / 장비

### 인벤토리 / Player 상태
- [x] (2026-06-11 #1) C2DRequestEquipItem / D2CResponseEquipItem 장비 프로토콜 구현 — equip/unequip 액션, 장비 슬롯 타입(primary_weapon/secondary_weapon/armor), 인벤토리·컨테이너 양쪽 지원. PlayerInventory에 `EquipWeaponFromSlot`/`UnequipWeaponToSlot`/`EquipArmorFromSlot`/`UnequipArmorToSlot` 메서드 추가. 맨손 금지·매거진 자동 언로드·타입 검증 포함 (`External_Protocol.proto`, `enum.h`, `PlayerInventory.h/cpp`, `ClientPacketHandler.h/cpp`)
- [x] (2026-06-11 #2) D2CResponseInteractItemDeny 거부 패킷 구현 — 요청 거부 시 `source_packet_id`+`deny_reason_mask` 비트필드로 거부 사유 전송. DenyReason enum 10비트 정의(VERSION_MISMATCH, SLOT_EMPTY, SLOT_NOT_EMPTY 등). PlayerInventory 4개 메서드에 `outDenyReason` 파라미터 추가. 두 핸들러를 `do-while(false)` 패턴으로 리팩터링하여 `SendDeny()` 호출을 핸들러당 1곳으로 집약 (`External_Protocol.proto`, `enum.h`, `PlayerInventory.h/cpp`, `ClientPacketHandler.h/cpp`)
- [x] (2026-06-15 #0) PlayerInventory.cpp `enum.h` include 누락 수정 — `DenyReason` 상수(`DENY_SLOT_EMPTY`, `DENY_ITEM_TYPE_MISMATCH` 등)가 `enum.h`에 정의되어 있으나 include 체인에 포함되지 않아 컴파일 에러 발생. `#include "enum.h"` 추가 (`PlayerInventory.cpp`)
- [x] (2026-06-15 #1) 매치메이킹 최소 인원 1명 테스트용 변경 — `FindMatchGroup()`의 waitTime≥8초 구간을 5초로 변경하고 `targetMinPlayers`를 2→1로 수정. 1명으로도 즉시 매칭 가능하도록 설정 (`Matchmaker.cpp`)
- [x] (2026-06-15 #2) 컨테이너 중복 열기 방어 코드 복원 — `Handle_C2D_RequestOpenContainer`에서 테스트용으로 주석 처리되어 있던 `GetInteractingContainerId() != -1` 가드 복원. 이미 컨테이너를 열고 있는 상태에서 중복 요청 차단 (`ClientPacketHandler.cpp`)
- [x] (2026-06-30 #0) TestItemBox 초기 아이템 설정 — `Container.h`에 `InitializeSlots()`·`PlaceItem()` protected 헬퍼 추가. `TestItemBox` 생성자에서 `InitializeTestItems()` 호출(슬롯0: 5.56mm×60, 슬롯1: 7.62mm×30, 슬롯2: 경량 조끼×1). `GameRoom.cpp` 변경 없음 (`Container.h`, `TestGameObjects.h`)
- [x] (2026-07-01 #0) Handle_C2D_RequestInteractContainerObject 거부 지점 상세 로그 추가 — 각 `denyMask` 설정 지점에 `std::cout` 로그 삽입. containerId 없음·유효하지 않은 interactType·GameRoom/오브젝트/Container nullptr·start/end 버전 불일치(클라이언트 vs 서버 값 출력)·슬롯 nullptr·빈 슬롯·get/swap/merge 개별 거부 사유(슬롯 인덱스·수량·blueprintId 등) 총 20개 지점 (`ClientPacketHandler.cpp`)
- [x] (2026-07-01 #1) 거부 패킷 분리 — 공용 `D2CResponseInteractItemDeny`(source_packet_id 포함)를 제거하고 `D2CResponseInteractContainerObjectDeny`·`D2CResponseEquipItemDeny` 두 개로 분리. `source_packet_id` 필드 삭제. PKT_ID 25/26 및 PKT_ID_MAX=27 재정의. `SendInteractContainerObjectDeny`/`SendEquipItemDeny` 헬퍼 함수 추가 (`External_Protocol.proto`, `enum.h`, `ClientPacketHandler.h/cpp`)
- [x] (2026-07-01 #2) C2DRequestEquipItem / D2CResponseEquipItem에 my_inventory_version 추가 — 컨테이너에서 장비 장착 시 플레이어 인벤토리 버전도 함께 검증·응답. 서버에서 objectId != 0xFFFFFFFF일 때만 `pkt.my_inventory_version()` 비교, 성공 시 `response.set_my_inventory_version()` 설정 (`External_Protocol.proto`, `ClientPacketHandler.cpp`)
- [x] (2026-07-03 #0) D2CResponseRecentContainerInfo proto 메시지 추가 — C2DRequestRecentInventoryInfo(PktId=27)의 컨테이너 응답용. D2CResponseOpenContainer와 동일 구조(container_object_id, container_version, container_volume, repeated container_slots). PktId=28 등록 (`External_Protocol.proto`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
0. **[현재] C2DRequestRecentInventoryInfo 핸들러 구현** — proto 메시지 정의 완료(D2CResponseRecentContainerInfo). proto 컴파일 선행 필요 → 핸들러 작성 (`ClientPacketHandler.h/cpp`)
    - object_id=0xFFFFFFFF → D2CFullInventorySync 응답 (기존 SerializeFullInventory 재사용)
    - object_id=컨테이너 → 상호작용 검증 후 D2CResponseRecentContainerInfo 응답 (기존 Container 직렬화 재사용)
1. **proto 컴파일 + 빌드 필요** — D2CResponseRecentContainerInfo 등 proto 변경분 미컴파일. Linux 서버에서 `bash Protocol/compileProto.sh` → `cmake --build build` 실행 필요
2. **플레이어 장비 변화 브로드캐스팅** — EquipItem 성공 시 같은 방의 다른 플레이어에게 장비 변경 사항을 전송하는 로직 구현 필요 (`ClientPacketHandler.cpp` TODO)
3. **HeartBeat / RequestBlueprint / RequestSpawnMe 클라이언트 연동 테스트 필요** — 프로토콜·직렬화 수정 완료, 빌드 통과. 클라이언트 측 구현 필요
    - `[DROP] 서명 불일치` 출력 → 클라이언트 서명 계산 로직 점검
    - `[DROP] unreliable 시퀀스 중복` 출력 → 클라이언트 uSeqNum 증가 로직 점검
    - StaticObjects 역직렬화 시 `TransformInfo` 구조 변경 반영 여부 클라이언트 측 확인 필요
    - `TestGameRoom::InitTestGameRoom()`에서 TestItemBox 등록 → Blueprint 응답(StaticObjects 청크)에 포함되는지 확인
4. `GameRoom::Update()` 추가 게임 로직 구현 — `TestGameRoom::Update()`의 PlayerState 브로드캐스트 완료. `WinchesterGameRoom::Update()` 로직 미구현(빈 함수). AI, 이벤트 등 추가 게임 루프 로직 필요 (`GameRoom.h/cpp`)
    - 새 플레이어 스폰 시 `D2CSpawnPlayerObject`를 다른 INPLAY 세션에게 개별 전송하는 로직 미구현 — `Handle_C2D_RequestSpawnMe`에서 세션별 SendBuffer 생성·전송 방식 필요 (기존 `Broadcast` 제거됨)
5. /connect요청을 통해서 ip와 port를 받았을 경우 동작 플로우 구현
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
6. 서버 RUDP 작동 검증
    - 헤더 크기 확인: `static_assert(sizeof(UDPHeader) == 35, ...)` (이미 ClientPacketHandler.h에 추가됨)
    - 에코 테스트: `C2DChannelOpen` → `D2CResponseChannelOpen` 흐름이 여전히 동작하는지 확인 (unreliable 경로)
    - reliable 경로 테스트: 테스트 패킷 하나를 FLAG_RELIABLE로 전송 → `_pendingReliable`에 등록되는지 확인
    - ACK 처리 확인: 클라이언트가 응답 패킷 전송 → `_pendingReliable`에서 해당 seqNum 제거되는지 확인
    - 재전송 확인: 클라이언트 ACK 없이 100ms 경과 → `CheckRetransmits()`가 재전송 패킷 송신하는지 로그 확인
7. **PlayerSession을 풀에 반납할 때 반드시 ACK Bitfield 관련 멤버변수를 초기화할 것**
8. `DisconnectSession` 구현 — MAX_RETRY 초과 시 세션 강제 종료 (`DediServerService.cpp` TODO)


### 진행 고려사항

---
