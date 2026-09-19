# 진행 상황 정리 (2026-09-19 업데이트)


## 완료된 것들

### 네트워크 / 패킷
- [x] (2026-09-03 #1) 테네리페 컨테이너 64대에 초기 전리품 분배 — 배치만 끝나고 전부 비어 있던 컨테이너를 3단계 규칙으로 채운다(전 대수 기본 탄약 8~16발 → 랜덤 20대에 추가 탄약 10~20발 → 랜덤 20대에 AK-47 4·M4A1 4·SCAR 8·경량 조끼 4를 겹치지 않게). 탄약 풀과 장비 쿼터는 `MapDataManager`의 맵별 테이블에 뒀고(`ItemDataManager`는 생성 산출물이라 카테고리 열거 API를 붙일 수 없다) `DistributeLoot()`이 배치 전 `GetType()`으로 대조해 어긋난 항목만 걸러낸다. 미결이던 월드 배치 아이템의 `instanceUid` 출처는 `WORLD_ITEM_UID_BASE`(`1<<62`)에서 시작하는 룸 로컬 카운터로 확정해 실 DB uid 공간과 영구히 분리했고 `TestItemBox`의 1001~1003도 같은 체계로 옮겼다 (`GameRoom.h/cpp`, `MapDataManager.h`, `Container.h`, `Items.h`, `TestGameObjects.h`, `src/DedicateProcess/CLAUDE.md`)
- [x] (2026-09-18 #0) HostileNPC 계층과 주도권 프로토콜 신설 — `CombatObject` 파생이 `PlayerObject` 하나뿐이라 한 번도 실행된 적 없던 피격→사망→회수 경로 위에, 클라이언트가 주도권을 갖는 적대 오브젝트 베이스(`HostileNPC`)와 첫 구체 타입 `Turret`(ObjectType 9)을 올렸다. aggro 변경과 주도권 이전을 **별도 패킷으로 분리**해 반납 이후 도착한 낡은 감쇠 요청이 재탈취를 일으키는 경로를 없앴고, `aggro == MIN ⟺ targetId == NO_TARGET` 불변식을 `SetAggroInternal()` 한 곳에 가둬 「주인 없는 MAX aggro」로 오브젝트가 매치 내내 잠기는 것을 막았다. NPC 상태는 상향을 `C2DUpdatePlayerState`에 합승시키고 하향은 `Update()`와 두 틱(50ms) 엇갈린 `UpdateNpcStates()`로 내보내 두 unreliable 패킷이 같은 순간에 나가지 않게 했다 (`UnityGameObjects/HostileNPC.h`, `UnityGameObjects/HostileNPCs.h`, `GameRoom.h/cpp`, `ClientPacketHandler.h/cpp`, `DediServerService.cpp`, `enum.h`, `UnityGameObject.h`, `External_Protocol.proto`, `CMakeLists.txt`, `HTTPServer/index.js`, `src/DedicateProcess/CLAUDE.md`)
- [x] (2026-09-19 #0) 테네리페에 Turret 4대 배치 — 구현만 끝나고 인스턴스가 0이던 `HostileNPC` 계층에 첫 스폰 경로를 붙였다. 컨테이너 64대와 같은 형태로 `MapDataManager`에 맵별 정적 테이블(`MapHostileNpcSpawn`·`_tenerifeNpcSpawns`·`GetHostileNpcSpawns()`)을 두고 `InitTenerifeGameRoom()`이 전리품 분배 뒤 순회해 생성하며, 알 수 없는 `ObjectType`은 로그를 남기고 건너뛴다. 등록은 `SpawnStaticObject()`가 아니라 `SpawnDynamicObject()`다 — 회수 경로인 `DestroyDeadObject()`가 `_dynamicObjects`만 보기 때문이고, yaw 는 지시에 회전값이 없어 전부 0도로 뒀다 (`MapDataManager.h`, `GameRoom.cpp`)
- [x] (2026-09-19 #1) 피격에 의한 주도권 이전과 MIN aggro 하향 — 주도권이 클라이언트 요청으로만 옮겨가던 탓에 Turret 을 쏴도 반응을 끌어낼 수단이 없었다. 피격을 `ApplyAuthorityRequest(가해자, MAX)` 의 위임(`ApplyDamageAuthority()`)으로 정의해 새 관문 없이 「aggro 가 MAX 미만이면 가해자에게 넘어가고 MAX 면 무시」가 기존 `클램프(claimed) > 현재` 관문에서 그대로 떨어지게 했고, 덤으로 한 번 MAX 가 되면 뒤따르는 피격이 전부 거부돼 번갈아 쏠 때의 주도권 왕복이 구조적으로 막힌다. 겸사 `DEFAULT_MIN_AGGRO` 를 4 에서 1 로 내려 사유별 등급 여백을 4단계에서 7단계로 넓혔다 — 발걸음·시야처럼 낮은 등급의 주장이 붙을 예정이기 때문이고, 값의 의미는 클라이언트가 정하고 서버는 경위를 보지 않는다 (`UnityGameObjects/HostileNPC.h`, `ClientPacketHandler.cpp`, `External_Protocol.proto`, `src/DedicateProcess/CLAUDE.md`)

### HTTP 서버 / 인증·세션
- [x] (2026-09-02 #7) 프리 로드아웃 입장 조건 신설과 `match.js` 스냅샷 검증 통일 — FREE는 스냅샷 영속(`/match/start`)도 `DELETE`(`/match/connect`)도 건너뛰던 탓에, 귀환 전리품을 창고로 옮긴 클라이언트의 로컬 재배치가 DB에 닿지 못한 채 레이드 종료의 `ApplyInventoryToDb()`(로드아웃 타입도 이탈 사유도 보지 않고 `slot_index >= 80`을 지운다)에 조용히 파괴됐다. FREE도 CUSTOM과 같은 대조·영속 경로를 타게 하고 스냅샷에 `slot_index >= 80` 항목이 있으면 `ERR_LOADOUT_NOT_EMPTY`(400)로 거부한다 — 영속을 함께 붙이지 않으면 배치를 DB에 반영하는 경로가 그 셋뿐이라 유저가 조건을 만족시킬 수단이 없다(창고 만석은 판매로 덜어내는 것이 정상 경로라 예외를 두지 않았고, `/match/connect`의 `DELETE`는 CUSTOM 조건을 유지한다). 겸사 `match.js`의 자체 형식 검증·총량 대조를 `utils/inventorySnapshot.js`로 합쳐 CUSTOM에도 `0~107` 상한이 생겼고, `inventory`가 두 모드 공통 필수가 되어 게스트를 포함한 FREE 요청이 빈 배열을 명시해야 한다 (`routes/match.js`, `http-api-spec.yaml`, `HTTPServer/CLAUDE.md`)
- [x] (2026-09-08 #0) 시동 시 Redis 키스페이스 파기 — 재시작하면 `token_`의 `fd`·`session_id`는 사라진 Dedicate 프로세스를 가리키고 `ticket_`은 비어서 새로 뜬 매치메이커 큐에 없어 영영 매칭되지 않으며 `active_match:`는 해제 주체(`NotifyPlayerLeftRequest::Execute()`)가 사라져 TTL 900초까지 잔류해 로그인·재매칭을 막았다. `RedisHandler::ResetKeyspace()`가 `InitializeItemCache()`보다 먼저 `FLUSHDB`로 db 0을 지우고 `guest_uid_counter`만 되쓰며, 실패하면 낡은 상태로 뜨지 않도록 기동을 포기한다(영속 데이터는 전부 MySQL에 있어 유실이 없다). 파기 대상을 키 패턴 목록으로 좁히지 않은 것은 목록에서 빠진 키만 조용히 살아남기 때문이고, 전제는 서버 인스턴스가 하나뿐이라는 것이며(둘이면 나중에 뜬 쪽이 먼저 뜬 쪽의 세션·락·티켓을 지운다) 귀결로 메인 프로세스 재시작이 곧 전원 강제 로그아웃이 됐다 (`src/RedisHandler.h/cpp`, `src/main.cpp`, `database/redis_keys.md`, `src/CLAUDE.md`)

### DB / 마이그레이션
- [x] (2026-09-03 #0) db-migrate 설정 예시에 `multipleStatements` 추가 — `20260825210551-initial-schema-up.sql`이 6개 문장이라 이 플래그 없이는 baseline 마이그레이션이 첫 문장에서 끊긴다. `database.json.example`에 넣으면서 함께 빠져 있던 쉼표를 채워 JSON 파싱 오류도 고쳤다. 실 `database.json`은 `.gitignore` 대상이라 예시만 고쳐서는 기존 환경이 그대로이므로 `local`·`production` 양쪽에 손으로 넣어야 한다 (`database/database.json.example`)

### 빌드 / 의존성
- [x] (2026-09-15 #0) myUtils 의존성 제거 — `CMakeLists.txt`가 `FetchContent`로 BoyeonK/myUtils를 받아 `MyUtils::MyUtils`를 링크하고 있었으나, `src` 전체의 `#include` 대상을 전수 추출해 보니 이 라이브러리의 헤더 경로(`MyUtils/…`)를 쓰는 곳이 하나도 없었다. 선언·`FetchContent_MakeAvailable`·링크 항목을 지웠고 `include(FetchContent)`는 abseil이 쓰므로 남겼다. 루트 CLAUDE.md는 이것을 「깃 서브모듈」로 적고 있었으나 실제로는 FetchContent였고(`.gitmodules` 자체가 없다) 항목째 지웠다 — 리눅스 쪽 `build/_deps`에 캐시가 남아 있으면 첫 재구성은 빌드 디렉터리를 비우고 할 것 (`CMakeLists.txt`, `CLAUDE.md`)

### 문서
- [x] (2026-09-07 #0) 프롬프트 피드백 로그 3건을 `.txt`에서 `.md`로 전환 — 같은 폴더의 `CC프롬프트4_서버-피드백.md`만 마크다운이라 확장자가 갈려 있었고, 평문은 GitHub에서 구조 없이 렌더돼 공개 목적에 맞지 않았다. 원문 문장은 그대로 두고 서식만 입혔으며(제목·인용·코드펜스·표), 3번 파일의 두 덩어리를 나누는 제목 한 줄과 따옴표 하나가 어긋난 자리만 손봤다. 프롬프트 원문 `.txt` 5건은 대상이 아니라 그대로 뒀다 (`docs/Claude_Code_프롬프트/CC프롬프트1~3_피드백.md`)
- [x] (2026-09-16 #0) 룸 상한 서술 정정과 제품 명칭 통일 — `redis_keys.md` 2번 절이 「`GameRoom`에 시간 상한이 없고 그 여유는 코드가 아니라 기획이 지킨다」고 적고 있었으나 `GameRoom::ROOM_LIFETIME_MS`(600000ms)가 이미 상한을 강제해 `src/CLAUDE.md`의 락 항목과 정반대로 말하고 있었다 — 현재 사실로 고치고 프로세스를 가로지르는 불변식(`ROOM_LIFETIME_MS < ACTIVE_MATCH_TTL_SEC × 1000`)의 출처를 가리키게 했다. 겸사 `docs/` 도입부 네 곳의 「ExtractionServer」를 정식 명칭인 「Salvage Protocol 서버」로 통일해 `gamestate-persistence.md`·`authentication.md`와 갈려 있던 것을 맞췄고, 저장소 이름으로 쓰인 README 두 곳과 장르명 「Extraction Shooter」는 그대로 뒀다 (`HTTPServer/database/redis_keys.md`, `docs/networking.md`, `docs/matchmaking.md`, `docs/dedicated-server.md`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
1. **DB 작업 선행** — 아래 다섯 건은 서버 코드 변경 없이 DB 에서 끝나지만, 뒤의 컨텐츠 작업이 전부 이 값을 전제로 한다. 수치가 바뀌는 항목은 「마이그레이션 → `generate_script.py` → 빌드」 순서를 지킬 것(루트 CLAUDE.md 「데이터 원천 규율」)
   - ~~SCAR 추가~~ — **완료.** `ItemDataManager.h` 의 `_nameMap`·`_weaponSpecs` 에 id 3 이 이미 들어 있다(b9fa926)
   - 전술 아머 추가 (`items` + `armor_specs`) — `_armorSpecs` 는 아직 id 4(경량 조끼) 하나뿐이다
   - 아이템 설명 최신화
   - 스프레드 값 소폭 하향, 스프레드 회복값 상향
   - 상점 리스트를 실제값으로 채우기 — **마이그레이션은 올라갔다**(4c68b45). 남은 것은 두 프로세스 재시동이다 — 가격을 손대면 갱신 수단이 없는 캐시 둘(Node `shopCache`·Redis `item_meta`)이 낡은 값을 문다(아래 「진행 고려사항」 참조)
2. **Turret 수치 확정과 첫 실플레이 디버그** — 배치(2026-09-19 #0)와 피격 주도권(#1)까지 붙었으나 피격→사망→회수→통보 경로는 **아직 한 번도 실행된 적이 없다**. 남은 것 셋 — ① `HostileNPCs.h`의 잠정 수치 확정(`DEFAULT_MAX_HP` 30000·`ATTACK_DAMAGE` 1200은 AK-47 `baseDamage` 4800과 플레이어 200HP만 보고 넣은 값이다. aggro 범위는 1~8로 확정) ② 네 대의 yaw — 현재 전부 0도이고 바라볼 방향이 정해지면 `_tenerifeNpcSpawns`만 고치면 된다 ③ 클라이언트 동반 구현(아래 항목) 뒤 전 경로를 그 자리에서 디버그할 것

### 진행 고려사항

미결·보류·검증 대기 항목 전용이다. 확정된 불변식·규약·결정 배경은 해당 디렉터리의 CLAUDE.md에 적는다 (루트 `CLAUDE.md` 「진행 상황 추적」 절 참조).

- **서버가 systemd 서비스로 등록됐고, Node 실행 경로가 nvm 버전에 묶여 있다** — 유닛 파일(`extraction-server.service`, 저장소 밖)이 `WorkingDirectory=/home/ubuntu/LinuxServerTest/build`로 `.env` 로드와 `execl("./LinuxServerTest")`를 살리고, `Environment=PATH=`에 `/home/ubuntu/.nvm/versions/node/v24.19.0/bin`을 박아 `launchNode()`의 `execlp("node", ...)`를 살린다. **nvm 으로 node 버전을 올려 그 디렉터리가 사라지면 Node 만 안 뜨는데 `systemctl status`는 `active (running)`이다**(자식 감시가 없다) — 유일한 신호는 journal 의 `execlp failed` 한 줄이다. 부팅 자동 실행은 하지 않기로 했고(`systemctl enable` 금지), 로그 유실을 막으려 `LogRateLimitIntervalSec=0`을, 크래시가 조용히 묻히지 않도록 `Restart=no`를 택했다
- **메인 프로세스에 시그널 처리가 없다 (SIGPIPE·SIGTERM)** — `IoUringWrapper.cpp`의 `io_uring_prep_send()`가 flags 를 0 으로 넘겨 `MSG_NOSIGNAL` 이 없으므로, 자식이 먼저 죽은 뒤 그 IPC fd 로 보내면 SIGPIPE 로 메인이 통째로 내려갈 수 있다(`main()`에 `signal(SIGPIPE, SIG_IGN)` 한 줄로 닫힌다). SIGTERM 핸들러도 없어 `systemctl stop`·Ctrl+C 가 진행 중인 DB 반영을 끊는데, 그때 생기던 `active_match` 잔류는 이제 다음 시동의 키스페이스 파기가 청소하므로 남는 손해는 그 판의 MySQL 반영 유실뿐이다. 알파 단계에서 재시작이 드물어 우선순위를 낮게 뒀다
- **`utils/redisKeys.js`는 모듈 전체가 죽어 있고 그중 하나는 값이 틀리다** — `session`·`userSession`·`matchTicket` 셋 다 `require` 하는 곳이 없고(각 라우트가 키를 인라인으로 만든다), `matchTicket`은 `` `ticket:${id}` ``(콜론)를 돌려주는데 실제 키는 `match.js:222`의 `ticket_<UUID>`(언더바)다. 지금은 동작에 영향이 없지만 누군가 이 헬퍼를 쓰는 순간 어긋나고, `HTTPServer/CLAUDE.md`의 파일 목록은 이것을 살아 있는 유틸로 적고 있다 — 지우든 살려 쓰든 한쪽으로 정리할 것
- **`src/sample.h`는 `.gitignore`에 있지만 git 이 여전히 추적한다** — 추적 중인 파일에는 `.gitignore`가 적용되지 않으므로 변경분이 계속 커밋 대상으로 잡히고, 그 안의 `TEMP:` 두 줄(로컬 테스트용 IP 하드코딩)이 릴리스 전 전수 확인에 계속 걸린다. 파일을 지우지 않고 추적만 끊으려면 `git rm --cached src/sample.h`가 필요한데 커밋에 삭제로 기록되는 것을 원치 않아 보류했다 — 정리한다면 파일째 삭제가 맞다는 루트 CLAUDE.md의 판단과 함께 볼 것
- **아이템 가격을 바꾸려면 두 프로세스를 모두 재시동해야 한다** — 구매가는 Node 의 `shopCache`(시동 시 1회), 판매가는 Redis `item_meta:<item_id>`(C++ 메인 프로세스 시동 시 1회)에서 오고 **갱신 수단이 어느 쪽에도 없다.** MySQL `items.price` 만 고치면 두 값이 서로도, DB 와도 어긋난 채 돈다. 한쪽만 재시동하면 같은 아이템의 구매가와 판매가가 갈리므로, 갱신 API 나 캐시 무효화가 필요해지는 시점까지는 순서를 손으로 지킨다
- **`/api/items/purchase`에 `active_match` 검사가 없다** — 게임 중 세션이 상점을 부르는 것을 서버가 막지 않는다. 정확성은 스냅샷 대조의 `FOR UPDATE`가 이미 확보하므로(끼어든 레이드 결과가 있으면 409) 손실은 없고, 붙인다면 얻는 것은 "게임 중엔 상점을 쓸 수 없다"를 침묵이 아닌 명시적 거부로 알려 주는 것뿐이다. 정상 클라이언트는 인게임에서 상점 UI를 띄우지 않아 우선순위가 낮다
- **`/match/start`의 커밋과 `active_match` 락 획득 사이에 창이 있다** — 스냅샷 영속이 트랜잭션에서 끝난 뒤에 락을 잡으므로(`match.js`), 그 틈에 들어온 `/api/items/sell`이 인벤토리 영역(80~107)에 아이템을 다시 놓을 수 있다. 그러면 FREE의 「입장 시 그 영역은 비어 있다」가 깨지고 그 아이템은 레이드 종료에 지워진다 — 매칭 중에 파는 정상 클라이언트가 없어 실질 위험은 낮고 CUSTOM에도 원래 있던 창이다. 닫는 방법은 락을 트랜잭션 앞에서 잡거나 `items.js` 두 라우트에 `active_match` 검사를 붙이는 것이고, 후자는 위의 `/purchase` 항목과 같이 볼 것
- **테네리페 초기 전리품 총량은 실플레이 검증 전이다** — 매치당 탄약 712~1424발(평균 1068), 무기 16정, 경량 조끼 4벌이고 정원 4인이면 조끼는 인당 정확히 1벌이다(의도된 값으로 확정). 조정은 `MapDataManager`의 `TENERIFE_*` 상수와 `_tenerifeEquipQuotas`·`_tenerifeAmmoPool` 세 곳에서만 하면 되고 서버 로직은 건드릴 필요가 없다 — 차종별로 다른 전리품을 주는 것은 지금 구조에 없으며, 필요해지면 `MapContainerSpawn`에 루팅 등급을 붙이는 형태가 될 것
- **`DistributeLoot()`은 아직 컴파일된 적이 없다** — 작업 환경(Windows)에 C++ 컴파일러가 없어 분배 규칙만 별도 포팅으로 검증했다(장비 쿼터 정확성·컨테이너당 최대 3칸·2·3단계 독립성 확인). 문법·타입은 다음 Linux 빌드에서 처음 확인된다
- **비플레이어 사망 경로는 이제 코드로는 도달 가능하지만 실행된 적은 없다** — 테네리페 룸이 Turret 4대를 `_dynamicObjects`에 올리므로 `FindNonplayerObject()`가 전투 오브젝트를 찾는 경로가 열렸으나, 클라이언트 쪽 구현이 선행돼야 실제로 쏠 수 있다. 검증용 더미를 만들지 않기로 했으므로 클라이언트가 붙는 시점에 피격→사망→회수→통보 전체를 그 자리에서 디버그할 것. 그때 지켜야 할 불변식 둘 — ① 회수 지시는 `TakeDamage()` 안이 아니라 **호출부가** 한다(안에서 `delete`하면 호출부가 직후에 읽는 `GetCurrentHp()`가 use-after-free) ② `OnDeathResolved()`에 `_dynamicObjects`의 반복자를 넘기지 않는다(훅이 흔적을 스폰하면 rehash로 무효화)
- **Turret 의 클라이언트 동반 구현 셋이 검증 대기다** — `ObjectType::Turret`(9)의 프리팹 매핑은 되어 있다고 확인받았고, 남은 것은 ① **반납을 0으로 보낼 것** — MIN 을 4에서 1로 내렸으므로 4를 반납으로 박아둔 구현이 있으면 그 값이 반납이 아니라 살아 있는 주장(aggro 4)이 된다. 이번 변경에서 실제로 깨질 수 있는 유일한 자리다 ② 요청 없이 도착하는 `D2CNotifyNpcAuthority`(피격 각성)를 받아 즉시 `npc_states` 송신을 시작할 것 ③ `D2CNotifyObjectKilled` 의 victim 이 자기가 쥔 NPC 면 주도권도 함께 놓을 것(서버는 해제 통보를 내지 않는다). 셋 다 어긋나도 서버 빌드·통신·로그가 전부 정상이라 조용히 드러난다 — 상위 세션에 확인을 요청할 것
- **aggro 감쇠가 게임플레이의 핵심이 되면 서버측 타임아웃을 재검토해야 한다** — 「숨거나 도망쳐 aggro 를 내려 남에게 넘긴다」가 클라이언트에 구현되면, 그 클라이언트가 감쇠 요청을 보내다 멈추는 경우(프리즈·조작 중단)에 NPC 가 그 사람에게 붙박이는 상태의 노출 빈도가 올라간다. 서버가 감쇠도 타임아웃도 돌리지 않는 것은 확정 사항이고(`src/DedicateProcess/CLAUDE.md`) 지금은 수용된 실패 모드이지만, 감쇠가 핵심 메커닉이 되는 시점에 다시 볼 것
- **`targetId == -1`일 때 서버가 하는 일은 정하지 않았다** — Turret은 그 상태에서 움직이지 않으므로 지금은 공백이 문제되지 않는다. 능동적으로 움직이는 적대 오브젝트를 추가할 때 재논의하기로 했고, 그때 서버 주도 이동 통보(reliable)가 함께 오는데 reliable·unreliable이 시퀀스 공간을 공유하지 않아 같은 NPC를 두 채널로 옮기면 낡은 unreliable 상태가 최신 reliable 상태를 덮을 수 있다(자세한 조건은 `src/DedicateProcess/CLAUDE.md`)
- **2026-09-18 #0·09-19 #0·#1의 변경도 아직 컴파일된 적이 없다** — `DistributeLoot()`과 같은 사정으로, 확인한 것은 `PktId` 짝 전수 대조(`enum.h` ↔ `.proto` ↔ 등록·Make 함수 6개)와 필드명·기존 관용구 일치, Turret 배치 쪽의 `ObjectType` 존재·`SpawnDynamicObject` 시그니처·헤더 포함 관계, 그리고 `DEFAULT_MIN_AGGRO` 를 참조하는 자리가 `HostileNPC.h` 안 넷뿐이라는 전수 대조(4를 박은 곳 없음)까지다. 문법·타입은 다음 Linux 빌드에서 처음 확인된다

---
