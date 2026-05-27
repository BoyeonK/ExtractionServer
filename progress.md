# 진행 상황 정리 (2026-05-27 업데이트)

## 완료된 것들

### 무기 시스템 / 장비

### 인벤토리 / Player 상태
- [x] (2026-05-20 #6) Item에서 itemType 필드 제거 → ItemDataManager::GetType() 조회 방식으로 전환 — `struct Item`에서 `itemType` 멤버 삭제. `Player.cpp` 내 6곳의 `item.itemType` 참조를 `ItemDataManager::GetType(item.blueprintId)`로 교체. `ItemType::ARMOR` → `EQUIPMENT` 리네이밍(enum, _typeMap, Player.cpp 전부). `GetType()` fallback을 `MISC` → `NONE`으로 변경 (`Items.h`, `ItemDataManager.h`, `Player.cpp`)
- [x] (2026-05-20 #7) pItemDataManager 전역 포인터 및 인스턴스 생성/Init 제거 — ItemDataManager가 inline static const 데이터 + static 메서드만 사용하는 정적 클래스로 전환됨에 따라 `DedicateGlobalVariable.h/cpp`에서 `pItemDataManager` 선언·정의 삭제, `DedicateMain.cpp`에서 `new`/`Init()` 호출 및 include 삭제 (`DedicateMain.cpp`, `DedicateGlobalVariable.h/cpp`)
- [x] (2026-05-22 #0) ItemType::EQUIPMENT → ItemType::ARMOR 통일 — enum 값, `_typeMap`, `Player.cpp` 내 3곳 참조를 모두 ARMOR로 변경. DB 스키마의 ENUM은 이미 ARMOR이었으므로 코드-DB 네이밍 일치 (`Items.h`, `ItemDataManager.h`, `Player.cpp`)
- [x] (2026-05-22 #1) weapon_specs·armor_specs DB 스펙 테이블 추가 — `weapon_specs`(base_damage, rpm, moa, v/h recoil min/max, ammo_type FK) 및 `armor_specs`(max_shield_point, damage_reduction_rate, regeneration_per_second) 테이블을 schema.sql에 추가. items 테이블과 1:1 FK 관계. Python 스크립트로 ItemDataManager.h의 `_weaponSpecs`·`_armorSpecs` 자동생성 예정 (`HTTPServer/database/schema.sql`, `ItemDataManager.h`)
- [x] (2026-05-26 #0) Player 인벤토리 로직을 PlayerInventory 클래스로 분리 — `_primaryWeaponSlot`, `_secondaryWeaponSlot`, `_armorSlot`, `_inventorySlots`, `_inventoryVersion`, `_firstEmptySlotIndex` 및 관련 메서드 7종을 `PlayerInventory.h/cpp`로 추출. Player는 `PlayerInventory _inventory` 멤버 + `GetInventory()` 접근자만 보유. PlayerSession 위임 경로를 `_player.GetInventory().XXX()`로 변경 (`PlayerInventory.h/cpp` 신규, `Player.h/cpp`, `PlayerSession.h`, `CMakeLists.txt`)
- [x] (2026-05-26 #1) PlayerInventory에 매거진 슬롯 추가 — `_primaryWeaponMagazineSlot`, `_secondaryWeaponMagazineSlot` 멤버 + getter 추가. `D2CFullInventorySync` proto에 `primary_weapon_magazine`(필드6), `secondary_weapon_magazine`(필드7) 추가. `SerializeFullInventory()`에 매거진 직렬화 로직 반영. proto 재컴파일 필요 (`PlayerInventory.h/cpp`, `External_Protocol.proto`)
- [x] (2026-05-26 #2) Equip/Unequip 시 매거진 슬롯 자동 언로드 — `UnloadMagazineToInventory(bool isPrimary)` private 헬퍼 추가. `EquipWeaponFromInventory`·`UnequipWeaponToInventory` 양쪽에서 무기 교체/해제 전 대응 매거진 슬롯을 인벤토리로 이동(동일 blueprintId 합산, 빈 슬롯 배치, 슬롯 없으면 파기) (`PlayerInventory.h/cpp`)
- [x] (2026-05-27 #0) weapon_specs 스키마 변경에 따른 WeaponSpec 구조체 수정 — `h_recoil_min` 컬럼 삭제 → `hRecoilMin` 필드 제거. `spread_base`·`spread_max`·`spread_increase_per_shot`·`spread_recovery_rate` 4개 컬럼 추가 → `spreadBase`·`spreadMax`·`spreadIncreasePerShot`·`spreadRecoveryRate` 필드 추가 (`schema.sql`, `ItemDataManager.h`)
- [x] (2026-05-27 #1) weapon_specs에 ammo_max 컬럼 추가 — 한 번 장전 시 최대 탄약 수를 나타내는 `ammo_max INT UNSIGNED NOT NULL` 컬럼을 `ammo_type` 앞에 추가 (`schema.sql`)
- [x] (2026-05-27 #2) PlayerInventory 생성 시 매거진 자동 장전 — `LoadMagazineFromInventory(weaponSlot, magazineSlot)` private 메서드 추가. 생성자에서 무기 슬롯 배치 후 `WeaponSpec::ammoType`에 해당하는 탄약을 인벤토리에서 `maxAmmo`만큼 매거진 슬롯에 이동. 복수 인벤토리 슬롯에 분산된 탄약도 순차 합산 (`PlayerInventory.h/cpp`)
- [x] (2026-05-27 #3) LoadMagazineFromInventory 부분 장전 지원 및 버전 관리 — 이미 매거진에 탄알이 있는 경우 `maxAmmo - 현재량`만큼만 충전하도록 수정. 인벤토리 변경 발생 시 `UpdateFirstEmptySlotIndex()` + `_inventoryVersion++` 호출 추가 (`PlayerInventory.cpp`)

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
