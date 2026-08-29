# 진행 상황 정리 (2026-08-29 업데이트)


## 완료된 것들

### 네트워크 / 패킷
- [x] (2026-08-27 #0) `D2CFullInventorySync` 크기 검사를 두지 않기로 확정 — 최악이 834B로 더 빡빡한 제약인 수신 버퍼(1024B)의 85%라 검사가 방어할 구간이 없다. 근거와 재계산 시 놓치기 쉬운 둘은 「진행 고려사항」으로 옮겼고, 2026-08-26 #6의 정상 경로 실측도 함께 끝났다 (`progress.md`)
- [x] (2026-08-27 #1) 세션 경계 규칙 수립과 `.proto` 주석 정리 반영 — 서버 세션이 클라이언트 디렉터리를 **수정뿐 아니라 열람도 하지 않는다**는 규칙을 루트 `CLAUDE.md`에 세웠다(컨텍스트 오염 차단). 클라이언트 세션의 `.proto` 정리(651→391줄)로 끊긴 서버 문서의 참조 둘을 복구했다 (`CLAUDE.md`, `progress.md`)
- [x] (2026-08-27 #2) 컨테이너를 열지 않으면 인벤토리 정리가 통째로 막히던 것 수정 — 컨테이너 ID 검사가 맨 위에서 돌아 양쪽 id가 `0xFFFFFFFF`인 **순수 인벤토리 내 이동까지** 거부했다. 옆 `Handle_C2D_RequestEquipItem`처럼 한쪽이라도 센티널이 아닐 때만 검사하도록 옮겼고 본문은 그대로다 (`ClientPacketHandler.cpp`)
- [x] (2026-08-27 #3) 오브젝트 표시명 도입과 킬 로그 반영 — `virtual GetObjectName()`으로 타입당 고정 이름과 인스턴스별 이름을 갈라(비-플레이어 저장 비용 0B) 킬 통보 둘에 이름 필드를 순수 추가했다. 함께 `CorpseContainer` → `PlayerLootContainer`, `ObjectType::Corpse` → `PlayerLoot`로 정리했고 와이어 값 3은 그대로다 (`UnityGameObject.h`, `PlayerObject.h`, `PlayerLootContainer.h`, `GameRoom.h/cpp`, `ClientPacketHandler.cpp`, `External_Protocol.proto`)
- [x] (2026-08-29 #0) 재장전 연출 통보 경로 신설 — `C2DNotifyReloadSequence`(44)·`D2CNotifyReloadSequence`(45) 추가, `PKT_ID_MAX` 46. 남에게 "장전 중"만 보여주는 순수 연출 채널이라 서버가 단계 번호를 보관하지 않으며, 15=완료만 서버가 발행한다 — 거부된 재장전이 남의 화면에서 완료로 보이는 것을 막는다 (`External_Protocol.proto`, `enum.h`, `ClientPacketHandler.h/cpp`)
- [x] (2026-08-29 #1) 아이템 데이터 헤더를 `database/`에서 서버 트리로 복사하도록 빌드에 연결 — 이것으로 아이템 수치의 출처가 서버 헤더에서 **DB**로 뒤집혔다. 붙이는 과정에서 세 총기의 `spread_recovery_rate`가 이미 갈라져 있던 것(`database/` 800, `src/` 400)을 발견했고, 다음 빌드에서 800으로 바뀐다 (`CMakeLists.txt`)
- [x] (2026-08-29 #2) 평문 비밀번호 제거와 클라이언트 반영 완료분 `[작업사항]` 정리 — 공개 저장소에 올라가 있던 `generate_script.py`의 MySQL root 비밀번호를 지우고 DB 계정 비밀번호를 교체했으며, `.gitignore` + `git rm --cached`로 추적까지 끊었다(추적 중인 파일에는 ignore가 적용되지 않아 둘은 짝이다). `.proto` 상단 `[작업사항]` (3)(4)(5)를 삭제해 `D2CUpdatePlayerStates` 한 건만 남겼다 (`.gitignore`, `database/generate_script.py`, `External_Protocol.proto`)
- [x] (2026-08-29 #3) `progress.md` 완료 목록 압축 — 스킬의 완료 항목 규칙이 "전체 10개 상한 + 항목당 3문장 이하"로 바뀌어 기존 항목을 "무엇을 왜"만 남기도록 다시 썼다(완료 목록 29,870 → 4,468B). 잘라낸 것은 「진행 고려사항」이 이미 들고 있는 설계 근거·불변식뿐이며, 대응 항목이 없던 `(2026-08-26 #4)`의 down 마이그레이션 관례는 지우기 전에 그쪽으로 옮겼다 (`progress.md`)
- [x] (2026-08-29 #4) 「진행 우선사항」 최신화 — 클라이언트 코드 실사 결과 미구현으로 적혀 있던 연동 목록 12건이 전부 구현돼 있어 목록에서 제거했고, `generate_script.py` 자격증명 항목은 해결 확인으로 종결했다. 우선사항에 남은 것은 래그돌·사망 애니메이션, 피격 방향 표시, 실환경 검증 체크리스트뿐이다. 지우기 전에 영속 규칙 둘(`[작업사항]` 블록 삭제 전 이관 단계, 생성기의 저장소 외부화)은 「진행 고려사항」으로 옮겼다 (`progress.md`)
- [x] (2026-08-29 #5) 「진행 고려사항」 해체 — 44개 항목 중 42건이 진행 상황이 아니라 확정된 불변식·규약·결정이라 단조 증가만 하는 구조였고, 해당 디렉터리의 CLAUDE.md 4곳(루트·`src/`·`src/DedicateProcess/`·`HTTPServer/`)으로 날짜·경위 서사를 걷어내며 압축 이관했다(progress.md 53KB → 10KB). 진짜 미결 2건만 남기고 절을 "미결·보류·검증 대기 전용"으로 재정의했으며, 주석 정책의 기록처도 CLAUDE.md로 바꿔 재비대를 막았다 (`progress.md`, `CLAUDE.md`, `src/CLAUDE.md`, `src/DedicateProcess/CLAUDE.md`, `HTTPServer/CLAUDE.md`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
1. **사망 연출 — 남은 것은 캐릭터 자체의 연출뿐** — 서버 측(이탈 처리·시신 컨테이너·가해자 전달·킬 피드·사망 후 5초 세션 유예)은 완료됐고, 클라이언트도 `D2CNotifyPlayerKilled` 수신 → 킬 로그 표시 → 탑뷰 사망 카메라(`DeathCameraController`, 2초 보간) → 매치 이탈까지 붙어 있다. 클라이언트의 씬 정리 타이머는 `MATCH_EXIT_DELAY = 4f`로 서버 유예 5초보다 짧게 잡아 하향 트래픽이 끊기기 전에 나간다. 남은 것은 피격·사망 시 캐릭터의 래그돌·애니메이션 트리거(`PlayerController.ProcessHit`이 TODO 스텁)이며 전부 클라이언트 몫이다 — 서버는 `OnDeathResolved()` 훅을 열어둔 것으로 완료이고, 비플레이어 오브젝트의 연출은 첫 파괴 가능 오브젝트가 생길 때 그 훅 안에서 각자 처리한다
2. **클라이언트 연동 — 구현 확인 완료, 실환경 검증과 부분구현 둘만 남았다** — 2026-08-29 클라이언트 코드 실사 결과, 이 항목이 들고 있던 미구현 목록(스폰 통보 36·선제 `D2CSpawnPlayerObject` 13, 무기 교체 37/38, 재장전 42/43, 재장전 연출 44/45, 킬 피드 40, 오브젝트 킬·디스폰 41/39, 헤더 `timestampEcho` 에코, 탄약 차감, 장착 시 손 슬롯 이동 규칙, 자기 실드 재생 예측, 사망 유예 처리)은 **전부 구현돼 있어 목록에서 제거했다**. 남은 것 둘 —
    - **피격 방향 표시(damage indicator) 미구현** — `D2CNotifyHealthChange.attacker_object_id`의 수신·가해자 추적(5초 기한)까지는 구현됐으나 표시 UI·이펙트가 없다. 피격 리액션을 담당할 `PlayerController.ProcessHit`이 TODO 스텁이다 (위 사망 연출 항목과 같은 자리다)
    - **실환경 스모크 테스트** — 서버 로그에 `[DROP] 서명 불일치`·`[DROP] unreliable 시퀀스 중복`이 찍히는지, StaticObjects 역직렬화가 `TransformInfo` 구조 변경을 반영하는지, `TestGameRoom::InitTestGameRoom()`의 TestItemBox가 Blueprint 응답(StaticObjects 청크)에 포함되는지 확인
3. **`POST /api/session/resume` 실호출 검증** — 서버 구현과 클라이언트 연동(`LobbyScene`이 결과 씬 경유 복귀 시 `PostResumeSessionCall` 호출)은 끝났고, Redis·MySQL이 붙은 환경에서의 확인만 남았다. ① `/api/login` 후 그 sessionId로 호출 시 200 + `uid`/`money`/`inventory`가 DB 값과 일치하는지 ② 호출 직후 `TTL sess:<UUID>`가 3600 근처로 재충전됐는지(미들웨어 슬라이딩 갱신 확인) ③ 삭제·만료된 sessionId로 호출 시 401 ④ `/api/guest` 세션으로 호출 시 200 + `money: 0, inventory: []`. **응답 필드를 바꾸려면 루트 세션에서 양쪽을 함께 고쳐야 한다** — `Extraction/Assets/Scripts/Network/http-api-spec.yaml`이 `HTTPServer/http-api-spec.yaml`의 사본이라 명세도 짝으로 움직인다
4. **윈체스터 룸의 고유 게임 루프 로직** — 상태 브로드캐스트는 `GameRoom::Update()`가 모든 룸에 대해 처리하므로 `WinchesterGameRoom`은 현재 `Update()`를 override하지 않는다. 맵 고유의 주기 처리(이벤트 등)가 정해지면 `Update() override`를 새로 쓰면서 **파생 로직을 먼저 하고 마지막에 `GameRoom::Update()`를 부를 것**. AI는 서버에서 다루지 않기로 했다 (`GameRoom.h/cpp`)

### 진행 고려사항

미결·보류·검증 대기 항목 전용이다. 확정된 불변식·규약·결정 배경은 해당 디렉터리의 CLAUDE.md에 적는다 (루트 `CLAUDE.md` 「진행 상황 추적」 절 참조).

- **컨테이너 경로에는 미결 둘이 남아 있다** — 2026-08-27 #2에서 파악됐고 고치지 않았다. ① **`C2DRequestOpenContainer`의 실패가 전부 조용하다** — 이미 다른 컨테이너가 열려 있음·룸 없음·오브젝트 못 찾음·`Container` 타입 아님 네 갈래가 모두 deny 패킷도 로그도 없이 `return false`다. 클라이언트는 열기가 거부됐는지 패킷이 유실됐는지 구분할 수 없고, 열렸다고 가정하고 진행하면 뒤이은 조작이 엉뚱한 로그로 나타난다 ② **`Handle_C2D_RequestInteractContainerObject`가 정상 조작의 거부에도 `DENY_SERVER_INTERNAL`(0x0200)을 쓴다** — 순서 역전으로 `C2DCloseContainer`가 먼저 처리된 경우가 대표적인데, 클라이언트에는 "서버 내부 오류"로 보여 재시도가 무의미하다는 판단조차 못 한다. 둘 다 증상이 아니라 **진단을 어렵게 만드는 구조**라, 컨테이너 관련 버그 제보를 받으면 이 둘을 먼저 의심할 것
- **비플레이어 사망 경로는 아직 한 번도 실행되지 않는다** — `CombatObject`의 파생이 `PlayerObject`뿐이라 `FindNonplayerObject()`가 전투 오브젝트를 찾는 일이 없다. 검증용 더미 클래스를 만들지 않기로 했으므로 첫 파괴 가능 오브젝트를 만드는 시점에 피격→사망→회수→통보 전체를 그 자리에서 디버그할 것. 그때 지켜야 할 불변식 둘 — ① 회수 지시는 `TakeDamage()` 안이 아니라 **호출부가** 한다(안에서 `delete`하면 호출부가 직후에 읽는 `GetCurrentHp()`가 use-after-free) ② `OnDeathResolved()`에 `_dynamicObjects`의 반복자를 넘기지 않는다(훅이 흔적을 스폰하면 rehash로 무효화)

---
