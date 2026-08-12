# src/ — Main C++ 프로세스

역할: Node.js ↔ DedicateProcess 간 IPC 라우팅, io_uring 기반 비동기 I/O, Redis 프록시, DediManager(자식 프로세스 생성/관리).

```
Node.js ──IPC──▶ Main C++ Process (src/)
                  ├─ io_uring 기반 비동기 I/O
                  ├─ Redis Proxy (자식 프로세스 대신 Redis 처리)
                  └─ DediManager (전용 게임 프로세스 생성/관리)

Main C++ ──IPC──▶ DedicateProcess (src/DedicateProcess/)
```

## IPC 프로토콜

IPC 패킷 정의는 `Protocol/IPCProtocol/` 참조 (IPC_HTTP.proto, IPC_Dedicate.proto, IPC_enum.proto).

- Main ↔ Node.js: `IPC_HTTP.proto`
- Main ↔ Dedicate: `IPC_Dedicate.proto`

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
| Redis 핸들러 + RedisProxyService | `RedisHandler.h/cpp` |
| Redis 프록시 요청 브릿지 | `RedisProxyRequest.h/cpp` |
| 상시 MySQL 연결 (생존 확인·재연결) | `MysqlHandle.h/cpp` |
| Dedicate 프로세스 관리 | `DediManager.h/cpp` |
| Main→Dedicate IPC 세션 (M2DSession, M2DTempSession) | `M2DSessions.h/cpp` |
| 매치메이킹 알고리즘 (MatchTicket, MatchMaker) | `Matchmaker.h/cpp` |
| CMake 빌드 설정 | `../CMakeLists.txt` |
