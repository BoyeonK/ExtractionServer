# HTTPServer/ — Node.js HTTPS 서버

## 역할

클라이언트 인증/인가, 매치메이킹 REST API,
IPC를 통해 Main C++ 프로세스와 통신, 게임 아이템/샵 관리.

## 주요 파일 위치

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
