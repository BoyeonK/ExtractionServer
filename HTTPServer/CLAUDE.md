# HTTPServer/ — Node.js HTTPS 서버

역할: 클라이언트 인증/인가, 매치메이킹 REST API, IPC를 통해 Main C++ 프로세스와 통신, 게임 아이템/샵 관리.

```
Client ──HTTPS──▶ Node.js (HTTPServer/)
                    — 인증, 매치메이킹 REST API

Node.js ──IPC──▶ Main C++ Process (src/)
                  (IPC_HTTP.proto)
```

## 환경 변수

`HTTPServer/.env` 파일에 Redis, MySQL 연결 정보, 서버 포트, 로컬 테스트 여부(`IS_LOCAL_TEST`) 등이 정의된다.

## IPC 프로토콜

Main 프로세스와의 통신은 `IPC_HTTP.proto` 참조. Node.js 측 사본은 `IPCProtocol.proto`, `IPC_HTTP.proto`.

## 데이터베이스

- **Redis 키 구조**: `database/redis_keys.md`
- **MySQL 스키마**: `database/schema.sql`

## 불변식·확정 결정

- **게스트는 MySQL에 행이 없고 그것을 FK가 강제한다** — `/api/guest`는 `guest_uid_counter`를 `DECR`해 음수 db_id를 발급하고 `users` 행을 만들지 않는다. `user_inventory.uid`의 FK가 음수 uid의 INSERT를 거부하므로 게스트의 재화·인벤토리는 어느 경로로도 영속되지 않는다(이탈 반영이 `_uid < 0`을 건너뛰는 것도 같은 이유). 게스트에게 인벤토리를 돌려주는 API의 **빈 배열은 오답이 아니라 정답**이다. 세션 데이터만 보고 DB 조회를 거는 새 엔드포인트는 `user_type`으로 먼저 가를 것. 게스트에게 영속 데이터를 주려는 요구가 생기면 그 지점은 API가 아니라 FK이며, 정식 계정 전환을 설계해야 한다.
- **수량의 권위는 DB, 배치의 권위는 클라이언트 스냅샷이다** — 클라이언트는 수량이 변하지 않는 조작(이동·정렬·스택 분할/병합)을 서버에 알리지 않고 로컬에서 처리하고, 수량을 바꾸는 요청에 인벤토리 전체 스냅샷을 실어 보낸다. 서버는 **아이템별 총량만** 대조하고(`slot_index`는 비교에 넣지 않는다) 통과하면 기존 행을 전부 지운 뒤 스냅샷대로 다시 넣는다 — 그래서 배치를 영속시키는 지점은 `/api/items/purchase`와 `/match/start`(CUSTOM) 둘뿐이다. **총량만 보는 것은 구멍이 아니라 이 구조의 필수 조건이다** — 위치를 대조하면 로컬 재배치가 전부 409로 튕긴다. 스냅샷을 행 단위로 검사하거나 DELETE 전체를 없애자고 제안하지 말 것. 배치 저장을 매치 흐름 밖으로 빼려면 전용 엔드포인트가 새로 필요하다. **실패 응답은 `data: null`이고 그대로 둔다** — 클라이언트가 실패 시 인벤토리를 재조회해 재시도하므로, 409에 서버의 현재 인벤토리·잔액을 실어 왕복을 줄이자고 제안하지 말 것.
- **스냅샷 대조부터 재작성까지는 한 트랜잭션이고 잠금 순서는 `user_inventory` → `users` 다** — 총량 대조(compare)와 `DELETE` 전체 + 재INSERT(swap)가 갈라지면 그 사이에 낀 쓰기를 낡은 스냅샷이 덮는다(갱신 손실. 늦게 덮으면 레이드에서 잃은 아이템이 되살아나고, 먼저 덮이면 동시 구매 하나가 사라진 채 돈만 두 번 나간다). 두 라우트 모두 조회를 트랜잭션 안에서 `FOR UPDATE`로 걸어 막으며, `WHERE uid = ?`가 `uq_uid_slot`의 선두 컬럼을 타 넥스트키 잠금이 같은 uid의 INSERT까지 막는다 — **조회를 트랜잭션 밖으로 빼지 말 것.** 끼어들 수 있는 쓰기는 다른 구매 요청·`/match/start`·`/match/connect`의 `DELETE`·레이드 이탈 반영(C++) 넷이고, 잠금 아래에서 읽으면 전부 `ERR_SNAPSHOT_MISMATCH`(409)로 수렴한다. `users`를 잠그는 곳은 현재 구매 하나뿐이라 교착 고리가 없으니, **반대 순서로 잠그는 경로를 만들지 말 것.** `/purchase`에 `active_match` 검사가 없어 게임 중 세션의 구매를 막지는 않지만 정확성은 이 잠금으로 확보된다.
- **판매는 대상 슬롯의 스택 전체를 팔고, 영역을 가리지 않는다** — `/api/items/sell`의 스냅샷은 **판매 전** 상태라 대상 슬롯이 그 안에 있어야 하고(없으면 `ERR_SLOT_EMPTY`), 서버는 그 항목을 뺀 나머지를 써넣는다 — 구매의 `ERR_SLOT_OCCUPIED`와 정확히 반대 조건이다. 대상 슬롯 범위는 `0~107`로 창고 전용인 구매와 다르다(장착 중인 것도 팔 수 있다). **부분 판매용 `quantity` 파라미터를 만들지 말 것** — 클라이언트가 스택을 나눈 스냅샷을 보내면 되고, 수량 불변 조작이라 기존 구조가 그대로 지원한다. 지급액은 스냅샷의 수량에서 읽지만 총량 대조가 아이템별 합계를 고정하고 나머지가 다시 써지므로 가진 것보다 많이 받을 수 없다. **단가의 출처는 Redis `item_meta:<item_id>`의 `price`이고 이것을 채우는 쪽은 C++ 메인 프로세스다**(`RedisHandler::InitializeItemCache()`, 시동 시 1회) — Node 에서 `items` 를 직접 읽지 않는 유일한 자리이니 캐시가 비면 판매가 500 으로 실패하고, MySQL 가격을 고쳐도 메인 프로세스 재시동 전까지는 낡은 값이 쓰인다. 구매가와 판매가가 같은 `items.price` 라는 것도 현재의 확정 사항이다. 필드 구성은 `database/redis_keys.md` 4번.
- **구매한 아이템은 창고(0~79)에만 놓을 수 있다** — `/api/items/purchase`의 `slot_index`가 창고 밖이면 `ERR_INVALID_SLOT`(400). 구매품이 정리 단계를 거치지 않고 곧바로 레이드에 나가거나 장착되는 것을 막기 위함이고, 창고가 만석이면 구매가 거부되므로 **클라이언트가 빈칸 없음을 먼저 막는 것과 짝이다**. 스냅샷 항목 쪽은 배치 통로라 위치를 자유롭게 두되 범위만 `0~107`로 막는다 — 세 영역 어디에도 없는 슬롯은 어느 조회 경로도 읽지 않는 유령 행이 된다. **`/match/start`의 CUSTOM 경로는 아직 이 상한이 없다** — 같은 형태의 스냅샷을 받지만 검사 강도가 다르니 한쪽을 보고 나머지를 짐작하지 말 것 — `items.js`의 두 라우트는 `utils/inventorySnapshot.js`를 함께 쓰고 `match.js`만 자체 사본을 들고 있다. 범위 상수의 출처는 `utils/slotLayout.js` 하나다.
- **`active_match` 락 규약은 `src/CLAUDE.md` 참조** — `match.js`의 EX 값과 `matchCancel` Lua가 그 네 곳 묶음에 속한다.
- **세션 TTL의 출처는 `config/redisClient.js`의 `SESSION_TTL_SEC` 하나** — `sess:`와 `user_sess:`는 항상 같은 TTL이어야 한다. `user_sess:`가 먼저 만료되면 `loginTakeover`가 옛 세션을 찾지 못해 파기하지 못하고 한 계정에 유효 세션이 둘 남는다. 값의 근거와 여유 계산은 `database/redis_keys.md` 3번. `active_match`의 TTL과 우연히 같은 값이지만 근거가 달라 함께 움직이지 않는다.
- **전역 `unhandledRejection` 핸들러가 없다 — await 없이 부르는 프라미스는 프로세스를 죽인다** — Node 18+ 의 기본값이 미처리 프라미스 거부 시 종료이고 `index.js`에 핸들러가 없어, 떠 있는 프라미스 하나의 거부가 요청 하나가 아니라 **서버 전체**를 내린다. `config/redisClient.js`의 `redisClient.on('error')`는 클라이언트 error 이벤트만 받을 뿐 개별 명령 프라미스의 거부와는 무관하다. 현재 await하지 않는 호출은 `requireAuth`의 `refreshSession(...).catch(() => {})` 하나뿐이고 — TTL 갱신은 실패해도 요청을 막지 않는 성격이라 의도적으로 await하지 않는다 — **그 `.catch()`를 떼지 말 것. 새로 추가하는 비동기 호출은 await하거나 catch를 붙인다.**
- **401은 이유를 구분해 주지 않는다 — `/login`의 두 갈래만 예외다** — `requireAuth`의 401은 자연 만료·중복 로그인 축출·위조 세션 ID 셋에 **같은 응답**을 낸다(`middleware/auth.js`). 셋 다 클라이언트 대응이 재로그인 하나라 구분이 기능적 가치가 없고, 구분을 주면 세션 ID를 쥔 쪽에 "존재했으나 만료됨"과 "애초에 없음"을 알려주는 것이 된다. **401에 구분용 `error.code`를 붙이거나 축출 통보 채널(로비 폴링·tombstone 키)을 신설하자고 제안하지 말 것** — 축출된 클라이언트가 받는 유일한 신호가 401인 것은 미결이 아니라 확정이다. 서버 쪽 추적은 `[Auth] 기존 접속 끊기` 로그로 한다. 예외는 `/login`의 401 둘(없는 ID·틀린 비밀번호)로, 문구를 합치면 유저가 로그인 실패 원인을 알 수 없어 플레이 경험을 해친다 — 계정 열거는 `/signup`의 409가 구조상 같은 정보를 주므로 합쳐도 어차피 막히지 않는다.
- **`/api/version`은 강제가 아니라 통보다** — `LATEST_VERSION`·`IS_MAINTENANCE`(`index.js`)는 응답에 실릴 뿐이고, 버전이 낡은 클라이언트가 `/api/login`이나 `/match/start`를 그냥 불러도 서버는 막지 않는다. 차단은 클라이언트가 최초 실행 시 문자열을 비교해 스스로 종료하는 것에 전적으로 의존하므로 **정상 클라이언트에만 통한다**. 점검도 같아서 `IS_MAINTENANCE`가 `true`여도 다른 API는 열려 있고, 코드 상수라 점검을 켜려면 재배포가 필요하다 — 알파 단계의 감수 사항이고 옮길 자리는 Redis다. **점검 중에도 응답은 200 + `isMaintenance: true`이고 503이 아니다** — 이 엔드포인트의 성패는 "상태 조회가 됐는가"이지 "서비스 중인가"가 아니며, 503을 내면 클라이언트가 점검과 서버 다운·네트워크 실패를 구분하지 못해 점검 안내 화면을 띄울 수 없다. 서버 측 강제가 필요해지면 자리는 `/api/login`이다(entry token 발급이 로그인을 타므로 UDP 경로까지 한 곳에서 닫힌다). 버전 증가 규칙은 루트 `CLAUDE.md` 「코드·문서 규율」 참조.
- **중복 로그인은 "게임 중이면 거부, 아니면 인수인계"** — `/api/login`의 `loginTakeover` Lua가 비밀번호 검증 직후 `active_match:<db_id>`를 보고 갈린다. 판정표와 설계 배경은 `database/redis_keys.md` 3번. 이 락은 이제 *매칭 차단*뿐 아니라 *로그인 차단*도 뜻하므로, 락의 값·수명·해제 시점을 건드리면 로그인 가능 여부가 함께 움직인다. 판정을 비밀번호 검증 **뒤에** 두는 것도 규약이다 — 앞에 두면 남의 계정이 게임 중인지가 인증 없이 샌다.

## 파일 목록

| 구성 요소 | 파일 |
|-----------|------|
| 서버 진입점 | `index.js` |
| IPC 매니저 | `ipc/ipcManager.js` |
| 인증 미들웨어 (세션 검증) | `middleware/auth.js` |
| 인증 라우트 | `routes/auth.js` |
| 아이템 라우트 | `routes/items.js` |
| 매치메이킹 라우트 | `routes/match.js` |
| Redis 클라이언트 | `config/redisClient.js` |
| MySQL 클라이언트 | `config/mysqlClient.js` |
| 샵 캐시 | `config/shopCache.js` |
| API 명세 (OpenAPI) | `http-api-spec.yaml` |
| Redis 키 유틸 | `utils/redisKeys.js` |
| 슬롯 레이아웃 상수 | `utils/slotLayout.js` |
| 인벤토리 스냅샷 검증·총량 대조 | `utils/inventorySnapshot.js` |
| 표준 HTTP 응답 포맷 | `utils/response.js` |
| MySQL 스키마 | `database/schema.sql` |
| Redis 키 구조 | `database/redis_keys.md` |
| IPC 프로토콜 정의 (Node.js 측 사본) | `IPCProtocol.proto`, `IPC_HTTP.proto` |

## 인벤토리 슬롯 구조 (`user_inventory.slot_index`)

| 범위 | 영역 |
|------|------|
| 0 ~ 79 | warehouse (창고) |
| 80 ~ 104 | inventory (인벤토리) |
| 105 ~ 107 | loadout (장착 슬롯) |

## Swagger UI

`IS_LOCAL_TEST=Y` 환경 변수 설정 시 활성화.
