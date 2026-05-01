# src/ — Main C++ 프로세스

## 역할

Node.js ↔ DedicateProcess 간 IPC 라우팅, io_uring 기반 비동기 I/O,
Redis 프록시, DediManager(자식 프로세스 생성/관리).

## 주요 파일 위치

| 구성 요소 | 파일 |
|-----------|------|
| 메인 진입점 | `main.cpp` |
| 전역 상태 (IORing, Redis, DediManager) | `GlobalVariable.h` |
| io_uring 래퍼 | `IoUringWrapper.h/cpp` |
| IPC 소켓 관리 | `SocketWrapper.h/cpp` |
| HTTP/HTTPS 핸들러 | `HTTPserver.h/cpp` |
| Redis 핸들러 | `RedisHandler.h/cpp` |
| Dedicate 프로세스 관리 | `DediManager.h/cpp` |
| Redis 프록시 | `RedisProxyRequest.h/cpp`, `RedisProxyService.*` |
| CMake 빌드 설정 | `../CMakeLists.txt` |
