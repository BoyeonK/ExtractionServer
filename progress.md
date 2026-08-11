# 진행 상황 정리 (2026-08-12 업데이트)


## 완료된 것들

### 전투 시스템 / 무기 / 장비
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
- [x] (2026-08-12 #0) 귀환 승인 후 5초 유지 검사 시퀀스 구현 — `D2CResponseRecall(result=true)` 이후 1초 간격으로 위치를 5회 재검사하고 전부 통과하면 귀환 확정. 결과는 `D2CNotifyRecallResult`(성공 여부 + 인덱스 에코 + `RecallResultReason`)로 reliable 통보. 취소 조건은 영역 이탈 / 사망(`IsAlive()`) / 세션이 INPLAY 이탈 / 오브젝트 조회 실패. `TimerExecuter`에 취소 API가 없으므로 `PlayerSession`에 귀환 세대(`_recallGeneration`)를 두어 취소·완료된 귀환의 잔여 콜백이 스스로 포기하게 처리, 콜백은 raw 포인터 대신 sessionId로 세션을 매번 재조회(uid로 슬롯 재사용 검증). 진행 중 중복 요청은 무시 (`External_Protocol.proto`, `enum.h`, `PlayerSession.h`, `ClientPacketHandler.h/cpp`)
- [x] (2026-08-12 #1) 튜토리얼 맵 귀환 영역 좌표 확정 — `_tutorialRecallZones`를 자리표시자 2개(북동 50,50 / 남서 -50,-50)에서 실제 값 1개로 교체: 중심 (10, 10), 반경 5.5m, Y 범위 -5 ~ 5. 인덱스 1이 사라졌으므로 클라이언트도 튜토리얼 탈출구를 1개만 두어야 한다. `TestGameRoom` 스폰 4곳과는 최소 XZ 거리 10m로 겹치지 않음. 자리표시자 확정 TODO 주석은 미확정 상태로 남은 `_winchesterRecallZones` 쪽으로 이동 (`MapDataManager.h`)

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
6. **귀환 확정 이후 실제 처리 구현** — 5초 유지 검사 시퀀스와 `D2CNotifyRecallResult` 통보까지는 구현됐으나, 확정 시점(`RecallTick()`)에 서버 상태는 그대로 INPLAY로 남는다. 세션 INPLAY 해제, 인벤토리 반출 확정 및 HTTP 서버 저장, 퇴장 브로드캐스트, `PlayerObject` 제거 필요. 미정의 영역 있음 (`ClientPacketHandler.cpp` TODO)
    - 이 정리가 없는 동안은 귀환 성공 후에도 플레이어가 룸에 남아 **재귀환 요청이 다시 승인된다**. 레거시 동작 확인용 중간 상태이며, 정리 구현 시 자연히 해소됨
    - `D2CNotifyRecallResult`는 reliable이라 재전송 큐가 세션에 붙는다. 세션 정리는 이 패킷이 ACK된 뒤에 수행해야 결과가 유실되지 않음
    - 연결 두절 검사는 `IsInplay()` 기반이라 `SessionState::DISCONNECTED`로 전환하는 코드(다음 할 일 5번)가 들어와야 실효가 생긴다. 그 전까지는 귀환 도중 접속을 끊어도 5초 뒤 성공 처리됨
7. **윈체스터 귀환 영역 좌표 확정** — `MapDataManager.h`의 `_winchesterRecallZones`(북/남/동 3개)는 아직 자리표시자 값. 실제 맵 지오메트리 및 클라이언트 트리거 콜라이더와 대조하여 확정 필요. 서버 반경 ⊇ 클라이언트 트리거(약 +0.5m 여유)를 지킬 것 — 서버 쪽이 좁으면 "귀환 UI는 떴는데 서버가 거부"하는 버그가 됨. 튜토리얼 맵은 확정 완료


### 진행 고려사항
- **`CombatObject::TakeDamage()` 감소율 적용 범위 미확정** — armor 착용 중이어도 실드가 0으로 소진되면 `_currentShield -= damage`가 즉시 음수가 되어 penetrated 경로를 타고 `damageReductionRate`가 전혀 적용되지 않는다. "실드 잔량이 있을 때만 감소율 적용"이 의도인지, armor 착용만으로 감소율이 상시 적용되어야 하는지 확인 필요 (`CombatObject.h:46`)
- **귀환 유효성 검사는 안티치트가 아님** — 검사에 쓰이는 `PlayerObject::position`은 `ApplyState()`에서 클라이언트가 보낸 좌표를 그대로 기록한 값이다(`PlayerObject.cpp:10-13`). 좌표 조작 클라이언트는 항상 통과한다. 실질적 차단에는 서버 측 이동 검증(속도/텔레포트 체크)이 별도로 필요

---
