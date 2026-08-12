# 진행 상황 정리 (2026-08-13 업데이트)


## 완료된 것들

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
- [x] (2026-08-13 #0) 룸 브로드캐스트 헬퍼 `Broadcast` / `BroadcastExcept` 추가 — `GameRoom`에 템플릿 멤버로 신설. `ClientPacketHandler::Make*()` 계열이 전부 `SendBuffer* (const PBType&, PlayerSession*)`로 통일돼 있어 이를 raw 함수 포인터로 받는다(`std::function` 미사용 — 힙 할당이 없고 패킷 타입 불일치를 컴파일 타임에 검출). reliable/unreliable 구분은 `Make*()`가 이미 갖고 있어 별도 인자가 없다. 대상은 `IsInplay()` 세션으로 한정(`_playerSessions`에 제거 로직이 없어 로딩 중·이탈 세션이 남아 있고, 로딩 중인 쪽에 스폰 패킷을 보내면 청사진 이전 오브젝트를 참조하게 됨), 제외 대상은 슬롯 재사용을 고려해 포인터가 아닌 세션 id로 지정. 반환값은 실제 전송 성공 수 — `SendBuffer` 확보 실패 시 reliable은 재전송 큐에도 오르지 못해 그 세션만 영구 누락되므로 로그를 남긴다. `GameRoom.h`의 `PlayerSession` 전방선언을 include로 교체(`PlayerSession.h`→`Player.h`→`PlayerInventory.h` 경로에 `GameRoom.h`가 없어 순환 없음). 이어서 기존 중복 루프 2곳 교체 — WeaponFire 브로드캐스트, `TestGameRoom::Update()`의 PlayerState 브로드캐스트. 필터 조건·전송 순서 동일, 동작 변화 없음 (`GameRoom.h`, `GameRoom.cpp`, `ClientPacketHandler.cpp`)

### 코드 규약 / 문서
- [x] (2026-08-12 #2) `TODO:` / `TEMP:` 주석 구분 규약 도입 — `TODO:`는 아직 구현되지 않은 것, `TEMP:`는 테스트를 위해 의도적으로 값을 제한하거나 코드를 막아둔 것. 코드베이스 전수 조사 후 8곳을 `TEMP:`로 전환: 2인 매칭 테스트 값(`Matchmaker.cpp:125`), UDP 포트 범위 하드코딩(`DediServerService.cpp:75`), `GetPublicIP()` 주석처리(`sample.h:88`), 테스트 로그 4곳(`match.js`, `ipcManager.js` 3곳), 플레이어 최대 HP 10배 값(`PlayerObject.h:7`). 이어서 `active_match` 락의 임시 동작을 문맥과 함께 문서화 — 락의 본래 의미("진행 중인 게임이 있음")와 임시 해제 경로 2가지(TTL 300초 자동 만료 / 만료 티켓에 대한 `/cancel` 강제 해제), 감수 사항(TTL이 게임 길이보다 짧아 게임 도중 락이 풀림), 해제 조건을 세 곳에 동일 문구로 명시. `redis_keys.md`의 INPROGRESS·SUCCESS 전환 주체를 실제 코드 위치로 정정(각각 `MatchMaker::VerifyAndSetMatchStatus()`, `UpdateEntryTokenRequest::Execute()`). 동작 변경 없음 (`Matchmaker.cpp`, `DediServerService.cpp`, `sample.h`, `PlayerObject.h`, `match.js`, `ipcManager.js`, `redis_keys.md`)

### 인프라 / 배포
- [x] (2026-08-06 #0) AWS→Oracle Cloud 이전에 따른 문서·주석 갱신 — AWS EC2→Oracle Compute Instance, AWS RDS→MySQL HeatWave로 언급 일괄 변경. 코드 주석 4건(`RedisProxyRequest.cpp`, `DediServerService.cpp`), 문서 4건(`README.md`, 루트 CLAUDE.md, `LinuxServerTest/CLAUDE.md`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
1. **사망 처리 구현** — `CombatObject::OnDeath()` 오버라이드(PlayerObject): 사망 브로드캐스트, 세션 정리, 리스폰 로직. 비플레이어 전투 오브젝트는 `FindNonplayerObject()` → `dynamic_cast<CombatObject*>`로 타입 판별 후 처리
2. **플레이어 장비 변화 브로드캐스팅** — EquipItem 성공 시 같은 방의 다른 플레이어에게 장비 변경 사항을 전송하는 로직 구현 필요. 전송 수단(`GameRoom::BroadcastExcept`)은 준비됐으므로 전용 패킷 정의만 추가하면 된다 (`ClientPacketHandler.cpp` TODO)
3. **HeartBeat / RequestBlueprint / RequestSpawnMe 클라이언트 연동 테스트 필요** — 프로토콜·직렬화 수정 완료, 빌드 통과. 클라이언트 측 구현 필요
    - `[DROP] 서명 불일치` 출력 → 클라이언트 서명 계산 로직 점검
    - `[DROP] unreliable 시퀀스 중복` 출력 → 클라이언트 uSeqNum 증가 로직 점검
    - StaticObjects 역직렬화 시 `TransformInfo` 구조 변경 반영 여부 클라이언트 측 확인 필요
    - `TestGameRoom::InitTestGameRoom()`에서 TestItemBox 등록 → Blueprint 응답(StaticObjects 청크)에 포함되는지 확인
4. `GameRoom::Update()` 추가 게임 로직 구현 — `TestGameRoom::Update()`의 PlayerState 브로드캐스트 완료. `WinchesterGameRoom::Update()` 로직 미구현(빈 함수). AI, 이벤트 등 추가 게임 루프 로직 필요 (`GameRoom.h/cpp`)
    - 새 플레이어 스폰 시 `D2CSpawnPlayerObject`를 다른 INPLAY 세션에게 전송하는 로직 미구현 — `GameRoom::BroadcastExcept`로 처리 가능하나, `SpawnPlayerObject()` 호출 시점에 당사자 세션이 아직 INPLAY가 아닐 수 있어 전환 순서를 먼저 확정해야 한다
5. **`DisconnectSession` 구현** — MAX_RETRY 초과 시 세션 비활성화 (`DediServerService.cpp` TODO). 세션 수명은 **즉시 해제하지 않고 매치 종료까지 유지**하는 방향으로 결정: 탈출·사망·연결 끊김 모두 비활성 상태로만 전환하고, 실제 `PlayerSession` 해제는 `GameRoom` 소멸 시 일괄 수행한다 (`GameRoom::ReleaseThis()`도 아직 호출부가 없어 룸 소멸 경로 자체가 미구현)
    - 이유: 매치 중 dangling 포인터가 원천 차단된다(`GameRoom::_playerSessions` / `DediServerService::_players` / `_tokenToPlayerSession`이 같은 포인터를 들고 있고, 특히 `_playerSessions`에는 제거 API가 없어 브로드캐스트가 순회 중 죽은 포인터를 만날 수 있음). `CheckRetransmits()` 루프 중 해제로 인한 재진입 함정(반환된 `PendingPacket*` 벡터가 세션 소멸자에서 풀로 반납되며 무효화)도 성립하지 않는다. 정리 지점이 3곳에서 1곳으로 수렴하고, 추후 재접속 복귀의 여지도 남는다
    - **해제는 미루되 정리는 즉시** 해야 하는 3가지 — ① `_pendingReliable` 비우기 + `CheckRetransmits()` 순회에 `IsActiveState()` 필터 추가(현재 `nullptr` 체크만) ② `GameRoom::_playerObjects`에서 `PlayerObject` 제거(오브젝트 수명 ≠ 세션 수명, 안 그러면 `FillPlayerStates()`가 나간 플레이어를 계속 브로드캐스트) ③ 인벤토리 확정·DB 반영(프로세스가 비정상 종료하면 룸 소멸 시점은 오지 않음)
    - 상태는 비활성 하나로 두고 **이탈 사유를 별도 필드**(`LeaveReason { RECALLED, DEAD, DISCONNECTED }`)로 보관할 것 — 탈출 성공을 `DISCONNECTED`로 표기하면 의미가 어긋나고, 매치 결과 집계·HTTP 보고에 사유가 필요하다. 세 경로가 공통 정리 함수로 모이므로 그 함수의 인자로 붙인다
6. **귀환 확정 이후 실제 처리 구현** — 5초 유지 검사 시퀀스와 `D2CNotifyRecallResult` 통보까지는 구현됐으나, 확정 시점(`RecallTick()`)에 서버 상태는 그대로 INPLAY로 남는다. 세션 INPLAY 해제, 인벤토리 반출 확정 및 HTTP 서버 저장, 퇴장 브로드캐스트, `PlayerObject` 제거 필요. 미정의 영역 있음 (`ClientPacketHandler.cpp` TODO)
    - 이 정리가 없는 동안은 귀환 성공 후에도 플레이어가 룸에 남아 **재귀환 요청이 다시 승인된다**. 레거시 동작 확인용 중간 상태이며, 정리 구현 시 자연히 해소됨
    - `D2CNotifyRecallResult`는 reliable이라 재전송 큐가 세션에 붙는다. 세션 정리는 이 패킷이 ACK된 뒤에 수행해야 결과가 유실되지 않음
    - 연결 두절 검사는 `IsInplay()` 기반이라 세션을 비활성 상태로 전환하는 코드(할 일 5번)가 들어와야 실효가 생긴다. 그 전까지는 귀환 도중 접속을 끊어도 5초 뒤 성공 처리됨
7. **윈체스터 귀환 영역 좌표 확정** — `MapDataManager.h`의 `_winchesterRecallZones`(북/남/동 3개)는 아직 자리표시자 값. 실제 맵 지오메트리 및 클라이언트 트리거 콜라이더와 대조하여 확정 필요. 서버 반경 ⊇ 클라이언트 트리거(약 +0.5m 여유)를 지킬 것 — 서버 쪽이 좁으면 "귀환 UI는 떴는데 서버가 거부"하는 버그가 됨. 튜토리얼 맵은 확정 완료
8. **`GameRoom`의 오브젝트 생성 브로드캐스트 6곳** — `SpawnStaticObject`/`SpawnDynamicObject`/`SpawnPlayerObject`(Test·Winchester 각각)의 `TODO : 생성 정보를 broadcast`. 전송 수단은 준비됐으나 그대로 꽂으면 안 된다 — ① 정적 오브젝트는 `TestGameRoom` 생성자(`InitTestGameRoom()`)에서 스폰되어 그 시점엔 세션이 0명이고 청사진 응답으로 이미 전달되므로, 브로드캐스트가 의미 있는 건 런타임 스폰뿐이다 ② reliable로 N개를 개별 통보하면 ACK 윈도우(33개) 한계에 걸리므로 `FillStaticObjects()`의 청킹 패턴을 따라 묶어야 한다 (`GameRoom.cpp`)


### 진행 고려사항
- **`TODO:` / `TEMP:` 주석 구분** — `TODO:`는 아직 구현되지 않은 것, `TEMP:`는 테스트를 위해 의도적으로 값을 제한하거나 코드를 막아둔 것. 릴리스 전 되돌려야 할 항목은 `grep -rn "TEMP" src HTTPServer`로 전수 확인할 것 (현재 8곳)
- **`active_match` 락의 임시 동작은 세 곳이 한 묶음** — `match.js`의 TTL 300초(`/start`)와 `matchCancel` Lua의 `return 2` 분기, 그리고 `redis_keys.md` 2번 절이 같은 해제 조건을 공유한다. 이 락은 본래 "진행 중인 게임이 있음"을 뜻하며 **사망 처리(할 일 1번) · 귀환 확정 처리(할 일 6번) · `DisconnectSession`(할 일 5번)** 이 완성되어야 정상화된다. 셋 중 하나라도 구현할 때 세 곳을 함께 걷어낼 것. 현재는 TTL이 게임 길이보다 짧아 게임 도중 락이 풀리고 새 매칭을 걸 수 있는 상태를 감수 중
- **`CombatObject::TakeDamage()` 감소율 적용 범위 미확정** — armor 착용 중이어도 실드가 0으로 소진되면 `_currentShield -= damage`가 즉시 음수가 되어 penetrated 경로를 타고 `damageReductionRate`가 전혀 적용되지 않는다. "실드 잔량이 있을 때만 감소율 적용"이 의도인지, armor 착용만으로 감소율이 상시 적용되어야 하는지 확인 필요 (`CombatObject.h:46`)
- **지연 콜백은 예외 없이 sessionId 재조회 규약을 따를 것** — `TimerExecuter`에 취소 API가 없어 룸/세션이 정리된 뒤에도 잔여 콜백이 실행될 수 있다. 콜백이 `PlayerSession*`를 캡처하지 않고 sessionId로 매번 재조회하며 uid·generation을 검증하면(현재 `RecallTick()`이 이 방식), 정리 시 `_players[id] = nullptr`만으로 잔여 콜백이 스스로 포기하므로 별도의 정지 확인(quiescence) 로직이 필요 없다. **한 곳이라도 raw 포인터를 캡처하면 이 설계가 무너진다**
- **reliable in-flight 33개 초과 시 영구 ACK 불가** — ACK 커버리지는 `ackRSeqNum` 1개 + 비트필드 32개 = 총 33개다(`PlayerSession.cpp:85`, `120-130`). `_pendingReliable`에 34개 이상이 동시에 쌓이면 가장 오래된 것은 클라이언트가 이미 받았어도 알릴 수단이 없어 MAX_RETRY까지 재전송된다(수신 측은 `UpdateRRecvState()`에서 `diff > 32`로 버리므로 정확성은 유지, 대역폭만 낭비). 지금까지는 reliable이 요청-응답이라 도달하지 않았으나 **reliable 브로드캐스트가 들어오면 도달 가능해진다** — 대량 통보는 개별 전송 대신 청킹으로 묶을 것. 근본 해결은 비트필드 64비트 확장 또는 송신 창 제한이며 별도 작업
- **MAX_RETRY 초과 패킷이 `_pendingReliable`에 영구 잔류** — `DediServerService.cpp:227-229`가 `continue`만 하고 제거하지 않으며, 제거 경로는 `ProcessIncomingAck()` 뿐이라 침묵한 클라이언트의 패킷은 세션 소멸까지 남는다. `GetRetransmitCandidates()`가 매번 큐 전체를 순회하며 벡터에 담으므로 할당 비용도 함께 커진다. 브로드캐스트는 서버가 먼저 미는 구조라 응답 없는 세션의 큐가 단조 증가한다(현재 브로드캐스트 2개는 둘 다 unreliable이라 미발현). 할 일 5번 구현 시 해소됨
- **브로드캐스트 페이로드 재직렬화는 의도적 보류** — N명에게 보낼 때 페이로드가 N번 직렬화된다. 세션마다 헤더·서명이 달라 `SendBuffer` 재사용이 불가능하지만 **페이로드 바이트 자체는 동일**하므로 1회 직렬화 + memcpy로 줄일 수 있다. 다만 4인/100ms 기준 룸당 절약분이 ~3μs, 250룸이어도 코어의 0.75% 수준이고 같은 경로의 `sendto` 비용이 이를 압도해 실익이 없다. `Make*FromPayload` 짝 함수가 패킷마다 늘어나 `pktId`/`reliable`이 어긋날 여지가 생기는 쪽이 손해. **재검토 조건: 룸 인원 10명 초과 / 틱 25~50ms로 상향 / 동시 룸 수 세 자릿수.** 변경 영향은 `ClientPacketHandler`와 브로드캐스트 두 함수 안에 갇혀 있다
- **귀환 유효성 검사는 안티치트가 아님** — 검사에 쓰이는 `PlayerObject::position`은 `ApplyState()`에서 클라이언트가 보낸 좌표를 그대로 기록한 값이다(`PlayerObject.cpp:10-13`). 좌표 조작 클라이언트는 항상 통과한다. 실질적 차단에는 서버 측 이동 검증(속도/텔레포트 체크)이 별도로 필요

---
