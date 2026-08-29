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
