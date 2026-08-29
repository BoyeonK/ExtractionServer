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

- **`active_match` 락은 네 곳이 한 묶음** — `match.js` `/start`의 EX 값, `matchCancel` Lua의 `return 2`/`return 3` 분기, `DBProxyRequest.h`의 `ACTIVE_MATCH_TTL_SEC`, `redis_keys.md` 2번 절. 불변식 셋: ① TTL(3600초)은 최대 게임 길이보다 길 것 — TTL은 백스톱일 뿐이고 정상 해제는 룸 분리(`GameRoom::DetachPlayer()`) 한 곳이다 ② `match.js`의 EX와 `ACTIVE_MATCH_TTL_SEC`는 같은 값 ③ `INGAME:` 접두어는 `UpdateEntryTokenRequest`가 쓰고 `matchCancel`이 읽는다 — 티켓 TTL(300초)이 락 TTL보다 짧아 생기는 "티켓 없고 락만 남은" 구간을 이 접두어로만 구분한다.
- **MySQL과 Redis 사이에 공유 트랜잭션이 없다** — 이탈 반영은 **MySQL 먼저, 락 해제 나중**(fail-closed). 순서를 뒤집으면 DB 반영 실패 시 유저가 반영 안 된 인벤토리로 새 매치를 시작한다. 반영 실패 시 재시도 2회 후 락을 풀고 페이로드를 error 로그로 남긴다(영구 잠금 방지). 이 순서는 `NotifyPlayerLeftRequest::Execute()` 안에 갇혀 있다.
- **매치 종료 시 세션 TTL 재충전은 하지 않기로 확정** — 갱신 공백은 `/api/game/match/start`부터 복귀 후 `/api/session/resume`까지이고, 정상 매치(≤15분)는 TTL 3600초와 자릿수가 다르다. 초과해도 실패 모드가 401 → 일반 로그인 폴백이라 인벤토리 유실이 없다(결과는 사망·귀환 시점에 이미 MySQL에 있다). 붙인다면 자리는 `NotifyPlayerLeftRequest::Execute()`지만 그 경로는 `db_id`만 알아 db_id → user_id 역참조 수단을 새로 만들어야 한다. 재검토 조건: 결과 화면 체류가 길어지는 UI 변경, 세션 TTL 대폭 축소.
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
