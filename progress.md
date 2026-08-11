# 진행 상황 정리 (2026-08-11 업데이트)


## 완료된 것들

### 빌드 / 의존성 관리
- [x] (2026-07-15 #1) CLAUDE.md 세분화 — 루트 CLAUDE.md를 개괄 + 참조 테이블로 축소, `src/CLAUDE.md`·`src/DedicateProcess/CLAUDE.md`·`HTTPServer/CLAUDE.md` 자식 파일로 분산. 하위 디렉터리 CLAUDE.md는 지연 로드되어 세션 초기 컨텍스트 절약

### 전투 시스템 / 무기 / 장비
- [x] (2026-07-16 #0) 총알 발사 핸들러 세부 구현 — `Handle_C2D_RequestWeaponFire()`의 weapon_dbid 검증(`PlayerObject::GetCurrentWeaponId()` 비교) 및 탄약 차감(현재 무기 magazine 슬롯 quantity 1 차감, 0이면 거부) 구현. `PlayerObject::IsUsingPrimary()` 접근자, `PlayerInventory::Get[Primary|Secondary]WeaponMagazineMutable()` 접근자 추가 (`ClientPacketHandler.cpp`, `PlayerObject.h`, `PlayerInventory.h`)
- [x] (2026-07-17 #0) CombatObject 레이어 추가 — `UnityGameObject` → `CombatObject` → `PlayerObject` 계층 도입. HP(maxHp, currentHp), Shield/AP(maxShield, currentShield, damageReductionRate, shieldRegenPerSec) 데이터 필드 추가 (`UnityGameObjects/CombatObject.h`, `PlayerObject.h`, `CMakeLists.txt`)
- [x] (2026-07-21 #0) CombatObject 데미지 처리 구현 및 WeaponFire 피격 연결 — `CombatObject::TakeDamage()` 구현(실드 우선 차감 → 관통 시 감소율 미적용, 비관통 시 `damageReductionRate` 적용 → HP 차감). `OnDeath()`·`OnDamageApplied()` virtual 콜백 추가. `Handle_C2D_RequestWeaponFire()`에서 `ItemDataManager::GetWeaponSpec()`으로 baseDamage 조회 후 `TakeDamage()` 호출. `PlayerObject::DEFAULT_MAX_HP`를 100000(테스트용)으로 상향 (`CombatObject.h`, `PlayerObject.h`, `ClientPacketHandler.cpp`)

### 인벤토리 / Player 상태
- [x] (2026-07-21 #1) Armor 장비 시 CombatObject shield 스탯 연동 — `PlayerObject::SetArmor()`에서 `ItemDataManager::GetArmorSpec()` 조회 후 `SetShield()` 호출하여 armor 종류별 maxShield·damageReductionRate·regenPerSec 적용. armor 교체 시 shield=0으로 리셋(치트 방지), 해제 시 `SetShield(0,0,0)`. `ChargeShield(int32_t)` 충전 메서드 추가(max 클램프). 스폰 시 `ChargeShield(maxShield)`로 최대 충전. 장비 교체 핸들러에서 `PlayerObject::SetArmor()` 호출 추가 (`CombatObject.h`, `PlayerObject.cpp`, `ClientPacketHandler.cpp`)

### 인증 / 세션
- [x] (2026-07-21 #2) 세션 슬라이딩 만료(refreshSession) 구현 — `redisClient.js`에 Lua 스크립트 기반 `refreshSession()` 메서드 추가. `sess:<UUID>`와 `user_sess:{userId}` TTL을 원자적으로 갱신. `requireAuth` 미들웨어에 적용하여 인증된 요청마다 세션 수명 자동 연장 (`config/redisClient.js`, `middleware/auth.js`)

### 네트워크 / 패킷
- [x] (2026-07-22 #0) D2CNotifyHealthChange 패킷 추가 — 피격 시 대상 클라이언트에게 현재 HP/쉴드 절대값과 변화 원인(`HealthChangeReason`)을 reliable 전송. `Handle_C2D_RequestWeaponFire()`의 `TakeDamage()` 호출 직후 objectId로 세션을 찾아 전송 (`External_Protocol.proto`, `enum.h`, `ClientPacketHandler.h/cpp`)
- [x] (2026-08-11 #0) 귀환(탈출) 요청/응답 프로토콜 추가 — `C2DRequestRecall`(귀환 스팟 인덱스) / `D2CResponseRecall`(bool 결과 + 인덱스 에코) 정의. `MapDataManager` 신설하여 맵별 귀환 영역(`RecallZone`: XZ 원기둥, `radiusSq` 사전 제곱 + y 범위)을 `static constexpr` 테이블로 보유. `GameRoom` 기반 생성자에서 mapId로 테이블 포인터+개수만 연결(룸별 복사·힙 할당 없음), `MapType`↔`MapDataManager::MapId` 값 일치를 `static_assert`로 검증. `Handle_C2D_RequestRecall()`은 인덱스 범위·영역 포함 여부를 검사해 결과를 reliable 응답 (거부 시에도 응답 후 true 반환) (`External_Protocol.proto`, `MapDataManager.h`, `GameRoom.h/cpp`, `enum.h`, `ClientPacketHandler.h/cpp`, `CMakeLists.txt`)
- [x] (2026-08-11 #1) `PlayerSession::Send()` null 체크 추가 — `Make*()` 계열이 `nullptr`을 반환한 경우를 송신 진입점에서 일괄 차단. 기존에는 `D2CSendTask` 생성자의 `_pBuffer->Buffer()`에서 즉시 크래시했다. 이로써 모든 호출부가 `pSession->Send(Make...(...))` 형태를 안전하게 사용 가능 (`PlayerSession.cpp`)

### 인프라 / 배포
- [x] (2026-08-06 #0) AWS→Oracle Cloud 이전에 따른 문서·주석 갱신 — AWS EC2→Oracle Compute Instance, AWS RDS→MySQL HeatWave로 언급 일괄 변경. 코드 주석 4건(`RedisProxyRequest.cpp`, `DediServerService.cpp`), 문서 4건(`README.md`, 루트 CLAUDE.md, `LinuxServerTest/CLAUDE.md`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
1. **사망 처리 구현** — `CombatObject::OnDeath()` 오버라이드(PlayerObject): 사망 브로드캐스트, 세션 정리, 리스폰 로직. 비플레이어 전투 오브젝트는 `FindNonplayerObject()` → `dynamic_cast<CombatObject*>`로 타입 판별 후 처리
2. **플레이어 장비 변화 브로드캐스팅** — EquipItem 성공 시 같은 방의 다른 플레이어에게 장비 변경 사항을 전송하는 로직 구현 필요 (`ClientPacketHandler.cpp` TODO)
3. **HeartBeat / RequestBlueprint / RequestSpawnMe 클라이언트 연동 테스트 필요** — 프로토콜·직렬화 수정 완료, 빌드 통과. 클라이언트 측 구현 필요
    - `[DROP] 서명 불일치` 출력 → 클라이언트 서명 계산 로직 점검
    - `[DROP] unreliable 시퀀스 중복` 출력 → 클라이언트 uSeqNum 증가 로직 점검
    - StaticObjects 역직렬화 시 `TransformInfo` 구조 변경 반영 여부 클라이언트 측 확인 필요
    - `TestGameRoom::InitTestGameRoom()`에서 TestItemBox 등록 → Blueprint 응답(StaticObjects 청크)에 포함되는지 확인
4. `GameRoom::Update()` 추가 게임 로직 구현 — `TestGameRoom::Update()`의 PlayerState 브로드캐스트 완료. `WinchesterGameRoom::Update()` 로직 미구현(빈 함수). AI, 이벤트 등 추가 게임 루프 로직 필요 (`GameRoom.h/cpp`)
    - 새 플레이어 스폰 시 `D2CSpawnPlayerObject`를 다른 INPLAY 세션에게 개별 전송하는 로직 미구현 — `Handle_C2D_RequestSpawnMe`에서 세션별 SendBuffer 생성·전송 방식 필요 (기존 `Broadcast` 제거됨)
5. `DisconnectSession` 구현 — MAX_RETRY 초과 시 세션 강제 종료 (`DediServerService.cpp` TODO)
6. **귀환 승인 이후 실제 처리 구현** — `Handle_C2D_RequestRecall()`이 현재는 `result=true` 응답만 보내고 서버 상태는 그대로 INPLAY로 남는다. 세션 INPLAY 해제, 인벤토리 반출 확정 및 HTTP 서버 저장, 퇴장 브로드캐스트, `PlayerObject` 제거 필요. 미정의 영역 있음 (`ClientPacketHandler.cpp` TODO)
7. **귀환 영역 좌표 확정** — `MapDataManager.h`의 `_tutorialRecallZones` / `_winchesterRecallZones`는 현재 자리표시자 값. 실제 맵 지오메트리 및 클라이언트 트리거 콜라이더와 대조하여 확정 필요. 서버 반경 ⊇ 클라이언트 트리거(약 +0.5m 여유)를 지킬 것 — 서버 쪽이 좁으면 "귀환 UI는 떴는데 서버가 거부"하는 버그가 됨


### 진행 고려사항
- **proto 재생성 선행 필요 (빌드 차단)** — 2026-08-11 추가된 `C2DRequestRecall`/`D2CResponseRecall`은 `.proto`만 수정된 상태다. Windows 개발 환경에는 `protoc`가 없어 컴파일이 불가하므로, 리눅스 환경에서 `Protocol/compileProto.sh`를 실행해 `.pb.cc`/`.pb.h`를 재생성해야 빌드된다. 현재 상태로는 `External_Game_Protocol::C2DRequestRecall` 심볼 미존재로 컴파일 실패
- **`CombatObject::TakeDamage()` 감소율 적용 범위 미확정** — armor 착용 중이어도 실드가 0으로 소진되면 `_currentShield -= damage`가 즉시 음수가 되어 penetrated 경로를 타고 `damageReductionRate`가 전혀 적용되지 않는다. "실드 잔량이 있을 때만 감소율 적용"이 의도인지, armor 착용만으로 감소율이 상시 적용되어야 하는지 확인 필요 (`CombatObject.h:46`)
- **귀환 유효성 검사는 안티치트가 아님** — 검사에 쓰이는 `PlayerObject::position`은 `ApplyState()`에서 클라이언트가 보낸 좌표를 그대로 기록한 값이다(`PlayerObject.cpp:10-13`). 좌표 조작 클라이언트는 항상 통과한다. 실질적 차단에는 서버 측 이동 검증(속도/텔레포트 체크)이 별도로 필요

---
