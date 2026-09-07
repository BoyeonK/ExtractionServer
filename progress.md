# 진행 상황 정리 (2026-09-08 업데이트)


## 완료된 것들

### 네트워크 / 패킷
- [x] (2026-09-03 #1) 테네리페 컨테이너 64대에 초기 전리품 분배 — 배치만 끝나고 전부 비어 있던 컨테이너를 3단계 규칙으로 채운다(전 대수 기본 탄약 8~16발 → 랜덤 20대에 추가 탄약 10~20발 → 랜덤 20대에 AK-47 4·M4A1 4·SCAR 8·경량 조끼 4를 겹치지 않게). 탄약 풀과 장비 쿼터는 `MapDataManager`의 맵별 테이블에 뒀고(`ItemDataManager`는 생성 산출물이라 카테고리 열거 API를 붙일 수 없다) `DistributeLoot()`이 배치 전 `GetType()`으로 대조해 어긋난 항목만 걸러낸다. 미결이던 월드 배치 아이템의 `instanceUid` 출처는 `WORLD_ITEM_UID_BASE`(`1<<62`)에서 시작하는 룸 로컬 카운터로 확정해 실 DB uid 공간과 영구히 분리했고 `TestItemBox`의 1001~1003도 같은 체계로 옮겼다 (`GameRoom.h/cpp`, `MapDataManager.h`, `Container.h`, `Items.h`, `TestGameObjects.h`, `src/DedicateProcess/CLAUDE.md`)

### HTTP 서버 / 인증·세션
- [x] (2026-09-02 #2) 구매 슬롯 유효성 검사를 창고 영역으로 제한 — `/api/items/purchase`의 `slot_index`가 상한 없이 `>= 0`만 검사돼 인벤토리(80~104)·장착(105~107)은 물론 세 영역 밖(108~255, `TINYINT UNSIGNED` 상한)까지 통과했고, 창고가 만석이어도 구매가 조용히 성공했다. 대상 슬롯을 `0~79`로 못 박고 `ERR_INVALID_SLOT`(400)을 신설했으며, 스냅샷 항목에는 위치 자유를 유지한 채 `0~107` 상한만 걸어 유령 행 생성을 막았다 — 배치의 권위가 클라이언트 스냅샷이라 위치까지 대조하면 로컬 재배치가 전부 409로 튕긴다. 흩어져 있던 범위 상수는 `utils/slotLayout.js`로 모았고(`match.js`의 `WAREHOUSE_SLOT_MAX`는 정의만 되고 쓰이지 않던 값이었다), 창고 만석 차단과 새 코드 분기는 클라이언트에서 함께 진행된다 (`routes/items.js`, `routes/match.js`, `utils/slotLayout.js`, `http-api-spec.yaml`, `HTTPServer/CLAUDE.md`)
- [x] (2026-09-02 #3) 구매의 스냅샷 대조와 재작성을 한 트랜잭션으로 묶음 — 인벤토리 조회가 `beginTransaction`보다 앞에 있고 `FOR UPDATE`도 없어, 대조 시점과 `DELETE` 전체 + 재INSERT 시점 사이에 낀 쓰기를 낡은 스냅샷이 덮었다(갱신 손실 — 늦게 덮으면 레이드에서 잃은 아이템이 되살아나고, 먼저 덮이면 동시 구매 하나가 사라진 채 돈만 두 번 나간다). 조회를 트랜잭션 안으로 옮기고 `FOR UPDATE`를 걸어 끼어들 수 있는 넷(다른 구매 요청·`/match/start`·`/match/connect`·레이드 이탈 반영)을 전부 409로 수렴시켰다. 곁따라 DB가 필요 없는 샵 검사 셋을 커넥션 획득 전으로 올리고, 이제 트랜잭션 안에서 나가게 된 스냅샷 불일치 경로에 `rollback()`을 붙였다 (`routes/items.js`, `HTTPServer/CLAUDE.md`)
- [x] (2026-09-02 #4) `/api/items/sell` 신설 — 스냅샷을 판매 **전** 상태로 받아 대상 슬롯이 그 안에 있어야 하고(없으면 `ERR_SLOT_EMPTY`), 총량 대조를 통과하면 그 항목을 뺀 나머지를 써넣고 대금을 지급한다 — 구매의 `ERR_SLOT_OCCUPIED`와 정확히 반대 조건이고 트랜잭션·잠금 순서는 구매와 같다. 대상 슬롯 범위는 `0~107`이라 장착 중인 것도 팔 수 있고, 부분 판매는 클라이언트가 스택을 나눈 스냅샷을 보내는 것으로 처리해 `quantity` 파라미터를 두지 않았다. 구매와 글자 그대로 같던 스냅샷 형식·범위·중복 검사와 총량 대조는 `utils/inventorySnapshot.js`로 빼 두 라우트가 함께 쓴다 — `match.js`만 자체 사본을 유지한다 (`routes/items.js`, `utils/inventorySnapshot.js`, `http-api-spec.yaml`, `HTTPServer/CLAUDE.md`)
- [x] (2026-09-02 #5) `item_meta` 캐시에 `price` 추가와 필드명 문서 오류 정정 — 판매 대금의 단가를 `items.price`로 확정하면서(구매가와 같은 값), 시동 시 캐시를 만드는 `RedisHandler::InitializeItemCache()`의 SELECT·hmset에 `price`를 넣고 `/sell`이 `item_meta:<item_id>`에서 읽게 했다. `redis_keys.md`가 필드를 `item_name`/`item_type`/`description`으로 적고 있었으나 실제 키는 `name`/`type`/`desc`였다 — 그동안 이 캐시를 읽는 코드가 하나도 없어 드러나지 않던 오류이고, 문서를 보고 짠 코드는 전부 빈 값을 받았을 것이다. 가격 조회를 총량 대조 뒤에 둬 캐시 미스가 클라이언트 잘못이 아님을 확정한 뒤 500으로 처리한다 (`src/RedisHandler.cpp`, `routes/items.js`, `database/redis_keys.md`, `src/CLAUDE.md`, `HTTPServer/CLAUDE.md`)
- [x] (2026-09-02 #6) `/api/items/sell` 요청 본문을 구매와 같은 네 필드로 통일하고 스냅샷 대조를 추가 — 슬롯 번호 하나만 믿던 탓에 클라이언트가 슬롯 계산을 틀리면 엉뚱한 아이템이 조용히 팔리고 200이 나갔고(로그에도 정상 판매로 남아 사후 추적이 사실상 불가능하다), 이제 `item_id`·`quantity`를 함께 받아 대상 슬롯의 스냅샷 항목과 대조해 하나라도 다르면 트랜잭션 밖에서 `ERR_ITEM_MISMATCH`(400)로 끊는다. 악의적 클라이언트는 일관되게 거짓말하면 통과하므로 보안 장치가 아니라 클라이언트 버그 탐지기이고, `quantity`는 부분 판매 지시가 아니라 스택 전체에 대한 주장이라 구매의 검증 블록과 공유하면 안 된다(슬롯 범위 `0~107` vs `0~79`, `quantity` 상한 유무). `External_Protocol.proto` 변경이 없어 `LATEST_VERSION`은 올리지 않았고, 클라이언트의 판매 요청 빌더가 새 필드를 실을 때까지 구 클라이언트의 판매는 400으로 떨어진다 (`routes/items.js`, `http-api-spec.yaml`, `HTTPServer/CLAUDE.md`)
- [x] (2026-09-02 #7) 프리 로드아웃 입장 조건 신설과 `match.js` 스냅샷 검증 통일 — FREE는 스냅샷 영속(`/match/start`)도 `DELETE`(`/match/connect`)도 건너뛰던 탓에, 귀환 전리품을 창고로 옮긴 클라이언트의 로컬 재배치가 DB에 닿지 못한 채 레이드 종료의 `ApplyInventoryToDb()`(로드아웃 타입도 이탈 사유도 보지 않고 `slot_index >= 80`을 지운다)에 조용히 파괴됐다. FREE도 CUSTOM과 같은 대조·영속 경로를 타게 하고 스냅샷에 `slot_index >= 80` 항목이 있으면 `ERR_LOADOUT_NOT_EMPTY`(400)로 거부한다 — 영속을 함께 붙이지 않으면 배치를 DB에 반영하는 경로가 그 셋뿐이라 유저가 조건을 만족시킬 수단이 없다(창고 만석은 판매로 덜어내는 것이 정상 경로라 예외를 두지 않았고, `/match/connect`의 `DELETE`는 CUSTOM 조건을 유지한다). 겸사 `match.js`의 자체 형식 검증·총량 대조를 `utils/inventorySnapshot.js`로 합쳐 CUSTOM에도 `0~107` 상한이 생겼고, `inventory`가 두 모드 공통 필수가 되어 게스트를 포함한 FREE 요청이 빈 배열을 명시해야 한다 (`routes/match.js`, `http-api-spec.yaml`, `HTTPServer/CLAUDE.md`)
- [x] (2026-09-08 #0) 시동 시 Redis 키스페이스 파기 — 재시작하면 `token_`의 `fd`·`session_id`는 사라진 Dedicate 프로세스를 가리키고 `ticket_`은 비어서 새로 뜬 매치메이커 큐에 없어 영영 매칭되지 않으며 `active_match:`는 해제 주체(`NotifyPlayerLeftRequest::Execute()`)가 사라져 TTL 900초까지 잔류해 로그인·재매칭을 막았다. `RedisHandler::ResetKeyspace()`가 `InitializeItemCache()`보다 먼저 `FLUSHDB`로 db 0을 지우고 `guest_uid_counter`만 되쓰며, 실패하면 낡은 상태로 뜨지 않도록 기동을 포기한다(영속 데이터는 전부 MySQL에 있어 유실이 없다). 파기 대상을 키 패턴 목록으로 좁히지 않은 것은 목록에서 빠진 키만 조용히 살아남기 때문이고, 전제는 서버 인스턴스가 하나뿐이라는 것이며(둘이면 나중에 뜬 쪽이 먼저 뜬 쪽의 세션·락·티켓을 지운다) 귀결로 메인 프로세스 재시작이 곧 전원 강제 로그아웃이 됐다 (`src/RedisHandler.h/cpp`, `src/main.cpp`, `database/redis_keys.md`, `src/CLAUDE.md`)

### DB / 마이그레이션
- [x] (2026-09-03 #0) db-migrate 설정 예시에 `multipleStatements` 추가 — `20260825210551-initial-schema-up.sql`이 6개 문장이라 이 플래그 없이는 baseline 마이그레이션이 첫 문장에서 끊긴다. `database.json.example`에 넣으면서 함께 빠져 있던 쉼표를 채워 JSON 파싱 오류도 고쳤다. 실 `database.json`은 `.gitignore` 대상이라 예시만 고쳐서는 기존 환경이 그대로이므로 `local`·`production` 양쪽에 손으로 넣어야 한다 (`database/database.json.example`)

### 문서
- [x] (2026-09-07 #0) 프롬프트 피드백 로그 3건을 `.txt`에서 `.md`로 전환 — 같은 폴더의 `CC프롬프트4_서버-피드백.md`만 마크다운이라 확장자가 갈려 있었고, 평문은 GitHub에서 구조 없이 렌더돼 공개 목적에 맞지 않았다. 원문 문장은 그대로 두고 서식만 입혔으며(제목·인용·코드펜스·표), 3번 파일의 두 덩어리를 나누는 제목 한 줄과 따옴표 하나가 어긋난 자리만 손봤다. 프롬프트 원문 `.txt` 5건은 대상이 아니라 그대로 뒀다 (`docs/Claude_Code_프롬프트/CC프롬프트1~3_피드백.md`)

---

## 진행 중 / 다음 할 것들

### 진행 우선사항
1. **DB 작업 선행** — 아래 다섯 건은 서버 코드 변경 없이 DB 에서 끝나지만, 뒤의 컨텐츠 작업이 전부 이 값을 전제로 한다. 수치가 바뀌는 항목은 「마이그레이션 → `generate_script.py` → 빌드」 순서를 지킬 것(루트 CLAUDE.md 「데이터 원천 규율」)
   - ~~SCAR 추가~~ — **완료.** `ItemDataManager.h` 의 `_nameMap`·`_weaponSpecs` 에 id 3 이 이미 들어 있다(b9fa926)
   - 전술 아머 추가 (`items` + `armor_specs`) — `_armorSpecs` 는 아직 id 4(경량 조끼) 하나뿐이다
   - 아이템 설명 최신화
   - 스프레드 값 소폭 하향, 스프레드 회복값 상향
   - 상점 리스트를 실제값으로 채우기 — **마이그레이션은 올라갔다**(4c68b45). 남은 것은 두 프로세스 재시동이다 — 가격을 손대면 갱신 수단이 없는 캐시 둘(Node `shopCache`·Redis `item_meta`)이 낡은 값을 문다(아래 「진행 고려사항」 참조)
2. **첫 파괴 가능 오브젝트** — `CombatObject`의 파생이 `PlayerObject`뿐이라 피격→사망→회수→통보 경로가 한 번도 실행된 적이 없다(아래 「진행 고려사항」 참조). 서버 몫은 `ObjectType` 추가와 `ObjectTypeToName()` case 짝, `CombatObject` 파생 클래스 하나, 룸의 `SpawnDynamicObject()` 배치이고 킬·디스폰 통보(41·39)는 이미 붙어 있다. 착수 전에 기획 결정 셋이 필요하다 — 무엇을 만들지(컨테이너 겸 파괴 가능은 금지라 상자류 제외), 최대 HP(100배 스케일), 파괴 시 흔적을 남길지(남긴다면 `OnDeathResolved()` 안에서 별개 오브젝트로 스폰)

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
- **비플레이어 사망 경로는 아직 한 번도 실행되지 않는다** — `CombatObject`의 파생이 `PlayerObject`뿐이라 `FindNonplayerObject()`가 전투 오브젝트를 찾는 일이 없다. 검증용 더미 클래스를 만들지 않기로 했으므로 첫 파괴 가능 오브젝트를 만드는 시점에 피격→사망→회수→통보 전체를 그 자리에서 디버그할 것. 그때 지켜야 할 불변식 둘 — ① 회수 지시는 `TakeDamage()` 안이 아니라 **호출부가** 한다(안에서 `delete`하면 호출부가 직후에 읽는 `GetCurrentHp()`가 use-after-free) ② `OnDeathResolved()`에 `_dynamicObjects`의 반복자를 넘기지 않는다(훅이 흔적을 스폰하면 rehash로 무효화)

---
