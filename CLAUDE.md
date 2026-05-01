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

## 컴포넌트별 상세 문서

각 서브시스템의 파일 목록 및 세부사항은 해당 디렉토리의 CLAUDE.md 참조:

- `src/CLAUDE.md` — Main C++ 프로세스 (io_uring, IPC 라우팅, Redis 프록시)
- `src/DedicateProcess/CLAUDE.md` — 전용 게임 프로세스 (UDP 세션, 게임룸, 매치메이킹)
- `HTTPServer/CLAUDE.md` — Node.js HTTPS 서버 (REST API, 인증, 인벤토리 구조)

## 진행 상황 추적

`progress.md` (루트)에서 완료된 작업, 진행 중인 작업, 다음 할 일을 관리한다.

## 개발 참고 사항

- **외부 라이브러리**: `myUtils` (BoyeonK/myUtils 깃 서브모듈)
- **배포**: AWS EC2 + RDS, Cloudflare 프론트엔드
- **언어**: 주석/변수명은 한국어 혼용
