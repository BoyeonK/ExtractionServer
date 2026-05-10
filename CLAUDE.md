# CLAUDE.md

## 프로젝트 개요

실시간 멀티플레이어 게임 서버. C++ 메인 프로세스, Node.js HTTP 서버, C++ 전용 게임 프로세스(Dedicate)가 IPC 소켓으로 통신하는 멀티 프로세스 아키텍처.

```
Client
  ├─ HTTPS → Node.js (HTTPServer/) — 인증, 매치메이킹 REST API
  └─ UDP   → C++ DedicateProcess — 게임 로직 (이동, 전투 등)

Node.js ──IPC──▶ Main C++ Process (src/)
                  ├─ io_uring 기반 비동기 I/O
                  ├─ Redis Proxy (자식 프로세스 대신 Redis 처리)
                  └─ DediManager (전용 게임 프로세스 생성/관리)

Main C++ ──IPC──▶ DedicateProcess (src/DedicateProcess/)
                  └─ UDP 게임 세션 관리
```

## 환경 변수

`HTTPServer/.env` 파일에 Redis, MySQL 연결 정보, 서버 포트, 로컬 테스트 여부(`IS_LOCAL_TEST`) 등이 정의된다.

## 아키텍처 핵심 사항

### IPC 프로토콜
IPC 패킷 정의는 `Protocol/IPCProtocol/` 참조 (IPC_HTTP.proto, IPC_Dedicate.proto, IPC_enum.proto).

### UDP 패킷 형식 (클라이언트 ↔ Dedicate)
UDPHeader 35B 고정 헤더 + 페이로드(protobuf).
```
[8B: signature]
[2B: Packet ID][2B: Session ID]
[4B: rSeqNum][2B: uSeqNum][1B: Flags]
[4B: ackRSeqNum][4B: ackBitfield]
[4B: timestamp][4B: timestampEcho]
```
- `signature`: xxHash64(전체패킷(signature=0) + secKey) MAC. 서명 계산 시 이 필드는 0으로 세팅
- `rSeqNum`: reliable 채널 전용 시퀀스 (`FLAG_RELIABLE` 패킷에만 유효)
- `uSeqNum`: unreliable 채널 전용 시퀀스 (그 외 패킷에만 유효, wrap-around는 signed 차 비교로 처리)
- `ackRSeqNum`/`ackBitfield`: reliable 채널 ACK 피기백 (`FLAG_HAS_ACK` 세트 시 유효)

페이로드 정의는 `Protocol/ExternalProtocol/` 참조 (External_Protocol.proto, External_Unity_Object.proto).

### Redis 키 구조
`HTTPServer/database/redis_keys.md` 참조.

### MySQL 스키마
`HTTPServer/database/schema.sql` 참조.

## 컴포넌트별 파일 목록

### src/ — Main C++ 프로세스

역할: Node.js ↔ DedicateProcess 간 IPC 라우팅, io_uring 기반 비동기 I/O, Redis 프록시, DediManager(자식 프로세스 생성/관리).

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
| Redis 핸들러 + RedisProxyService | `RedisHandler.h/cpp` |
| Redis 프록시 요청 브릿지 | `RedisProxyRequest.h/cpp` |
| Dedicate 프로세스 관리 | `DediManager.h/cpp` |
| CMake 빌드 설정 | `../CMakeLists.txt` |

### src/DedicateProcess/ — 전용 게임 프로세스

역할: UDP를 통한 클라이언트 통신, 플레이어/세션/게임룸 관리, 매치메이킹 알고리즘 실행, 아이템 시스템.

| 구성 요소 | 파일 |
|-----------|------|
| 프로세스 진입점 | `DedicateMain.h/cpp` |
| 전역 상태 (pDediServer, pTimerExecuter, pItemDataManager) | `DedicateGlobalVariable.h/cpp` |
| 게임 서비스 코어 | `DediServerService.h/cpp` |
| UDP 세션 관리 | `DediSessions.h/cpp` |
| UDP 클라이언트 패킷 핸들러 | `ClientPacketHandler.h/cpp` |
| 플레이어 세션 | `PlayerSession.h/cpp` |
| 플레이어 상태 | `Player.h/cpp` |
| 게임 룸 | `GameRoom.h/cpp` |
| 타이머 스케줄러 | `TimerExecuter.h/cpp` |
| 매치메이킹 알고리즘 | `Matchmaker.h/cpp` |
| 게임 아이템 | `Items.h/cpp` |
| 아이템 스펙 데이터 관리 | `ItemDataManager.h` |
| Unity 게임 오브젝트 (베이스) | `UnityGameObjects/UnityGameObject.h/cpp` |
| Unity 게임 오브젝트 (구체 타입) | `UnityGameObjects/TestGameObjects.h/cpp` |
| UDP 태스크 | `UDPTask.h/cpp` |
| 열거형 | `enum.h` |
| 외부 패킷 프로토콜 (컴파일 결과) | `ExternalProtocol/` |

### HTTPServer/ — Node.js HTTPS 서버

역할: 클라이언트 인증/인가, 매치메이킹 REST API, IPC를 통해 Main C++ 프로세스와 통신, 게임 아이템/샵 관리.

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

#### 인벤토리 슬롯 구조 (`user_inventory.slot_index`)

| 범위 | 영역 |
|------|------|
| 0 ~ 79 | warehouse (창고) |
| 80 ~ 104 | inventory (인벤토리) |
| 105 ~ 107 | loadout (장착 슬롯) |

#### Swagger UI

`IS_LOCAL_TEST=Y` 환경 변수 설정 시 활성화.

## 진행 상황 추적

`progress.md` (루트)에서 완료된 작업, 진행 중인 작업, 다음 할 일을 관리한다.

## 개발 참고 사항

- **외부 라이브러리**: `myUtils` (BoyeonK/myUtils 깃 서브모듈)
- **배포**: AWS EC2 + RDS, Cloudflare 프론트엔드
- **언어**: 주석/변수명은 한국어 혼용
