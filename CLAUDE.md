# CLAUDE.md

## 세션 경계

- 이 세션의 작업 범위는 서버 코드베이스로 한정한다.
- 클라이언트 디렉터리(`Extraction/Extraction/`)의 코드는 읽지 않는다.
  수정뿐 아니라 열람도 하지 않는다. 컨텍스트 오염을 막기 위함이다.
- 클라이언트 측 원인으로 보이는 문제는 이 세션에서 해결하지 않는다.
  원인이 클라이언트에 있다고 판단되면 그렇게 보고하고 멈춘다.
- 클라이언트 코드와의 비교·대조가 필요하다고 판단되면, 직접 열지 말고
  상위 세션에 해당 작업을 요청할지 사용자에게 물어본다.

## 프로젝트 개요

실시간 멀티플레이어 게임 서버. C++ 메인 프로세스, Node.js HTTP 서버, C++ 전용 게임 프로세스(Dedicate)가 IPC 소켓으로 통신하는 멀티 프로세스 아키텍처.

```
Client
  ├─ HTTPS → Node.js (HTTPServer/) — 인증, 매치메이킹 REST API
  └─ UDP   → C++ DedicateProcess — 게임 로직 (이동, 전투 등)

Node.js ──IPC──▶ Main C++ Process (src/)
                  ├─ io_uring 기반 비동기 I/O
                  ├─ DB Proxy (자식 프로세스 대신 Redis·MySQL 처리)
                  └─ DediManager (전용 게임 프로세스 생성/관리)

Main C++ ──IPC──▶ DedicateProcess (src/DedicateProcess/)
                  └─ UDP 게임 세션 관리
```

## 디렉터리별 상세 문서

| 디렉터리 | CLAUDE.md | 내용 |
|-----------|-----------|------|
| Main C++ 프로세스 | `src/CLAUDE.md` | IPC 라우팅, io_uring, DB 프록시, DediManager |
| Dedicate 프로세스 | `src/DedicateProcess/CLAUDE.md` | UDP 패킷 형식, 게임룸, 플레이어, 아이템 |
| Node.js HTTP 서버 | `HTTPServer/CLAUDE.md` | 인증, REST API, 인벤토리 슬롯 구조 |

## 환경 변수

`HTTPServer/.env` 파일에 Redis, MySQL 연결 정보, 서버 포트, 로컬 테스트 여부(`IS_LOCAL_TEST`) 등이 정의된다.

## 프로토콜 참조

- **IPC 프로토콜**: `Protocol/IPCProtocol/` (IPC_HTTP.proto, IPC_Dedicate.proto, IPC_enum.proto)
- **외부 패킷 프로토콜**: `Protocol/ExternalProtocol/` (External_Protocol.proto, External_Unity_Object.proto)
- **Redis 키 구조**: `HTTPServer/database/redis_keys.md`
- **MySQL 스키마**: `HTTPServer/database/schema.sql`

## 진행 상황 추적

`progress.md` (루트)에서 완료된 작업, 진행 중인 작업, 다음 할 일을 관리한다.

## 개발 참고 사항

- **외부 라이브러리**: `myUtils` (BoyeonK/myUtils 깃 서브모듈), `abseil-cpp` (FetchContent)
- **third_party/**: 경량 의존성 소스 직접 포함 — `xxhash/` (xxhash.h, xxhash.c), `nlohmann/` (json.hpp)
- **배포**: Oracle Compute + MySQL HeatWave, Cloudflare 프론트엔드
- **언어**: 주석/변수명은 한국어 혼용
