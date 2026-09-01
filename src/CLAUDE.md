# src/ — Main C++ 프로세스

역할: Node.js ↔ DedicateProcess 간 IPC 라우팅, io_uring 기반 비동기 I/O, DB 프록시(Redis·MySQL), DediManager(자식 프로세스 생성/관리).

```
Node.js ──IPC──▶ Main C++ Process (src/)
                  ├─ io_uring 기반 비동기 I/O
                  ├─ DB Proxy (자식 프로세스 대신 Redis·MySQL 처리)
                  └─ DediManager (전용 게임 프로세스 생성/관리)

Main C++ ──IPC──▶ DedicateProcess (src/DedicateProcess/)
```

## IPC 프로토콜

IPC 패킷 정의는 `Protocol/IPCProtocol/` 참조 (IPC_HTTP.proto, IPC_Dedicate.proto, IPC_enum.proto).

- Main ↔ Node.js: `IPC_HTTP.proto`
- Main ↔ Dedicate: `IPC_Dedicate.proto`

## 불변식·확정 결정

- **`active_match` 락은 네 곳이 한 묶음** — `match.js`의 `ACTIVE_MATCH_TTL_SEC`, `matchCancel` Lua의 `return 2`/`return 3` 분기, `DBProxyRequest.h`의 `ACTIVE_MATCH_TTL_SEC`, `redis_keys.md` 2번 절. 불변식 셋: ① TTL(900초)은 최대 게임 길이보다 길 것 — TTL은 백스톱일 뿐이고 정상 해제는 룸 분리(`GameRoom::DetachPlayer()`) 한 곳이다 ② `match.js`와 `DBProxyRequest.h`의 `ACTIVE_MATCH_TTL_SEC`는 같은 값 ③ `INGAME:` 접두어는 `UpdateEntryTokenRequest`가 쓰고 `matchCancel`이 읽는다 — 티켓 TTL(300초)이 락 TTL보다 짧아 생기는 "티켓 없고 락만 남은" 구간을 이 접두어로만 구분한다.
- **불변식 ①은 이제 코드가 지키지만 짝은 손으로 맞춘다** — `GameRoom::ROOM_LIFETIME_MS`(600초)가 룸 수명을 상한 짓고, 락 해제는 룸 회수가 아니라 `DetachPlayer` 시점이라 만료 즉시 풀린다(TTL 900초까지 300초 여유). **`ROOM_LIFETIME_MS < ACTIVE_MATCH_TTL_SEC × 1000`을 깨지 말 것** — 두 값이 다른 프로세스에 있어 `static_assert`로 묶을 수단이 없다. 깨지면 락이 게임 도중 풀리고, 그 뒤 `NotifyPlayerLeftRequest::Execute()`의 `del("active_match:" + uid)`가 **값을 보지 않으므로** 그 사이 생성된 새 매치의 락을 지운다 — 더 나쁜 쪽은 락이 아니라 옛 세션의 `ApplyInventoryToDb()`가 새 매치의 `/match/start` 스냅샷을 덮어쓰는 것이다. 값-무검사 `del`은 지금 도달 불가일 뿐 방어는 없다.
- **MySQL과 Redis 사이에 공유 트랜잭션이 없다** — 이탈 반영은 **MySQL 먼저, 락 해제 나중**(fail-closed). 순서를 뒤집으면 DB 반영 실패 시 유저가 반영 안 된 인벤토리로 새 매치를 시작한다. 반영 실패 시 재시도 2회 후 락을 풀고 페이로드를 error 로그로 남긴다(영구 잠금 방지). 이 순서는 `NotifyPlayerLeftRequest::Execute()` 안에 갇혀 있다.
- **매치 종료 시 세션 TTL 재충전은 하지 않기로 확정** — 갱신 공백은 마지막 인증 요청(`/api/game/match/connect`)부터 복귀 후 `/api/session/resume`까지다. 세션 TTL 900초와 한 판 10분 미만이면 로딩·결과 화면에 5분 이상 남는다. 초과해도 실패 모드가 401 → 일반 로그인 폴백이라 인벤토리 유실이 없고(결과는 사망·귀환 시점에 이미 MySQL에 있다), 초과가 성립하는 경우는 결과 화면 체류가 긴 경우뿐이라 그때는 락이 이미 풀려 있어 재로그인이 `ERR_ALREADY_IN_GAME`에 막히지도 않는다. **붙인다면 자리는 `/match/connect`다** — `requireAuth`를 타서 `user_id`가 이미 손에 있다. `NotifyPlayerLeftRequest::Execute()`는 `db_id`만 알아 db_id → user_id 역참조 수단이 새로 필요하므로 후보에서 내렸다. 재검토 조건: 결과 화면 체류가 길어지는 UI 변경, 세션 TTL의 추가 축소, `GameRoom::ROOM_LIFETIME_MS`의 상향.
- **`item_meta:` 캐시는 이제 소비처가 있다 — 필드를 줄이면 Node 의 판매가 죽는다** — `RedisHandler::InitializeItemCache()`가 시동 시 `items` 를 읽어 채우는 이 해시의 `price` 를 Node 의 `POST /api/items/sell` 이 판매 대금 계산에 쓴다. 오래도록 아무도 읽지 않던 캐시라 필드를 줄이거나 이름을 바꿔도 티가 나지 않았지만, 지금은 그 순간 판매가 500 으로 실패한다 — 서버 빌드는 정상이고 C++ 쪽 로그에도 아무것도 남지 않는다. 재구축 지점이 시동 한 곳뿐이라 **MySQL 가격을 고쳐도 메인 프로세스를 다시 띄우기 전까지는 낡은 값이 쓰인다.** 필드 구성과 짝은 `HTTPServer/database/redis_keys.md` 4번.
- **`sql::Connection`은 자동 재연결이 없다** — MySQL을 쓰는 코드는 반드시 `pMysql->Get()`으로 핸들을 받을 것(그 안에서 `isValid()`→`reconnect()`가 처리된다). `sql::Connection*`를 어딘가에 캐시하면 `wait_timeout`(기본 8시간)·HeatWave 페일오버 후 모든 사용이 예외를 던진다.
- **IPC 프로토콜 파일을 추가하면 `CMakeLists.txt` 두 곳을 같이 고칠 것** — `PROJECT_SOURCES`의 `.pb.cc`/`.pb.h` 목록과 `IPC_PB_NAMES`. 한쪽만 고치면 컴파일 대상 누락 또는 `Protocol/Compiled/IPC/` → `src/IPCProtocol/` 복사 누락. External 프로토콜은 `PROJECT_SOURCES`만 보면 된다.
- **`_allocatedPlayers`의 증감은 두 곳뿐이고 짝이 맞는다** — 증가는 `M2DSession::AllocatePlayers()`, 감소는 `ReleasePlayers()`의 `player_count`(= 룸의 `_playerSessions.size()`). 둘이 같다는 보장 셋: ① `MatchMaker::VerifyAndSetMatchStatus()`의 Lua가 그룹 전원의 티켓 존재와 `WAITING`을 원자적으로 확인한 뒤 `INPROGRESS`로 바꾼다 ② Main은 단일 스레드이고 그 검증부터 `hgetall`까지 연속 호출이라 중간에 끼어드는 것이 없다 ③ 유일한 외부 삭제자인 Node `matchCancel` Lua는 `WAITING`만 `DEL`한다. **셋 중 하나라도 바꾸면 카운트가 어긋나 프로세스가 누적 증가한다.** 티켓 TTL 만료가 ①과 ② 사이 마이크로초에 걸리는 경우와 Redis eviction은 대응하지 않기로 했다.
- **전용 프로세스는 룸이 비어도 종료하지 않는다** — 서버 가동 중 관측된 최대 수용 인원만큼 유지하고 회수하지 않는다(fork·exec과 초기화 비용 회피). 유휴 프로세스도 `DedicateMain`의 1ms 슬립 루프로 초당 1000회 깨어나므로 수십 개 규모가 되면 룸 0개 백오프(1ms→50ms)가 필요해진다 — 예상 2개 규모라 보류.

## 파일 목록

| 구성 요소 | 파일 |
|-----------|------|
| 메인 진입점 | `main.cpp` |
| 전역 상태 (IORing, Redis, DediManager) | `GlobalVariable.h/cpp` |
| 환경변수 로드 | `EnvSetter.h/cpp` |
| io_uring 래퍼 | `IoUringWrapper.h/cpp` |
| 비동기 I/O 태스크 | `IOTask.h/cpp` |
| IPC 소켓 관리 | `SocketWrapper.h/cpp` |
| 네트워크 주소 유틸 | `NetAddress.h/cpp` |
| IPC 패킷 라우터 | `PacketHandler.h/cpp` |
| 수신 버퍼 | `RecvBuffer.h/cpp` |
| 송신 버퍼 | `SendBuffer.h/cpp` |
| 오브젝트 풀 | `ObjectPool.h/cpp` |
| HTTP/HTTPS 핸들러 | `HTTPserver.h/cpp` |
| Redis 아이템 캐시 구축 | `RedisHandler.h/cpp` |
| DB 프록시 (DBProxyService + 요청 객체들) | `DBProxyRequest.h/cpp` |
| 상시 MySQL 연결 (생존 확인·재연결) | `MysqlHandle.h/cpp` |
| Dedicate 프로세스 관리 | `DediManager.h/cpp` |
| Main→Dedicate IPC 세션 (M2DSession, M2DTempSession) | `M2DSessions.h/cpp` |
| 매치메이킹 알고리즘 (MatchTicket, MatchMaker) | `Matchmaker.h/cpp` |
| CMake 빌드 설정 | `../CMakeLists.txt` |
