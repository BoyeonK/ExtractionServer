# Extraction Shooter

Linux C++ 게임 서버와 Unity 클라이언트로 구축된 멀티플레이어 익스트랙션 슈터 게임 프로젝트입니다.
이 프로젝트는 비동기 네트워킹, 매치메이킹, 데디케이티드 서버, DB 연동 등을 설계 및 구현하고, 클라우드 환경으로의 배포함으로써 단순한 로컬 테스트 환경에 머무르지 않고 외부 사용자가 접속하고 플레이할 수 있는 온라인 멀티플레이 환경을 구축하는 데 중점을 두었습니다.

[다운로드 구글 드라이브 링크]

[게임 플레이 GIF / 영상]

## Overview

이 프로젝트는 게임 서버 아키텍처와 클라이언트 사이의 네트워킹 시스템을 직접 구현하기 위해 1인 개발로 진행된 멀티플레이어 게임 프로젝트의 서버측 부분입니다.
프로젝트를 단순히 로컬 환경에서 동작한다 수준이 아니라, 실제 공개된 환경에서 구동 가능한 멀티플레이어 게임을 목표로 합니다.

프로젝트에서 강조하고 싶은 영역들입니다.
- Linux C++ 멀티플레이어 서버 아키텍쳐
    - io_uring을 활용한 비동기 네트워킹
    - 메인 서버(C++), HTTPS API 서버(js), 그리고 다수의 데디케이티드 게임 서버 프로세스로 구성
- 커스텀 RUDP 전송 프로토콜
    - ACK 및 재전송 메커니즘을 가진 Reliable 채널 / 패킷 로스를 감수하는 Unreliable 채널 분리
    - HTTPS통신을 통해 교환된 키 기반 패킷 무결성 검증
- 매치메이킹 시스템
    - 플레이어의 공격성과 매치 대기 시간을 기반으로 한 매칭 시스템
- 동적 데디케이티드 프로세스 생성 및 관리
    - 액티브 유저 수에 따라 데디케이티드 프로세스를 동적으로 생성, 할당
- 퍼블릭 클라우드 환경으로의 배포
    - Cloudflare 리버스 프록시, Oracle Compute Instance, Redis 및 MySQL HeatWave 연동

## Architecture

![ExtractionServer Architecture](docs/diagrams/architecture.svg)

## Engineering Highlights

### 1. Multi-Process Server Architecture
서버의 역할과 장애 범위를 분리하기 위해 **Main Server, HTTP API Server, Dedicated Game Server**​를 각각 별도의 프로세스로 구성했습니다.

**Linux 기반 서버 환경**

이전 프로젝트를 AWS EC2의 제한된 메모리 환경에서 운영하려는 시도에서, 운영체제 자체의 메모리 사용량이 서버와 DB를 함께 구동하는 데 큰 제약이 되는 것을 경험했습니다.

이번 프로젝트에서는 동일한 제한된 클라우드 자원을 게임 서버에 더 많이 활용하기 위해 Linux 환경을 선택했습니다. 이는 Windows와 Linux의 일반적인 우열보다는, 프로젝트의 배포 환경과 자원 제약을 기준으로 한 선택입니다.

**HTTP API Server**

공개 인터넷에서 최초 접점이 되는 계정 생성 및 인증, 매치메이킹 요청, 게임 아이템 검증, 키 교환 등의 작업은 별도의 **Node.js HTTP API Server**에서 처리합니다.

외부 클라이언트는 Cloudflare Reverse Proxy를 통해 HTTPS API를 사용하며, 실시간 게임 서버와 인증·계정·API 영역의 책임을 분리했습니다.

이를 통해 Main Server는 매치메이킹과 프로세스 조율에, Dedicated Game Server는 실시간 게임 로직에 집중하도록 구성했습니다.

**Dedicated Game Server**

실제 클라이언트와 RUDP로 직접 통신하는 영역은 별도의 Dedicated Process로 격리했습니다.

각 Dedicated Process는 제한된 수의 플레이어와 GameRoom을 담당하며, 하나의 프로세스에서 발생한 장애가 다른 게임 세션이나 Main Server로 직접 확산되는 범위를 줄이도록 설계했습니다.

또한 Dedicated Process에는 Redis와 MySQL에 대한 직접 접근 권한을 두지 않았습니다. 데이터 작업이 필요한 경우 Unix Domain Socket IPC를 통해 Main Server의 **DB Proxy**에 요청하도록 구성했습니다.

이를 통해 Dedicated Process의 데이터 접근 범위를 제한하는 동시에, DB 작업과 실시간 게임 로직의 책임을 분리했습니다.

플레이어 수가 기존 Dedicated Process의 수용 범위를 넘어설 경우 Main Server가 새로운 Dedicated Process를 동적으로 생성하고, 준비가 완료된 프로세스에 새로운 게임 세션을 할당합니다.

### 2. Custom RUDP Transport
실시간 FPS 게임에서는 모든 패킷에 신뢰성을 적용하는 것(TCP)이 적합하지 않다고 판단했습니다.

위치나 방향처럼 지속적으로 갱신되는 데이터는 일부 패킷이 유실되더라도 이후의 최신 상태로 보완할 수 있습니다. 반면 중요한 내용을 포함한 패킷이, 아무런 증상 없이 사라져버리는 것 역시 허용하기 어렵습니다.

이 문제를 해결하기 위해 **GameNetworkingSockets의 전송 모델에서 영감을 받아 Reliable / Unreliable 채널을 분리한 자체 RUDP 전송 계층**을 구현했습니다.

- **Reliable Channel**
    - Sequence 기반 패킷 관리
    - ACK 처리
    - ACK를 받지 못한 패킷 재전송
- **Unreliable Channel**
    - 패킷 유실을 허용
    - 재전송으로 인한 지연 없이 최신 상태 전달을 우선

이를 통해 모든 패킷에 신뢰성을 강제하지 않으면서도, 반드시 전달되어야 하는 데이터에는 필요한 수준의 전달 보장을 적용할 수 있도록 구성했습니다.

또한 RUDP 패킷 전체를 별도로 암호화하지 않는 대신, 게임 접속 직전에 HTTPS를 통해 전달된 세션 키를 이용하여 패킷 Signature를 검증합니다. Signature 영역을 제외한 패킷 데이터를 기반으로 xxHash64 값을 계산하고, 수신된 Signature와 일치하지 않는 패킷은 처리하지 않고 폐기합니다.

현재 Signature 검증은 경량 패킷 검증을 목적으로 하며 암호학적으로 검증된 MAC을 대체하는 구조는 아닙니다. 반복적인 Signature 검증 실패에 대해 연결을 종료하는 정책 역시 향후 적용할 항목으로 남겨두었습니다.

### 3. io_uring Async Networking
Linux에서 비동기 네트워크 I/O를 구현하기 위해 epoll과 io_uring을 검토했습니다.

여러 방면으로 검토해 본 결과, 두 방식 중에서 어느쪽의 성능 우위를 확답할 수 없는 상황이었습니다. 따라서, 이전 프로젝트에서 사용했던 Windows IOCP의 작업 제출 → 완료 통지 → 후처리 구조와 유사한 형태로 서버를 설계할 수 있다는 점을 고려하여 io_uring을 선택했습니다.

I/O 작업의 종류와 관계없이 완료 처리를 통합하기 위해 IOTask라는 공통 인터페이스를 두고, UDP세션에서 사용할 Recv, Send, IPC세션에서 사용할 Recv, Send, Accept 등 각 작업의 성격에 따라 파생 Task를 구현했습니다.

각 IOTask는 I/O 완료 이후 실행할 callback과 후처리에 필요한 상태를 보관합니다. I/O 요청을 제출할 때 해당 Task를 SQE의 user_data와 연결하고, 완료된 CQE를 처리할 때 다시 Task를 찾아 callback을 실행하는 구조입니다.

IOTask \
↓ \
Submission Queue \
↓ \
Kernel I/O \
↓ \
Completion Queue\
↓ \
IOTask Callback\
↓ \
Post Processing

Recv/Send와 같은 I/O Task는 매우 높은 빈도로 생성되고 소멸할 것이 예상되었기 때문에, 반복적인 동적 메모리 할당을 줄이기 위해 Task 객체를 Object Pool로 관리하도록 구성했습니다.

## Matchmaking System

## Public Cloud Deployment

## Gameplay
[영상 / 스크린샷]

## Additional Work
- Unity Client
- Generative AI Asset Pipeline

## Tech Stack

## Documentation
- Architecture
- Networking
- Matchmaking
- Dedicated Server
- Deployment

## Repository
Client / Server