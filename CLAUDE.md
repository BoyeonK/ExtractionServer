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
```
[2B: Packet ID][2B: Session ID][4B: Sequence][4B: Security Key][1B: Flags]
```
페이로드 정의는 `Protocol/ExternalProtocol/` 참조 (External_Protocol.proto, External_Unity_Object.proto).

### Redis 키 구조
`HTTPServer/database/redis_keys.md` 참조.

### MySQL 스키마
`HTTPServer/database/schema.sql` 참조.

## 주요 파일 위치

| 구성 요소 | 파일 |
|-----------|------|
| C++ 메인 진입점 | `src/main.cpp` |
| 전역 상태 (IORing, Redis, DediManager) | `src/GlobalVariable.h` |
| io_uring 래퍼 | `src/IoUringWrapper.h/cpp` |
| IPC 소켓 관리 | `src/SocketWrapper.h/cpp` |
| HTTP/HTTPS 서버 | `src/HTTPserver.h/cpp` |
| Redis 핸들러 | `src/RedisHandler.h/cpp` |
| Dedicate 프로세스 관리 | `src/DediManager.h/cpp` |
| Redis 프록시 | `src/RedisProxyRequest.h/cpp`, `src/RedisProxyService.*` |
| 게임 서비스 코어 | `src/DedicateProcess/DediServerService.h/cpp` |
| UDP 세션 관리 | `src/DedicateProcess/DediSessions.h/cpp` |
| UDP 클라이언트 패킷 핸들러 | `src/DedicateProcess/ClientPacketHandler.h/cpp` |
| 플레이어 세션 | `src/DedicateProcess/PlayerSession.h/cpp` |
| 게임 룸 | `src/DedicateProcess/GameRoom.h/cpp` |
| 매치메이킹 알고리즘 | `src/DedicateProcess/Matchmaker.h/cpp` |
| Node.js 인증 라우트 | `HTTPServer/routes/auth.js` |
| Node.js 아이템 라우트 | `HTTPServer/routes/items.js` |
| Node.js 매치메이킹 라우트 | `HTTPServer/routes/match.js` |
| IPC 매니저 (Node.js) | `HTTPServer/ipc/ipcManager.js` |
| 샵 캐시 (Node.js) | `HTTPServer/config/shopCache.js` |
| API 명세 (OpenAPI) | `HTTPServer/http-api-spec.yaml` |
| CMake 빌드 설정 | `CMakeLists.txt` |

## 진행 상황 추적

`progress.md` (루트)에서 완료된 작업, 진행 중인 작업, 다음 할 일을 관리한다.

## 개발 참고 사항

- **외부 라이브러리**: `myUtils` (BoyeonK/myUtils 깃 서브모듈)
- **Swagger UI**: `IS_LOCAL_TEST=Y` 환경 변수 설정 시 활성화
- **배포**: AWS EC2 + RDS, Cloudflare 프론트엔드
- **언어**: 주석/변수명은 한국어 혼용
