# 진행 상황 정리 (2026-07-21 업데이트)


## 완료된 것들

### 빌드 / 의존성 관리
- [x] (2026-07-08 #0) third_party 폴더 도입 및 xxhash·nlohmann_json 의존성 전환 — `third_party/xxhash/`, `third_party/nlohmann/`에 소스 직접 포함. CMakeLists.txt에서 nlohmann_json FetchContent 제거, include 경로에 `third_party` 추가, `ClientPacketHandler.h`의 xxhash include 경로 수정 (`CMakeLists.txt`, `ClientPacketHandler.h`)
- [x] (2026-07-08 #1) CMake 구성 시 IPC pb 파일 자동 복사 — `Protocol/Compiled/IPC/`의 `.pb.cc`/`.pb.h` 파일을 `src/IPCProtocol/`로 자동 복사하는 `file(COPY)` 커맨드 추가. git에서 pb 파일 제외 후에도 빌드 가능하도록 처리 (`CMakeLists.txt`)
- [x] (2026-07-15 #0) `.claude/settings.json`에 `claudeMdExcludes` 설정 추가 — LinuxServerTest에서 Claude Code 실행 시 상위 `Extraction/CLAUDE.md` 로드를 방지. glob 패턴(`**/Extraction/CLAUDE.md`) 사용으로 환경 비종속적 처리
- [x] (2026-07-15 #1) CLAUDE.md 세분화 — 루트 CLAUDE.md를 개괄 + 참조 테이블로 축소, `src/CLAUDE.md`·`src/DedicateProcess/CLAUDE.md`·`HTTPServer/CLAUDE.md` 자식 파일로 분산. 하위 디렉터리 CLAUDE.md는 지연 로드되어 세션 초기 컨텍스트 절약

### 전투 시스템 / 무기 / 장비
- [x] (2026-07-16 #0) 총알 발사 핸들러 세부 구현 — `Handle_C2D_RequestWeaponFire()`의 weapon_dbid 검증(`PlayerObject::GetCurrentWeaponId()` 비교) 및 탄약 차감(현재 무기 magazine 슬롯 quantity 1 차감, 0이면 거부) 구현. `PlayerObject::IsUsingPrimary()` 접근자, `PlayerInventory::Get[Primary|Secondary]WeaponMagazineMutable()` 접근자 추가 (`ClientPacketHandler.cpp`, `PlayerObject.h`, `PlayerInventory.h`)
- [x] (2026-07-17 #0) CombatObject 레이어 추가 — `UnityGameObject` → `CombatObject` → `PlayerObject` 계층 도입. HP(maxHp, currentHp), Shield/AP(maxShield, currentShield, damageReductionRate, shieldRegenPerSec) 데이터 필드 추가 (`UnityGameObjects/CombatObject.h`, `PlayerObject.h`, `CMakeLists.txt`)
- [x] (2026-07-21 #0) CombatObject 데미지 처리 구현 및 WeaponFire 피격 연결 — `CombatObject::TakeDamage()` 구현(실드 우선 차감 → 관통 시 감소율 미적용, 비관통 시 `damageReductionRate` 적용 → HP 차감). `OnDeath()`·`OnDamageApplied()` virtual 콜백 추가. `Handle_C2D_RequestWeaponFire()`에서 `ItemDataManager::GetWeaponSpec()`으로 baseDamage 조회 후 `TakeDamage()` 호출. `PlayerObject::DEFAULT_MAX_HP`를 100000(테스트용)으로 상향 (`CombatObject.h`, `PlayerObject.h`, `ClientPacketHandler.cpp`)

### 인벤토리 / Player 상태
- [x] (2026-07-21 #1) Armor 장비 시 CombatObject shield 스탯 연동 — `PlayerObject::SetArmor()`에서 `ItemDataManager::GetArmorSpec()` 조회 후 `SetShield()` 호출하여 armor 종류별 maxShield·damageReductionRate·regenPerSec 적용. armor 교체 시 shield=0으로 리셋(치트 방지), 해제 시 `SetShield(0,0,0)`. `ChargeShield(int32_t)` 충전 메서드 추가(max 클램프). 스폰 시 `ChargeShield(maxShield)`로 최대 충전. 장비 교체 핸들러에서 `PlayerObject::SetArmor()` 호출 추가 (`CombatObject.h`, `PlayerObject.cpp`, `ClientPacketHandler.cpp`)
- [x] (2026-07-07 #1) 인벤토리 slotIndex 미초기화 버그 수정 — `PlayerInventory` 생성자에서 `_inventorySlots` 25개 전체에 `slotIndex = i` 선행 초기화 추가. DB에서 아이템이 로드되지 않았던 빈 슬롯의 `slotIndex`가 `-1`로 남아, 이후 `UnloadMagazineToInventory`·`InteractContainerObject` 등으로 아이템이 배치될 때 직렬화 시 잘못된 `slot_index`가 전달되어 아이템이 유실되던 문제 (`PlayerInventory.cpp`)

### 인증 / 세션
- [x] (2026-07-21 #2) 세션 슬라이딩 만료(refreshSession) 구현 — `redisClient.js`에 Lua 스크립트 기반 `refreshSession()` 메서드 추가. `sess:<UUID>`와 `user_sess:{userId}` TTL을 원자적으로 갱신. `requireAuth` 미들웨어에 적용하여 인증된 요청마다 세션 수명 자동 연장 (`config/redisClient.js`, `middleware/auth.js`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
1. **사망 처리 구현** — `CombatObject::OnDeath()` 오버라이드(PlayerObject): 사망 브로드캐스트, 세션 정리, 리스폰 로직. 비플레이어 전투 오브젝트는 `FindNonplayerObject()` → `dynamic_cast<CombatObject*>`로 타입 판별 후 처리
2. **플레이어 장비 변화 브로드캐스팅** — EquipItem 성공 시 같은 방의 다른 플레이어에게 장비 변경 사항을 전송하는 로직 구현 필요 (`ClientPacketHandler.cpp` TODO)
2. **HeartBeat / RequestBlueprint / RequestSpawnMe 클라이언트 연동 테스트 필요** — 프로토콜·직렬화 수정 완료, 빌드 통과. 클라이언트 측 구현 필요
    - `[DROP] 서명 불일치` 출력 → 클라이언트 서명 계산 로직 점검
    - `[DROP] unreliable 시퀀스 중복` 출력 → 클라이언트 uSeqNum 증가 로직 점검
    - StaticObjects 역직렬화 시 `TransformInfo` 구조 변경 반영 여부 클라이언트 측 확인 필요
    - `TestGameRoom::InitTestGameRoom()`에서 TestItemBox 등록 → Blueprint 응답(StaticObjects 청크)에 포함되는지 확인
3. `GameRoom::Update()` 추가 게임 로직 구현 — `TestGameRoom::Update()`의 PlayerState 브로드캐스트 완료. `WinchesterGameRoom::Update()` 로직 미구현(빈 함수). AI, 이벤트 등 추가 게임 루프 로직 필요 (`GameRoom.h/cpp`)
    - 새 플레이어 스폰 시 `D2CSpawnPlayerObject`를 다른 INPLAY 세션에게 개별 전송하는 로직 미구현 — `Handle_C2D_RequestSpawnMe`에서 세션별 SendBuffer 생성·전송 방식 필요 (기존 `Broadcast` 제거됨)
4. `DisconnectSession` 구현 — MAX_RETRY 초과 시 세션 강제 종료 (`DediServerService.cpp` TODO)


### 진행 고려사항

---
