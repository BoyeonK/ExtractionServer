## 개요

이 저장소는 1인 개발로 진행한 멀티플레이어 Extraction Shooter 프로젝트의 **서버 측 구현**입니다.

클라이언트의 상세 구현, 게임 Asset 제작 과정 및 플레이 영상은 [ExtractionClient](https://github.com/BoyeonK/ExtractionClient) 저장소에서 확인할 수 있습니다.

Linux C++ 기반의 실시간 게임 서버를 중심으로 HTTP API, Matchmaking, Dedicated Game Server, 데이터 계층 및 Public Cloud 배포 환경까지 구성했습니다.

> 실제 배포 환경에서 플레이 가능한 클라이언트 빌드를 제공합니다.  
> [게임 클라이언트 다운로드 - Google Drive](https://drive.google.com/file/d/1jEZZuNcX1D1u2ui_NkjkqWleFZ8hI3tX/view?usp=sharing)

## Tech Stack

| Area | Technology |
| --- | --- |
| Game Server | C++17, Ubuntu 24.04 LTS, `io_uring` |
| Client | Unity, C# |
| HTTP API | Node.js, Express |
| Realtime Transport | UDP, Custom RUDP |
| Internal IPC | Unix Domain Socket |
| Serialization | Protocol Buffers |
| Data | Redis, MySQL HeatWave |
| Infrastructure | Oracle Cloud, Cloudflare |
| Previous Deployment | AWS EC2, AWS RDS |

## 주요 구현

- **Linux C++ Multiplayer Server Architecture**
  - `io_uring` 기반 비동기 네트워킹
  - Main Server, Node.js HTTP API Server, 다수의 Dedicated Game Server Process로 구성
- **Custom RUDP Transport**
  - ACK 및 재전송을 지원하는 Reliable Channel
  - 패킷 유실을 허용하고 최신 상태 전달을 우선하는 Unreliable Channel
  - HTTPS를 통해 공유한 세션 키 기반의 경량 Packet Signature 검증
- **Matchmaking System**
  - 플레이어의 공격 성향과 대기시간을 기반으로 한 Matchmaking
- **Dynamic Dedicated Process Management**
  - 수용 가능한 Capacity에 따라 Dedicated Process를 동적으로 생성하고 GameRoom 할당
- **Game State & Item Lifecycle**
  - Lobby의 영속 상태와 Match 내부의 일시적인 상태를 분리하여 관리
- **Public Cloud Deployment**
  - Cloudflare Reverse Proxy
  - Oracle Compute Instance
  - Redis
  - MySQL HeatWave

## 아키텍처

![ExtractionServer Architecture](docs/diagrams/architecture.svg)

## Engineering Highlights

### 1. 멀티 프로세스 서버

서버의 역할과 장애 범위를 분리하기 위해 **Main Server, HTTP API Server, Dedicated Game Server**를 각각 별도의 프로세스로 구성했습니다.

#### HTTP API 서버

공개 인터넷에서 최초 접점이 되는 계정 생성 및 인증, Matchmaking 요청, 게임 아이템 검증, 게임 접속 준비 및 키 교환 등의 작업은 별도의 **Node.js HTTP API Server**에서 처리합니다.

외부 클라이언트는 Cloudflare Reverse Proxy를 통해 HTTPS API를 사용하며, 실시간 게임 서버와 인증·계정·API 영역의 책임을 분리했습니다.

이를 통해 Main Server는 Matchmaking 및 Process 조율에, Dedicated Game Server는 실시간 Game Logic 처리에 집중하도록 구성했습니다.

#### Dedicated 서버

실제 클라이언트와 Custom RUDP로 직접 통신하는 영역은 별도의 Dedicated Process로 격리했습니다.

각 Dedicated Process는 제한된 수의 플레이어와 여러 **GameRoom**을 관리합니다. GameRoom은 진행 중인 한 판의 게임 단위이며, 하나의 Dedicated Process에서 발생한 장애가 다른 Process의 GameRoom이나 Main Server로 직접 확산되는 범위를 줄이도록 구성했습니다.

또한 Dedicated Process에는 Redis와 MySQL에 대한 직접 접근 권한을 두지 않았습니다.

데이터 작업이 필요한 경우 Unix Domain Socket IPC를 통해 Main Server의 **DB Proxy**에 요청하도록 구성하여, Dedicated Process의 데이터 접근 범위를 제한하고 DB 작업과 실시간 Game Logic의 책임을 분리했습니다.

기존 Dedicated Process가 추가 세션을 수용할 수 없는 경우 Main Server가 새로운 Dedicated Process를 생성하고, 초기화가 완료된 프로세스에 새로운 GameRoom을 할당합니다.

#### Details

> [Dedicated Server](docs/dedicated-server.md) — 설계 의도와 구현은 분량이 많아 별도의 문서에 서술합니다.

### 2. Custom RUDP Transport

실시간 FPS 게임에서는 TCP처럼 모든 데이터에 동일한 순서 보장과 재전송을 적용하는 방식이 적합하지 않다고 판단했습니다.

위치나 방향처럼 지속적으로 갱신되는 데이터는 일부 패킷이 유실되더라도 이후의 최신 상태로 보완할 수 있습니다. 반대로, 반드시 전달되어야 하는 중요한 데이터가 별도의 처리 없이 유실되는 것은 허용하기 어렵습니다.

이 문제를 해결하기 위해 **GameNetworkingSockets의 전송 모델에서 영감을 받아 Reliable / Unreliable Channel을 분리한 자체 RUDP 전송 계층**을 구현했습니다.

- **Reliable Channel**
  - Sequence 및 Selective ACK 기반 전달 확인
  - RTT 추정값을 반영한 Retransmission
- **Unreliable Channel**
  - 재전송 없이 최신 상태 전달 우선

이를 통해 모든 패킷에 신뢰성을 강제하지 않으면서도, 반드시 전달되어야 하는 데이터에는 필요한 수준의 전달 보장을 적용하도록 구성했습니다.

#### Details

> [Networking](docs/networking.md) — 설계 의도와 구현은 분량이 많아 별도의 문서에 서술합니다.

### 3. `io_uring` Async Networking

Linux에서 비동기 네트워크 I/O를 구현하기 위해 `epoll`과 `io_uring`을 검토했습니다.

두 방식 중 단순한 성능 우위를 전제로 선택하기보다는, 이전 프로젝트에서 사용했던 Windows IOCP의 **작업 제출 → 완료 통지 → 후처리** 구조와 유사한 형태로 서버를 설계할 수 있다는 점을 고려하여 `io_uring`을 선택했습니다.

I/O 작업의 종류와 관계없이 완료 처리를 통합하기 위해 `IOTask`라는 공통 인터페이스를 두고, UDP Session의 Recv / Send, IPC Session의 Recv / Send / Accept 등 각 작업의 성격에 따라 파생 Task를 구현했습니다.

각 `IOTask`는 I/O 완료 이후 실행할 Callback과 후처리에 필요한 상태를 보관합니다.

I/O 요청을 제출할 때 해당 Task를 SQE의 데이터와 연결하고, 완료된 CQE를 처리할 때 다시 Task를 찾아 Callback을 실행합니다.

```text
IOTask
  ↓
SQE (user_data → IOTask)
  ↓
Submission Queue
  ↓
Kernel I/O
  ↓
Completion Queue (CQE)
  ↓
user_data → IOTask
  ↓
Callback / Post Processing
```

Recv / Send와 같은 I/O Task는 높은 빈도로 생성되고 소멸할 것이 예상되었기 때문에, 반복적인 동적 메모리 할당을 줄이기 위해 Task 객체를 **Object Pool**로 관리하도록 구성했습니다.

### 4. Matchmaking & State Consistency

ARC Raiders를 플레이하며 체감한 플레이어 간 우호·공격 성향에 따른 게임 경험에서 영감을 받아, 다른 플레이어를 얼마나 공격적으로 대하는지를 나타내는 `aggression`을 주요 지표로 사용했습니다.

MatchMaker의 목표는 비슷한 `aggression`을 가진 플레이어를 최대한 같은 Room에 배치하는 것입니다.

다만 매칭 품질 때문에 사용자를 무한히 대기시키지 않도록 대기시간이 증가할수록 필요한 인원 조건을 단계적으로 완화하고, 최종적으로 Solo Match도 허용합니다.

반면 `aggression` 차이는 일정 범위 이상 완화하지 않습니다. 지나치게 다른 성향의 플레이어를 억지로 같은 Room에 배치하기보다는 인원이 적더라도 게임을 시작시키는 편이 더 적합하다고 판단했습니다.

#### Match State Consistency

Match Cancel 요청과 MatchMaker의 결정은 동시에 발생할 수 있기 때문에 C++ 메모리의 상태만을 최종 상태로 사용하지 않습니다.

Redis의 Match Ticket을 Source of Truth로 사용하며, Lua Script를 통해 Match Group 전체가 여전히 `WAITING` 상태인지 원자적으로 검증한 뒤 `INPROGRESS`로 전환합니다.

```text
WAITING
   │
   │ Match 확정
   ▼
INPROGRESS
   │
   │ Dedicated Server 할당
   │ GameRoom 편입 완료
   ▼
SUCCESS
```

- **WAITING**
  - Matchmaking 대기 상태
  - 사용자 Match Cancel 가능
- **INPROGRESS**
  - Main Server가 Match Group을 확정한 상태
  - 이후에는 사용자 Match Cancel 불가
  - Main Server가 주도하는 단일 처리 흐름을 따름
- **SUCCESS**
  - Dedicated Server에 할당되고 GameRoom 편입까지 완료된 상태

이를 통해 MatchMaker가 Match Group을 구성하는 시점과 사용자의 Match Cancel 요청이 경쟁하는 상황에서도 일관된 상태 전이를 유지하도록 구성했습니다.

> [Matchmaking](docs/matchmaking.md) — 설계 의도와 구현은 분량이 많아 별도의 문서에 서술합니다.

### 5. Game State & Persistence

Extraction Shooter의 특성상 **Lobby의 영속 상태와 Match 내부의 일시적인 상태를 분리**하여 관리합니다.

#### In-Game Item Lifecycle

Lobby의 Inventory는 MySQL에 영속화하며, Match 진입이 확정된 Item은 DB에서 제거한 뒤 Dedicated Game Server의 메모리 상태로 관리합니다.

```text
Lobby Inventory (MySQL)
        │
        │ Enter Match
        ▼
Dedicated Game Server Memory
        │
        ├── Death / Disconnect → Item Lost
        │
        └── Successful Extraction
                    │
                    ▼
              Main DB Proxy
                    │
                    ▼
          Lobby Inventory (MySQL)
```

사망이나 연결 종료 시 Item은 소실되며, 정상적으로 탈출한 경우에만 Dedicated Server가 보유한 Item State를 Main Server의 DB Proxy를 통해 다시 영속 Inventory에 반영합니다.

이를 위해 플레이어의 Match 참여 상태와 GameRoom 이탈 사유, Item State의 소유 영역이 일관되게 전환되도록 Lifecycle을 관리합니다.

#### Inventory & Session Consistency

Lobby의 단순 Item 배치 변경은 즉시 DB에 반영하지 않고, 실제 Item 수량이 변경되는 시점에 클라이언트의 Inventory Snapshot과 서버의 영속 데이터를 비교하여 유효성을 검증합니다. 검증에 성공한 경우에만 변경을 반영하여 **Item 수량 변경의 최종 결정은 서버가 담당**하도록 구성했습니다.

사용자의 로그인 및 게임 참여 상태는 Redis로 관리합니다. 동일 계정이 이미 게임 중인 경우 새로운 로그인을 거부하고, 게임 중이 아니라면 기존 Session을 폐기한 뒤 새로운 Session으로 교체합니다. 탈출·사망·연결 종료 시에는 Player Session, 게임 참여 상태 및 GameRoom 자원이 함께 정리되도록 관리합니다.

> [GameState & Persistence](docs/gamestate-persistence.md) — Player에 대한 영속 데이터, Item Lifecycle, Inventory 검증 및 Player Session 관리 방식은 별도 문서에서 설명합니다.

> [Authentication](docs/authentication.md) — Player에 대한 영속 데이터중 계정과 인증, 로그인 정보에 대한 내용들은 여기서 자세히 다룹니다.

### 6. Public Cloud Deployment

프로젝트를 로컬 환경에서만 동작하는 프로토타입에 머무르게 하지 않고, **실제 외부 클라이언트가 접속하여 플레이할 수 있는 Public Cloud 환경까지 구성하고 배포했습니다.**

초기에는 **AWS EC2 + RDS** 환경에서 서버를 운영했으며, 이후 현재의 **Oracle Compute Instance + MySQL HeatWave** 환경으로 이전했습니다. 단순히 서버 실행 환경을 클라우드로 옮기는 것에 그치지 않고, 제한된 서버 자원의 활용, 데이터 계층의 분리, 외부 API 노출 범위, 실시간 게임 트래픽의 경로와 실제 배포 절차까지 함께 고려했습니다.

#### Linux 기반 서버 환경

현재 Game Server는 **Ubuntu 24.04 LTS** 환경에서 운영합니다.

이전 프로젝트에서는 제한된 메모리를 가진 AWS EC2 Windows Instance를 사용하면서, 운영체제가 사용하는 Resource가 실제 서버 프로세스에 활용할 수 있는 메모리를 크게 제한하는 문제를 경험했습니다.

이번 프로젝트에서는 제한된 Cloud Resource를 Game Server에 보다 집중적으로 사용하기 위해 Linux 기반 환경을 선택하고, C++ 서버 역시 이에 맞춰 `io_uring` 기반으로 구성했습니다.

이는 Windows와 Linux의 일반적인 성능 우위를 전제로 한 선택이 아니라, **프로젝트의 배포 환경과 Resource 제약을 고려한 결정**입니다.

#### Compute / Database 분리

Game Server와 관계형 데이터베이스를 하나의 Compute Instance에 함께 배치하지 않고, **애플리케이션 계층과 데이터 계층을 별도의 서비스로 분리**했습니다.

Game Server는 Oracle Compute Instance에서 실행하고, 영속 데이터는 MySQL HeatWave에서 관리합니다.

Database는 외부 인터넷에 직접 노출하지 않고 Oracle Cloud 내부 네트워크를 통해 Compute Instance에서만 접근할 수 있도록 제한했습니다. 이를 통해 데이터 저장소의 접근 범위를 줄이는 동시에, **Server Process와 Database의 생명주기를 독립적으로 관리**할 수 있도록 구성했습니다.

#### Public API Exposure

계정 생성 및 인증, Matchmaking 요청, Item 검증, 게임 접속 준비와 Session Key 교환 등 외부 Client의 최초 진입점이 되는 HTTP API는 **Cloudflare Reverse Proxy를 통해 공개**합니다.

```text
Client
   │
   │ HTTPS
   ▼
Cloudflare
   │
   │ HTTP
   ▼
HTTP API Server
```

Client는 Cloudflare와 HTTPS로 통신하며, Origin HTTP API Server의 ingress는 **Cloudflare의 공개 IP Range에서 들어오는 요청만 허용**합니다. 이를 통해 Origin Server의 공개 IP를 이용해 Cloudflare를 우회하여 HTTP API에 직접 접근하는 경로를 제한했습니다.

반면 실시간 게임 통신은 HTTP API 경로와 분리했습니다.

```text
Client
   │
   │ Matchmaking / Authentication
   ▼
Cloudflare → HTTP API Server
   │
   │ Dedicated Server Allocation
   ▼
Client
   │
   │ Custom RUDP
   ▼
Dedicated Game Server
```

Matchmaking과 게임 접속 준비가 완료되면 Client는 할당받은 Dedicated Game Server와 **Custom RUDP를 통해 직접 통신**합니다.

따라서 인증·Matchmaking과 같은 Web API Traffic과 지속적으로 발생하는 실시간 Game Traffic이 서로 다른 경로를 사용하도록 구성했습니다.

#### Deployment Workflow

개발과 테스트는 Local Environment에서 진행하며, **Application Source Code와 Database Schema 변경 사항을 함께 버전 관리**합니다.

DB Schema 변경이 필요한 경우 Migration File을 생성하고, 반복적인 Migration 작성 작업을 줄이기 위해 Python Script를 이용해 작성을 보조합니다.

배포 시에는 Cloud Server에서 최신 Source를 반영한 뒤,

```text
Source Update
     │
     ▼
DB Migration
     │
     ▼
Server Build
     │
     ▼
Server Start
```

순서로 Migration과 Server Build를 적용합니다.

이를 통해 로컬 개발 환경에서 발생한 Application 및 Database 변경 사항을 실제 Public Cloud 환경까지 일관된 절차로 반영하도록 구성했습니다.

## Documentation

- [Networking](docs/networking.md) — Custom RUDP, ACK / Retransmission, RTT / RTO, `io_uring`
- [Matchmaking](docs/matchmaking.md) — Aggression 기반 Matchmaking, Redis Ticket State, Atomic Commit
- [Dedicated Server](docs/dedicated-server.md) — Process Lifecycle, PID ↔ IPC Binding, GameRoom / Player Lifecycle
- [Game State & Persistence](docs/gamestate-persistence.md) — Persistent / Ephemeral State, Item Lifecycle, Snapshot Consistency, Redis Match State
- [Authentication](docs/authentication.md) — Account Authentication, bcrypt Password Handling, Redis Session, Duplicate Login Policy
- [Claude Code Prompt Logs](docs/Claude_Code_프롬프트/) — 개발 과정에서 사용한 Claude Code 프롬프트와 AI 피드백을 정리한 기록

## Repository

- Client: [ExtractionClient](https://github.com/BoyeonK/ExtractionClient)
- Server: [ExtractionServer](https://github.com/BoyeonK/ExtractionServer)