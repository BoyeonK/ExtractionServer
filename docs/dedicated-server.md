# Dedicated Server

이 문서는 ExtractionServer의 **Dedicated Game Server Process 구조**, Main Server와의 역할 분리, Process 생성 및 IPC 연결 과정, GameRoom / Player Lifecycle, Capacity 관리와 DB Proxy 흐름을 설명합니다.

Matchmaking이 완료되면 Main Server는 Match Group을 수용할 Dedicated Game Server를 선택합니다.

Dedicated Game Server는 실제 인게임 로직과 UDP 통신을 담당하며, 하나의 Dedicated Process가 여러 개의 `GameRoom`을 관리합니다.

```text
Main Server
     │
     ├── Dedicated Process A
     │       ├── GameRoom 1
     │       ├── GameRoom 2
     │       └── GameRoom 3
     │
     ├── Dedicated Process B
     │       ├── GameRoom 4
     │       └── GameRoom 5
     │
     └── Dedicated Process C
             └── GameRoom 6
```

---

## 1. Dedicated Process Architecture

### Dedicated Game Server의 역할

ExtractionServer는 HTTP API Server, Main Server, Dedicated Game Server의 책임을 분리합니다.

```text
HTTP API Server
    │
    ├── Authentication
    ├── Matchmaking Request
    ├── Inventory Validation
    └── Client-facing HTTP API

Main Server
    │
    ├── MatchMaker
    ├── Dedicated Process Management
    ├── Redis / MySQL Access
    ├── DB Proxy
    └── Internal Coordination

Dedicated Game Server
    │
    ├── UDP / Custom RUDP
    ├── Player Session
    ├── GameRoom
    ├── In-game State
    └── Gameplay Logic
```

Dedicated Game Server는 Match가 시작된 이후의 **실시간 게임 세션 처리**에 집중합니다.

인증, Match Ticket 관리, Lobby Inventory와 같은 기능은 Dedicated Process가 직접 처리하지 않고 HTTP API Server와 Main Server가 담당합니다.

이를 통해 외부 API, Persistent State 관리와 실시간 인게임 처리를 서로 다른 책임 영역으로 분리했습니다.


### 왜 Dedicated Process를 사용했는가?

모든 GameRoom을 Main Server Process 하나에서 직접 처리할 수도 있지만, 현재 프로젝트에서는 인게임 로직을 별도의 Dedicated Process로 분리했습니다.

```text
Main Server
    │
    ├── Matchmaking
    ├── Process Coordination
    └── Persistent State Access

Dedicated Process
    │
    ├── Real-time Network
    ├── Player State
    └── GameRoom Logic
```

### 책임 분리

Main Server는 자식 프로세스 관리 및 매치메이킹에 집중하고, Dedicated Server는 실제 게임 진행에 집중합니다.

이를 통해 매치메이킹, 프로세스 관리, DB 접근과 실시간 GameRoom 로직이 하나의 프로세스에 모두 포함되지 않도록 했습니다.

### 에러 범위 제한

하나의 Dedicated Process는 여러 GameRoom을 관리합니다.

따라서 특정 Dedicated Process에 문제가 발생할 경우 서버 전체가 아니라 해당 프로세스가 담당하는 GameRoom들이 영향을 받는 구조입니다.

이것은 특정 Dedicated Process에서 발생한 장애가 메인 프로세스의 매치메이킹이나 다른 Dedicated Process의 GameRoom까지 직접 확산되는 범위를 줄이기 위함입니다.

다만 Room마다 별도의 Process를 생성하는 구조는 아니기 때문에 하나의 Dedicated Process가 비정상 종료되면 해당 Process에 속한 여러 Match가 함께 영향을 받을 수 있습니다.

즉 현재 구조는 **Process 단위의 책임 분리와 여러 GameRoom의 공동 관리 사이의 절충 구조**입니다.


### Dedicated Process Lifecycle Overview

Main Server는 현재 실행 중인 Dedicated Process의 수용 가능 상태를 관리합니다.

새로운 Match Group이 만들어지면 기존 Dedicated Process 중 해당 Match를 수용할 수 있는 Process가 있는지 확인합니다.

```text
Match Group
    │
    ▼
Main Server
    │
    ▼
Available Capacity?
    │
 ┌──┴───────────┐
YES            NO
 │              │
 ▼              ▼
Allocate     New Dedicated
Match        Process Spawn
Group           │
                ▼
            Initialization
                │
                ▼
            Allocate Match Group
```

수용 가능한 Dedicated Process가 없다면 Main Server가 새로운 Dedicated Process를 생성합니다.

Dedicated Process가 초기화를 완료하고 Main Server와 IPC 연결을 확립하면 실제 Match Group을 할당할 수 있는 상태가 됩니다.

---

## 2. PID ↔ IPC Socket Binding

Dedicated Process를 생성하는 시점과 해당 Process의 IPC Socket 연결이 완료되는 시점은 서로 다릅니다.

Main Server가 `fork()`를 호출하면 자식 Process의 `pid`는 즉시 알 수 있지만, 해당 Process와 통신할 IPC Socket은 아직 생성되지 않았기 때문에 연결에 사용할 `fd`는 존재하지 않습니다.

반대로 Dedicated Process가 Unix Domain Socket을 통해 Main Server에 연결하고 `accept()`가 완료되면 새로운 Socket `fd`를 얻을 수 있지만, `accept()` 결과만으로는 이 연결이 어떤 Dedicated Process에서 생성된 것인지 알 수 없습니다.

예를 들어 두 개의 Dedicated Process가 거의 동시에 생성된 경우 Main Server는 다음 두 집합을 각각 가지게 됩니다.

```text
Spawn 결과

pid A
pid B


IPC accept 결과

fd X
fd Y
```

이 시점에는 두 프로세스와 두 IPC 연결 간의 관계를 바로 판단할 수 없습니다.

```text
pid A ──?── fd X
      └─?── fd Y

pid B ──?── fd X
      └─?── fd Y
```

따라서 Dedicated Process가 초기화를 완료한 뒤 최초 IPC Packet에 자신의 `pid`를 직접 포함하여 전송하도록 했습니다.

Main Server는 이 정보를 이용해 Process 생성 시점에 확보한 `pid`와 IPC accept 시점에 확보한 `fd`를 결합합니다.

```text
fork()
  │
  ├── pid 확보
  │
  ▼
Dedicated 초기화
  │
  ▼
IPC connect / accept
  │
  ├── fd 확보
  │
  ▼
D2MInitComplete(pid)
  │
  ▼
pid ↔ fd Binding
  │
  ▼
M2DSession Ready
```

이 Binding이 완료된 시점부터 해당 Dedicated Process는 Main Server가 Match Group을 할당할 수 있는 활성 Session으로 사용됩니다.


### Step 1. Process Spawn — PID만 존재

Main Server의 `DediManager::SpawnSingleServer()`는 `fork()`를 이용하여 새로운 Dedicated Process를 생성합니다.

```cpp
pid_t pid = fork();

// Child
// execl("./LinuxServerTest", ..., "--dedicated", nullptr)

// Parent
M2DSession* pSession =
    new M2DSession(pid, IORing);

_dediSessions[pid] = pSession;
```

부모 Process는 `fork()`의 반환값을 통해 자식의 `pid`를 알 수 있으므로, 이를 기준으로 실제 Dedicated Session 객체인 `M2DSession`을 먼저 생성합니다.

이 시점에는 Dedicated Process가 아직 IPC 연결을 완료하지 않았기 때문에 `M2DSession`은 유효한 Socket `fd`를 가지고 있지 않습니다.

```text
_dediSessions

pid
 │
 ▼
M2DSession
    fd = -1
    state = Initializing
```

즉 Main Server는 Dedicated Process의 **Identity는 알고 있지만 IPC Connection은 아직 확보하지 못한 상태**입니다.

```text
Process Identity = Known
IPC Connection   = Not Ready
```


### Step 2. IPC Accept — FD만 존재

Dedicated Process가 초기화를 진행하면서 Main Server의 Unix Domain Socket에 연결하면 `accept()`가 완료됩니다.

`DediAcceptTask::callback()`에서는 accept 결과로 얻은 `fd`를 이용해 임시 Session을 생성합니다.

```cpp
_tempSessions[DediIPCsockFd] = pTempSession;

pTempSession->Recv();
```

하지만 이 시점에는 `fd`가 어느 자식 Process의 연결인지 아직 확인할 수 없습니다.

따라서 바로 실제 `M2DSession`에 연결하지 않고 `fd`를 Key로 하는 `M2DTempSession`에 임시 보관합니다.

```text
_tempSessions

fd
 │
 ▼
M2DTempSession
```

즉 Step 1과 반대로:

```text
Process Identity = Unknown
IPC Connection   = Known
```

상태입니다.


### Step 3. Dedicated Self-identification

Dedicated Process가 초기화를 완료하면 Main Server에 `D2MInitComplete` Packet을 전송합니다.

이 Packet에는 Dedicated Process 자신의 `getpid()` 결과가 포함됩니다.

```text
Dedicated Process

getpid()
   │
   ▼
D2MInitComplete
   │
   ▼
Unix Domain Socket
   │
   ▼
M2DTempSession
```

`M2DTempSession::OnReadComplete()`는 Packet에서 `pid`를 추출한 뒤 현재 Socket의 `fd`와 함께 `FinalizeConnection()`에 전달합니다.

```cpp
int childPid = pkt.pid();

if (pDediManager->FinalizeConnection(
        childPid,
        this->GetFd()))
{
    this->ReleaseFd();
}

delete this;
```

이 시점에 Main Server는 처음으로:

```text
pid = 어느 Dedicated Process인가
fd  = 어느 IPC Connection인가
```

를 동시에 알 수 있게 됩니다.


### FinalizeConnection

실제 `pid ↔ fd` Binding은 `DediManager::FinalizeConnection()`에서 수행합니다.

```cpp
pReal->BindSocket(fd);

_sessionFd2pid[fd] = pid;

_tempSessions.erase(itTemp);
```

Process 생성 시점에 `pid`를 기준으로 만들어 두었던 `M2DSession`에 실제 IPC Socket `fd`를 연결합니다.

```text
Before

_dediSessions
pid ──► M2DSession
         fd = -1
         state = Initializing

_tempSessions
fd ──► M2DTempSession


After

_dediSessions
pid ──► M2DSession
         fd = accepted fd
         state = Ready

_sessionFd2pid
fd ──► pid
```

`BindSocket()`이 완료되면 실제 `M2DSession`은 `Ready` 상태가 되고 IPC Receive를 시작합니다.

이렇게 해서 서로 다른 시점에 확보한:

```text
Process Spawn → pid
IPC Accept    → fd
```

를 Dedicated Process가 직접 전달한 `pid`를 기준으로 하나의 Session으로 결합합니다.


### Socket Ownership Transfer

Binding 과정에서는 Socket `fd`의 소유권도 임시 Session에서 실제 Session으로 이전해야 합니다.

```cpp
if (pDediManager->FinalizeConnection(
        childPid,
        this->GetFd()))
{
    this->ReleaseFd();
}

delete this;
```

`FinalizeConnection()`이 성공하면 실제 `M2DSession`이 해당 `fd`를 사용하게 됩니다.

그 뒤 `ReleaseFd()`를 호출하여 `M2DTempSession` 내부의 `_fd`를 `-1`로 변경합니다.

```text
M2DTempSession
      │
      │ owns fd
      ▼
FinalizeConnection
      │
      ▼
M2DSession
      │
      │ takes fd ownership
      ▼
M2DTempSession::ReleaseFd()
      │
      ▼
delete M2DTempSession
```

이 처리가 없으면 임시 Session의 소멸 과정에서 이미 실제 `M2DSession`에 넘긴 Socket을 닫을 수 있습니다.

따라서 `BindSocket()`과 `ReleaseFd()`는 **IPC Socket의 실제 소유권 이전을 완성하는 과정**입니다.


### Session Indexes

Binding 이전에는 _tempSessions가 임시 IPC Connection을 관리합니다.

Binding이 완료된 이후 실제 Dedicated Session은 _dediSessions와 _sessionFd2pid 두 Map을 이용해 조회합니다.

| Container | Mapping | Role |
| --- | --- | --- |
| `_dediSessions` | `pid → M2DSession*` | Main Server가 관리하는 실제 Dedicated Session |
| `_sessionFd2pid` | `fd → pid` | IPC Socket에서 Dedicated Process 역추적 |

```text

fd
 │
 ▼
_sessionFd2pid
 │
 ▼
pid
 │
 ▼
_dediSessions
 │
 ▼
M2DSession
```

따라서 `pid`를 알고 있다면 `_dediSessions`에서 직접 Session을 찾을 수 있고, `fd`만 알고 있다면 `fd → pid → M2DSession` 순서로 역추적할 수 있습니다.

`fd`를 기준으로 Session을 찾아야 할 경우에는 항상:

```text
fd → pid → M2DSession
```

순서의 두 단계 Lookup을 사용합니다.


### Ready 이후 Match Group 할당

`pid ↔ fd` Binding이 완료되기 전의 `M2DSession`은 `Initializing` 상태이므로 실제 Match Group을 처리할 수 없습니다.

```text
Spawn
  │
  ▼
M2DSession
Initializing
  │
  ▼
IPC Connect
  │
  ▼
D2MInitComplete(pid)
  │
  ▼
pid ↔ fd Binding
  │
  ▼
M2DSession
Ready
  │
  ▼
Match Group 할당 가능
```

새로운 Dedicated Process가 필요한 동안 Main Server에 Match Group이 추가로 생성될 수 있으므로, 해당 Group들은 Dedicated Process가 Ready 상태가 될 때까지 대기합니다.

초기화와 IPC Binding이 완료되면 해당 Dedicated Session은 Match Group 할당 대상으로 사용할 수 있습니다.

Binding 완료 이후 실행되는 후속 처리에서, 이 Dedicated Session의 초기화를 기다리고 있던 Match Group의 할당을 계속 진행합니다.

---

## 3. Main Server ↔ Dedicated Server IPC

Main Server와 Dedicated Game Server는 **Unix Domain Socket 기반 IPC**를 통해 통신합니다.

```text
Main Server
     │
     │ Unix Domain Socket
     ▼
Dedicated Game Server
```

외부 Client와의 UDP 통신과 달리 Main ↔ Dedicated 통신은 동일 Host 내부의 Process 간 통신입니다.

IPC는 Dedicated Process의 초기 연결뿐 아니라 이후 Match와 Game State 관련 정보 전달에도 사용됩니다.

```text
Main → Dedicated

- Match Group
- Player Information
- GameRoom 생성 요청
- 게임 시작에 필요한 초기 상태

Dedicated → Main

- Dedicated 초기화 완료
- Capacity 변화
- Player 결과 상태
- Persistent State 변경 요청을 포함한 Redis 혹은 MySQL 작업 요청 (DB Proxy)
```

Dedicated Process는 Redis나 MySQL에 직접 접근하지 않고 Persistent State 변경이 필요한 경우 IPC를 통해 Main Server에 요청합니다.

---

## 4. Game Session Lifecycle

### GameRoom

`GameRoom`은 하나의 Match를 표현하는 인게임 상태 단위입니다.

```text
Dedicated Process
      │
      ├── GameRoom A
      │      ├── Player 1
      │      ├── Player 2
      │      └── Player 3
      │
      └── GameRoom B
             ├── Player 4
             └── Player 5
```

하나의 Dedicated Process는 여러 GameRoom을 동시에 관리할 수 있습니다.

각 GameRoom은 서로 다른 Match 상태를 가지며, Room 내부 Player와 인게임 상태를 관리합니다.

주요 책임은 다음과 같습니다.

```text
GameRoom

- Player 관리
- In-game State 관리
- Item 상태 관리
- Player State Broadcast
- Match 제한시간 관리
- Player 이탈 처리
- Room 종료 판단
```


### Client Connection

Match Group에 대한 Dedicated Process 할당이 완료되면 해당 그룹 플레이어들을 나타내는 Redis 영역의 Match Ticket은 `SUCCESS` 상태가 됩니다.

클라이언트는 HTTP API를 통해 Dedicated Server의 연결 정보와 인증에 필요한 정보를 전달받은 뒤 해당 Dedicated Process에 직접 접속합니다.

```text
Matchmaking SUCCESS
       │
       ▼
Client /connect
       │
       ▼
Dedicated IP / Port
securityKey
       │
       ▼
Client
       │
       │ UDP / Custom RUDP
       ▼
Dedicated Game Server
       │
       ▼
PlayerSession
       │
       ▼
GameRoom
```

이 시점부터 실시간 Game Packet은 HTTP API Server를 거치지 않고 Client와 Dedicated Server 사이에서 직접 교환됩니다.


### PlayerSession Lifecycle

Dedicated Server 내부에서 Player는 `PlayerSession`과 GameRoom 내부의 Player 상태를 통해 관리됩니다.

개념적인 Lifecycle은 다음과 같습니다.

```text
Assigned
   │
   ▼
Connected
   │
   ▼
GameRoom Joined
   │
   ▼
Playing
   │
   ├── Extraction
   ├── Death
   ├── Disconnect
   └── Match Timeout
          │
          ▼
       Leave Process
          │
          ▼
       FINALIZED
```

Player의 종료는 단순한 Socket Disconnect만으로 결정되지 않습니다.

게임 규칙상 여러 종류의 종료 경로가 존재합니다.

### Successful Extraction

Player가 Extraction에 성공하면, Match에서 확보한 Item 결과를 Player ID와 함께 메인 프로세스에 전달하여 해당 플레이어의 DB에 확보한 Item을 적용합니다.

### Death

Player가 사망하면 해당 Match에서 소유하고 있던 Item은 게임 규칙에 따라 손실됩니다.

### Disconnect

비정상적인 연결 종료 역시 Match 이탈로 처리합니다.

현재 프로젝트에서는 비정상 Disconnect 시 Death와 동일하게 Match 내부 Item을 잃는 정책을 사용합니다.

### Match Timeout

GameRoom은 최대 **10분** 동안 유지됩니다.

제한시간이 종료될 때까지 Room에 남아 있는 Player는 Match 종료 정책에 따라 처리됩니다.

현재 구현에서는 Death와 동일한 결과로 처리합니다.


### PlayerSession Cleanup

Player가 이미 GameRoom을 떠났음에도 Session 상태가 남아 있으면 단순한 메모리 문제 이상의 영향을 줄 수 있습니다.

```text
Stale PlayerSession
      │
      ├── ACK State 유지
      ├── Pending Reliable 유지 가능
      ├── Retransmission 대상 유지 가능
      ├── GameRoom Player Count에 영향
      └── Room 종료 판단 방해
```

Player가 실제로 게임에서 이탈한 이후에도 네트워크 상태가 유지되면 불필요한 ACK / Retransmission 처리가 계속될 수 있습니다.

또한 GameRoom이 아직 Player를 보유하고 있다고 판단하게 되어 Room 종료와 Capacity 반환도 지연될 수 있습니다.

따라서 Player의 게임 참여 상태와 Network Session 상태를 일관되게 종료하는 Cleanup 과정이 중요합니다.


### GameRoom Lifecycle

GameRoom의 전체 Lifecycle은 다음과 같습니다.

```text
Match Group
    │
    ▼
GameRoom Create
    │
    ▼
Player Join
    │
    ▼
Game Start
    │
    ▼
Gameplay
    │
    ├── Extraction
    ├── Death
    ├── Disconnect
    └── Timeout
    │
    ▼
Player Leave
    │
    ▼
Remaining Player?
    │
 ┌──┴───┐
YES     NO
 │       │
 ▼       ▼
계속     GameRoom Cleanup
진행          │
              ▼
        Capacity Return
```

GameRoom은 Player가 남아 있는 동안 유지됩니다.

모든 Player의 이탈 처리가 완료되어 Room 내부에 더 이상 Player가 존재하지 않으면 GameRoom 자원을 회수합니다.


### Capacity Management

Dedicated Process는 무제한으로 GameRoom과 Player를 수용하지 않습니다.

Main Server는 각 Dedicated Process가 새로운 Match를 추가로 받을 수 있는지 확인한 뒤 Match Group을 할당합니다.

```text
Dedicated Process

Current Capacity
      │
      ├── Available
      │      ↓
      │   New GameRoom
      │
      └── Full
             ↓
          Main Server
             ↓
      다른 Dedicated 탐색
             │
             └── 없으면 Spawn
```

GameRoom이 종료되고 자원이 회수되면 Dedicated Server는 새롭게 사용할 수 있는 Capacity가 생겼음을 Main Server에 IPC로 전달합니다.

```text
GameRoom Empty
     │
     ▼
Resource Reclamation
     │
     ▼
Dedicated Capacity 증가
     │
     ▼
IPC
     │
     ▼
Main Server
```

Main Server는 이후 Match Group을 기존 Dedicated Process에 다시 할당할 수 있습니다.

---

## 5. Persistent State and DB Proxy

### DB Proxy

Dedicated Game Server는 Redis와 MySQL에 직접 접근하지 않습니다.

Persistent State 변경이 필요한 경우 Main Server를 **DB Proxy**로 사용합니다.

```text
Dedicated
    │
    │ IPC
    ▼
Main Server
    │
    ├── Redis
    └── MySQL
```

Dedicated Process는 실시간 Game Logic과 Network 처리에 집중하고, Persistent Storage 접근 책임은 Main Server에 둡니다.

이 구조는 DB 접근 경로를 중앙화하는 대신 Dedicated에서 발생한 Persistent State 변경이 Main Server를 한 단계 더 거쳐야 하는 Trade-off를 가집니다.


### Lobby / In-game Item State

Extraction Shooter에서는 Lobby Inventory와 Match 내부 Item State를 분리합니다.

```text
Lobby
MySQL Inventory
     │
     │ Match Entry
     ▼
Dedicated Memory
In-game Item State
     │
     ├── Extraction
     │      ↓
     │   Persistent Inventory
     │
     ├── Death
     │      ↓
     │   Lost
     │
     └── Disconnect
            ↓
          Lost
```

### Match Entry

Player의 Match 진입이 확정되면 가지고 들어가는 Item은 Lobby의 Persistent Inventory에서 제거되고 Match 내부 상태로 이동합니다.

이 시점부터 해당 Item의 상태는 Dedicated Server Memory에서 관리합니다.

### Successful Extraction

Player가 Extraction에 성공하면 Dedicated Server는 Player의 Item 결과를 Main Server에 전달합니다.

Main Server는 DB Proxy 역할을 통해 결과를 Persistent Inventory에 반영합니다.

### Death / Abnormal Disconnect

Player가 사망하거나 비정상적으로 연결을 종료하면 해당 Match에서 소유한 Item은 게임 규칙에 따라 손실됩니다.

Lobby Inventory에 다시 복원하지 않습니다.

---

## 6. Design Constraints & Trade-offs

### Process-level Isolation

하나의 Dedicated Process가 여러 GameRoom을 관리합니다.

Room마다 Process를 하나씩 생성하는 방식보다 Process 수를 줄일 수 있지만, Dedicated Process 하나에 문제가 발생하면 해당 Process가 담당하는 여러 Room이 함께 영향을 받을 수 있습니다.

### In-memory Game State

Match가 진행되는 동안 Player와 Item의 주요 Game State는 Dedicated Process Memory에 존재합니다.

따라서 Dedicated Process가 비정상 종료된 경우 Match State 전체를 그대로 복구하는 구조는 현재 구현 범위에 포함하지 않습니다.

### Dedicated Process Reclamation

현재 Dedicated Process는 모든 GameRoom이 종료되더라도 바로 종료하지 않고 이후 Match에서 다시 사용할 수 있도록 유지합니다.

비정상 종료된 Dedicated Process에 대해 `_dediSessions`와 `_sessionFd2pid`를 자동으로 제거하고 Process를 회수하는 Lifecycle 관리 역시 현재 구현 범위에서는 제한적입니다.
