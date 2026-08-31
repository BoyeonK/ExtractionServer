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
