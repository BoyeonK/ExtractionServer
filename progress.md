# 진행 상황 정리 (2026-08-29 업데이트)


## 완료된 것들

### 네트워크 / 패킷
- [x] (2026-08-29 #1) 아이템 데이터 헤더를 `database/`에서 서버 트리로 복사하도록 빌드에 연결 — 이것으로 아이템 수치의 출처가 서버 헤더에서 **DB**로 뒤집혔다. 붙이는 과정에서 세 총기의 `spread_recovery_rate`가 이미 갈라져 있던 것(`database/` 800, `src/` 400)을 발견했고, 다음 빌드에서 800으로 바뀐다 (`CMakeLists.txt`)
- [x] (2026-08-29 #2) 평문 비밀번호 제거와 클라이언트 반영 완료분 `[작업사항]` 정리 — 공개 저장소에 올라가 있던 `generate_script.py`의 MySQL root 비밀번호를 지우고 DB 계정 비밀번호를 교체했으며, `.gitignore` + `git rm --cached`로 추적까지 끊었다(추적 중인 파일에는 ignore가 적용되지 않아 둘은 짝이다). `.proto` 상단 `[작업사항]` (3)(4)(5)를 삭제해 `D2CUpdatePlayerStates` 한 건만 남겼다 (`.gitignore`, `database/generate_script.py`, `External_Protocol.proto`)
- [x] (2026-08-29 #3) `progress.md` 완료 목록 압축 — 스킬의 완료 항목 규칙이 "전체 10개 상한 + 항목당 3문장 이하"로 바뀌어 기존 항목을 "무엇을 왜"만 남기도록 다시 썼다(완료 목록 29,870 → 4,468B). 잘라낸 것은 「진행 고려사항」이 이미 들고 있는 설계 근거·불변식뿐이며, 대응 항목이 없던 `(2026-08-26 #4)`의 down 마이그레이션 관례는 지우기 전에 그쪽으로 옮겼다 (`progress.md`)
- [x] (2026-08-29 #4) 「진행 우선사항」 최신화 — 클라이언트 코드 실사 결과 미구현으로 적혀 있던 연동 목록 12건이 전부 구현돼 있어 목록에서 제거했고, `generate_script.py` 자격증명 항목은 해결 확인으로 종결했다. 우선사항에 남은 것은 래그돌·사망 애니메이션, 피격 방향 표시, 실환경 검증 체크리스트뿐이다. 지우기 전에 영속 규칙 둘(`[작업사항]` 블록 삭제 전 이관 단계, 생성기의 저장소 외부화)은 「진행 고려사항」으로 옮겼다 (`progress.md`)
- [x] (2026-08-29 #5) 「진행 고려사항」 해체 — 44개 항목 중 42건이 진행 상황이 아니라 확정된 불변식·규약·결정이라 단조 증가만 하는 구조였고, 해당 디렉터리의 CLAUDE.md 4곳(루트·`src/`·`src/DedicateProcess/`·`HTTPServer/`)으로 날짜·경위 서사를 걷어내며 압축 이관했다(progress.md 53KB → 10KB). 진짜 미결 2건만 남기고 절을 "미결·보류·검증 대기 전용"으로 재정의했으며, 주석 정책의 기록처도 CLAUDE.md로 바꿔 재비대를 막았다 (`progress.md`, `CLAUDE.md`, `src/CLAUDE.md`, `src/DedicateProcess/CLAUDE.md`, `HTTPServer/CLAUDE.md`)
- [x] (2026-08-29 #6) 컨테이너 조작의 거부 사유 갈라내기 — reliable C2D 채널이 순서를 보장하지 않아 `C2DCloseContainer`가 앞선 조작보다 먼저 처리되는 역전은 정상 플레이에서 나오는데, 그 자리들이 `DENY_SERVER_INTERNAL`(0x0200)로 나가 클라이언트가 "재시도 무의미한 서버 오류"와 구분할 수 없었다. `DENY_CONTAINER_NOT_OPEN`(0x0400)을 신설해 interact 3곳·equip 2곳을 갈랐고, `.proto` 두 Deny 메시지의 사유 목록과 클라이언트 처리 지시(`[작업사항]`)를 함께 넣었다. `Handle_C2D_RequestOpenContainer`의 조용한 실패 네 갈래는 예상 동작으로 확정해 「진행 고려사항」에서 내렸다 (`enum.h`, `ClientPacketHandler.cpp`, `External_Protocol.proto`, `src/DedicateProcess/CLAUDE.md`)
- [x] (2026-08-29 #7) 윈체스터 룸의 주기 동작을 테스트 룸과 동일하게 유지하기로 확정 — 두 룸 모두 이미 `Update()`를 override하지 않아 코드는 그대로이고, 룸마다 다르게 동작시킬 계획이 없으며 `virtual`은 여지로만 남긴다는 것을 기록했다. 「진행 우선사항」의 윈체스터 항목은 할 일이 아니게 돼 내렸고, 그 안의 확정 결정 둘(override 시 베이스 마지막 호출, AI는 서버에서 다루지 않음)은 지우기 전에 CLAUDE.md로 옮겼다 (`src/DedicateProcess/CLAUDE.md`)
- [x] (2026-08-29 #8) 컨테이너 상호작용 거리 검사 도입 — `GameRoom::IsPlayerNearContainer()`를 열기·조작·장착 세 자리에 붙여 조작 시점의 좌표로 매번 검사하게 했다(열어둔 채 멀어지면 그때부터 거부된다). 열기 실패는 기존 규약대로 조용하고, 조작·장착은 새 비트 `DENY_OUT_OF_RANGE`(0x0800)로 거부한다. 허용 거리는 3m로 클라이언트의 상호작용 표시 거리 2m보다 1m 넉넉하게 잡아 위치 인식 시차를 흡수한다 (`GameRoom.h/cpp`, `ClientPacketHandler.cpp`, `enum.h`, `External_Protocol.proto`)
- [x] (2026-08-29 #9) 컨테이너 점유를 한 명으로 제한 — `Container::_interactingPlayerId`(플레이어 objectId)를 두고 남이 점유 중이면 열기를 거부하되, 점유자가 상호작용 범위 밖이면 소유권을 가져온다 — 해제를 `C2DCloseContainer` 하나에 의존하면 그 패킷 유실로 컨테이너가 매치 끝까지 잠기기 때문이다. 해제는 `GameRoom::ReleaseInteractingContainer()` 하나로 모아 닫기·이탈·재열기가 같이 쓰고, 앞선 점유의 해제는 열기 검증을 전부 통과한 뒤에만 일어난다(실패한 열기가 열어둔 것까지 잃지 않도록). 한 플레이어가 동시에 여는 컨테이너도 하나이며, 열기 실패가 조용한 것은 확정으로 올렸다 (`Container.h`, `GameRoom.h/cpp`, `ClientPacketHandler.cpp`, `External_Protocol.proto`)
- [x] (2026-08-29 #10) `DENY_CONTAINER_NOT_OPEN`의 원인 우선순위 재정리와 id 검사의 성격 기록 — 점유·소유권 이전이 붙으면서 이 비트의 가장 흔한 원인이 패킷 재정렬이 아니라 "거리 밖에 있는 동안 남이 가져감"으로 바뀌어, `.proto` 설명을 빈도순으로 다시 쓰고 "UI가 열려 있다고 내 것이라는 보장은 없다"를 명시했다. 함께 조작 요청의 `object_id` 일치 검사가 진단이 아니라 **교차 오염 방지 장치**임을 기록했다 — 대상 컨테이너를 세션 상태로 해석하므로 늦게 온 요청의 슬롯 인덱스가 남의 컨테이너에 적용될 수 있고, 컨테이너별 개별 카운터인 버전 가드는 값이 겹쳐 이를 못 막는다 (`External_Protocol.proto`, `src/DedicateProcess/CLAUDE.md`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
1. **사망 연출 — 남은 것은 캐릭터 자체의 연출뿐** — 서버 측(이탈 처리·시신 컨테이너·가해자 전달·킬 피드·사망 후 5초 세션 유예)은 완료됐고, 클라이언트도 `D2CNotifyPlayerKilled` 수신 → 킬 로그 표시 → 탑뷰 사망 카메라(`DeathCameraController`, 2초 보간) → 매치 이탈까지 붙어 있다. 클라이언트의 씬 정리 타이머는 `MATCH_EXIT_DELAY = 4f`로 서버 유예 5초보다 짧게 잡아 하향 트래픽이 끊기기 전에 나간다. 남은 것은 피격·사망 시 캐릭터의 래그돌·애니메이션 트리거(`PlayerController.ProcessHit`이 TODO 스텁)이며 전부 클라이언트 몫이다 — 서버는 `OnDeathResolved()` 훅을 열어둔 것으로 완료이고, 비플레이어 오브젝트의 연출은 첫 파괴 가능 오브젝트가 생길 때 그 훅 안에서 각자 처리한다
2. **클라이언트 연동 — 구현 확인 완료, 실환경 검증과 부분구현 둘만 남았다** — 2026-08-29 클라이언트 코드 실사 결과, 이 항목이 들고 있던 미구현 목록(스폰 통보 36·선제 `D2CSpawnPlayerObject` 13, 무기 교체 37/38, 재장전 42/43, 재장전 연출 44/45, 킬 피드 40, 오브젝트 킬·디스폰 41/39, 헤더 `timestampEcho` 에코, 탄약 차감, 장착 시 손 슬롯 이동 규칙, 자기 실드 재생 예측, 사망 유예 처리)은 **전부 구현돼 있어 목록에서 제거했다**. 남은 것 둘 —
    - **피격 방향 표시(damage indicator) 미구현** — `D2CNotifyHealthChange.attacker_object_id`의 수신·가해자 추적(5초 기한)까지는 구현됐으나 표시 UI·이펙트가 없다. 피격 리액션을 담당할 `PlayerController.ProcessHit`이 TODO 스텁이다 (위 사망 연출 항목과 같은 자리다)
    - **실환경 스모크 테스트** — 서버 로그에 `[DROP] 서명 불일치`·`[DROP] unreliable 시퀀스 중복`이 찍히는지, StaticObjects 역직렬화가 `TransformInfo` 구조 변경을 반영하는지, `TestGameRoom::InitTestGameRoom()`의 TestItemBox가 Blueprint 응답(StaticObjects 청크)에 포함되는지 확인
3. **`POST /api/session/resume` 실호출 검증** — 서버 구현과 클라이언트 연동(`LobbyScene`이 결과 씬 경유 복귀 시 `PostResumeSessionCall` 호출)은 끝났고, Redis·MySQL이 붙은 환경에서의 확인만 남았다. ① `/api/login` 후 그 sessionId로 호출 시 200 + `uid`/`money`/`inventory`가 DB 값과 일치하는지 ② 호출 직후 `TTL sess:<UUID>`가 3600 근처로 재충전됐는지(미들웨어 슬라이딩 갱신 확인) ③ 삭제·만료된 sessionId로 호출 시 401 ④ `/api/guest` 세션으로 호출 시 200 + `money: 0, inventory: []`. **응답 필드를 바꾸려면 루트 세션에서 양쪽을 함께 고쳐야 한다** — `Extraction/Assets/Scripts/Network/http-api-spec.yaml`이 `HTTPServer/http-api-spec.yaml`의 사본이라 명세도 짝으로 움직인다
4. **첫 파괴 가능 오브젝트** — `CombatObject`의 파생이 `PlayerObject`뿐이라 피격→사망→회수→통보 경로가 한 번도 실행된 적이 없다(아래 「진행 고려사항」 참조). 서버 몫은 `ObjectType` 추가와 `ObjectTypeToName()` case 짝, `CombatObject` 파생 클래스 하나, 룸의 `SpawnDynamicObject()` 배치이고 킬·디스폰 통보(41·39)는 이미 붙어 있다. 착수 전에 기획 결정 셋이 필요하다 — 무엇을 만들지(컨테이너 겸 파괴 가능은 금지라 상자류 제외), 최대 HP(100배 스케일), 파괴 시 흔적을 남길지(남긴다면 `OnDeathResolved()` 안에서 별개 오브젝트로 스폰)

### 진행 고려사항

미결·보류·검증 대기 항목 전용이다. 확정된 불변식·규약·결정 배경은 해당 디렉터리의 CLAUDE.md에 적는다 (루트 `CLAUDE.md` 「진행 상황 추적」 절 참조).

- **프리즈 클라이언트가 컨테이너를 무한 점유할 수 있다** — 하트비트만 돌고 상태 갱신이 멈춘 클라이언트는 끊김 판정(6초)에 걸리지 않고 좌표도 굳는다. 그 자리가 컨테이너 옆이면 거리 기반 소유권 이전이 성립하지 않아 매치 끝까지 잠긴다. 점유에 시간 상한(마지막 조작 이후 N초면 이전 허용)을 얹으면 닫히지만, 예외처리 계열이라 미뤄뒀다
- **비플레이어 사망 경로는 아직 한 번도 실행되지 않는다** — `CombatObject`의 파생이 `PlayerObject`뿐이라 `FindNonplayerObject()`가 전투 오브젝트를 찾는 일이 없다. 검증용 더미 클래스를 만들지 않기로 했으므로 첫 파괴 가능 오브젝트를 만드는 시점에 피격→사망→회수→통보 전체를 그 자리에서 디버그할 것. 그때 지켜야 할 불변식 둘 — ① 회수 지시는 `TakeDamage()` 안이 아니라 **호출부가** 한다(안에서 `delete`하면 호출부가 직후에 읽는 `GetCurrentHp()`가 use-after-free) ② `OnDeathResolved()`에 `_dynamicObjects`의 반복자를 넘기지 않는다(훅이 흔적을 스폰하면 rehash로 무효화)

---
