# 진행 상황 정리 (2026-05-20 업데이트)

## 완료된 것들

### HTTPServer / 매치메이킹
- [x] (2026-05-17 #0) `/status` SUCCESS 시 `ticket_` TTL 60초 단축 — 클라이언트 토큰 수신 확인 후 `expire(ticketId, 60)`으로 재전송 여유 확보. `ticket_`+`token_`의 최종 파기는 기존대로 C++ DediManager(`BindClientIpToSession` IPC 처리 시 `del({tokenKey, ticketKey})`)에서 일괄 수행. `/connect`에서의 중복 삭제는 IPC 실패 시 불일치 상태 방지를 위해 제외 (`HTTPServer/routes/match.js`)
- [x] (2026-05-17 #1) `D2MUpdateEntryToken` → Redis 키 라이프사이클 검토 완료 — `token_` 해시의 `ticket` 필드(back-reference)는 DediManager에서 cascade 삭제에 사용 중이므로 유지 확정. 삭제 책임은 C++ MainProcess 단일 지점에 집중
- [x] (2026-05-17 #3) 매치메이킹 2인 테스트 값 적용 — 1인 극단값에서 2인 매칭 테스트용으로 변경. 8초≥ allowedDiff=0/min=2, 20초≥ allowedDiff=1/min=3, 40초≥ allowedDiff=1/min=2. 조건문 순서 수정(큰 값 먼저 비교) (`src/Matchmaker.cpp`)
- [x] (2026-05-19 #1) CUSTOM 로드아웃 매치 요청 시 무기 슬롯 검증 추가 — equipment 슬롯 105(주무기)·106(보조무기) 중 최소 1개에 `item_type='WEAPON'` 아이템 필수. DB `items` 테이블 조회로 타입 검증. 슬롯 미존재 또는 WEAPON 아닌 경우 `400 ERR_NO_WEAPON_EQUIPPED` 반환. FREE 로드아웃은 검증 미적용 (`HTTPServer/routes/match.js`)

### 무기 시스템 / 장비
- [x] (2026-05-19 #0) PlayerObject 무기 상태 연결 — `D2CSpawnPlayerObject`에 `weapon_id` proto 필드 추가(field 3, int32). `PlayerObject`에 `_primaryWeaponId`·`_secondaryWeaponId`·`_isUsingPrimary` 필드 + `SetWeapons()`·`GetCurrentWeaponId()`·`SwitchWeapon()` 메서드 추가. `Handle_C2D_RequestSpawnMe`에서 PlayerSession의 장비슬롯 blueprintId를 PlayerObject에 전달. `FillPlayerObjects()`·`Handle_C2D_RequestSpawnByObjectId()`에서 `set_weapon_id()` 호출. **proto 재생성 필요: `bash Protocol/compileProto.sh`** (`External_Protocol.proto`, `PlayerObject.h/cpp`, `ClientPacketHandler.cpp`, `GameRoom.cpp`)
- [x] (2026-05-19 #2) PlayerObject 방어구 상태 추가 — `_armorId`(blueprintId) 필드 + `SetArmor()`·`GetArmorId()` 메서드 추가. `Handle_C2D_RequestSpawnMe`에서 `PlayerSession::GetArmorSlot().item.blueprintId`를 `PlayerObject`에 전달. 클라이언트 전송 불필요하므로 `FillState`/`Serialize`에는 미포함 (`PlayerObject.h/cpp`, `ClientPacketHandler.cpp`)
- [x] (2026-05-20 #0) SetWeapons primary 빈 슬롯 처리 — primary가 비어있고(blueprintId==0) secondary만 있을 때 `_isUsingPrimary = false`로 설정. 둘 다 비어있으면 기존 동작 유지 (`PlayerObject.cpp`)

### 인벤토리 / Player 상태
- [x] (2026-05-20 #1) Player 클래스 인벤토리 관리 멤버 추가 — `_inventoryVersion`·`_fireSequence`(uint32_t, getter 포함) + `_firstEmptySlotIndex`(int32_t, 빈 슬롯 없으면 -1) + `UpdateFirstEmptySlotIndex()` 메서드 추가. 생성자에서 자동 초기화 (`Player.h/cpp`)
- [x] (2026-05-20 #2) Player 인벤토리 조작 메서드 5종 추가 — `EquipWeaponFromInventory`, `UnequipWeaponToInventory`(맨손 금지 규칙 포함), `EquipArmorFromInventory`, `UnequipArmorToInventory`, `MoveInventorySlot`(동일 blueprintId+소모품 합산 포함). 모두 bool 반환, item 단위 이동/swap, `_inventoryVersion++` 적용 (`Player.h/cpp`)
- [x] (2026-05-20 #3) D2C 인벤토리 풀 싱크 프로토콜 추가 — `InventoryItemInfo`·`InventorySlot`·`D2CFullInventorySync` 메시지를 `External_Protocol.proto`에 추가(PktId=17). `Player::SerializeFullInventory()` 직렬화 메서드 구현. `Handle_C2D_RequestSpawnMe`에서 스폰 직후 reliable로 전송. **proto 재생성 필요: `bash Protocol/compileProto.sh`** (`External_Protocol.proto`, `enum.h`, `Player.h/cpp`, `PlayerSession.h`, `ClientPacketHandler.h/cpp`)

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
