# Networking

이 문서는 ExtractionServer에서 사용하는 **Custom RUDP Transport**와 Linux `io_uring` 기반 비동기 I/O 구조를 설명합니다.

게임 접속 전의 인증, Matchmaking, Item 검증 및 접속 준비는 HTTP API를 통해 처리합니다.

Matchmaking이 완료되면 클라이언트는 할당받은 Dedicated Game Server와 직접 UDP 통신을 시작합니다.

```text
Unity Client
     │
     │ HTTPS
     ▼
HTTP API Server
     │
     ├── Authentication
     ├── Matchmaking
     ├── Item Validation
     ├── Connection Preparation
     └── securityKey Exchange
     │
     ▼
Dedicated Server Assigned
     │
     ▼
Unity Client
     │
     │ UDP / Custom RUDP
     ▼
Dedicated Game Server
```

실시간 통신에서는 모든 Packet에 동일한 전달 보장을 적용하지 않고, 데이터의 성격에 따라 **Reliable / Unreliable Channel**을 분리합니다.

---

## 1. Custom RUDP를 구현한 이유

실시간 게임에서는 모든 데이터가 동일한 성질을 가지지 않습니다.

플레이어 위치나 방향처럼 지속적으로 새로운 값이 생성되는 상태 정보는 일부 Packet이 유실되더라도 이후 도착한 최신 상태로 보완할 수 있습니다.

반대로 게임 진행에 중요한 일부 Packet은 유실될 경우 Client와 Server의 상태가 어긋날 수 있으므로 재전송이 필요합니다.

TCP처럼 모든 데이터에 동일한 순서 보장과 재전송을 적용하면 이전 Packet의 유실이 이후의 최신 상태 전달에도 영향을 줄 수 있다고 판단했습니다.

따라서 **GameNetworkingSockets의 전송 모델에서 영감을 받아 Reliable / Unreliable Channel을 분리한 자체 RUDP Transport**를 구현했습니다.

```text
Application Packet
       │
       ├── Reliable Channel
       │      ├── Sequence
       │      ├── Selective ACK
       │      ├── RTT Estimation
       │      └── Retransmission
       │
       └── Unreliable Channel
              ├── Packet Loss Allowed
              └── Prefer Latest State
```

목표는 UDP 위에 TCP의 모든 기능을 다시 구현하는 것이 아니라, **현재 게임 세션에서 필요한 범위의 신뢰성을 선택적으로 제공하는 것**입니다.

---

## 2. RUDP Packet Header

모든 실시간 Game Packet은 공통 `UDPHeader`를 사용합니다.

```cpp
#pragma pack(push, 1)

struct UDPHeader {
    uint64_t signature;

    uint16_t packetId;
    uint16_t sessionId;

    uint32_t rSeqNum;
    uint16_t uSeqNum;
    uint8_t  flags;

    uint32_t ackRSeqNum;
    uint32_t ackBitfield;

    uint32_t timestamp;
    uint32_t timestampEcho;
};

#pragma pack(pop)
```

`#pragma pack(1)`을 사용하므로 Header 크기는 **35 Byte**입니다.

```text
UDPHeader (35B)

┌─────────────────────────────┐
│ signature          8B       │
├─────────────────────────────┤
│ packetId           2B       │
│ sessionId          2B       │
│ rSeqNum            4B       │
│ uSeqNum            2B       │
│ flags              1B       │
├─────────────────────────────┤
│ ackRSeqNum         4B       │
│ ackBitfield        4B       │
├─────────────────────────────┤
│ timestamp          4B       │
│ timestampEcho      4B       │
├─────────────────────────────┤
│ Payload                     │
└─────────────────────────────┘
```

| Field | Role |
| --- | --- |
| `signature` | Session별 Packet Signature |
| `packetId` | Application Packet 종류 |
| `sessionId` | Player Session 식별 |
| `rSeqNum` | Reliable Channel Sequence |
| `uSeqNum` | Unreliable Channel Sequence |
| `flags` | Packet 처리 Flag |
| `ackRSeqNum` | 가장 높은 Reliable 수신 Sequence |
| `ackBitfield` | 이전 Reliable Packet의 수신 상태 |
| `timestamp` | 현재 Packet의 송신 Timestamp |
| `timestampEcho` | 상대방이 이전에 보낸 Timestamp 반사 |

ACK와 RTT 측정에 필요한 상태를 공통 Header에 포함하여 실시간 Packet과 함께 지속적으로 교환합니다.

---

## 3. Reliable / Unreliable Channel

### Reliable Channel

Reliable Channel은 전달 신뢰성이 필요한 Packet에 사용합니다.

각 Packet에는 `uint32_t` 기반의 `rSeqNum`을 부여하고, ACK가 도착할 때까지 송신 측에서 원본 Packet을 보관합니다.

```text
Reliable Packet
      │
      ▼
rSeqNum 할당
      │
      ▼
Pending Reliable 등록
      │
      ▼
UDP Send
      │
      ▼
ACK 대기
      │
      ├── ACK 수신 → Pending 제거
      │
      └── RTO 초과 → Retransmit
```

제한된 Receive Window와 재전송을 이용하여 **현재 게임 세션에서 요구하는 범위의 전달 신뢰성**을 제공하는 Channel로서 동작합니다.

### Unreliable Channel

Unreliable Channel은 Packet 유실보다 오래된 상태의 재전송으로 인한 지연이 더 불리한 데이터에 사용합니다.

대표적인 예는 플레이어 위치와 이동 상태입니다.

```text
Position t=1 ──►

Position t=2 ────── X

Position t=3 ─────────────►
                           ↑
                       최신 상태 사용
```

`t=2`가 유실되더라도 이후 `t=3`가 도착했다면 정상적으로 동작합니다.

Reliable / Unreliable Sequence는 각각 별도로 관리합니다.

```cpp
uint32_t rSeqNum;
uint16_t uSeqNum;
```

---

## 4. Reliable Receive Window

수신 측은 다음 상태를 이용해 최근 Reliable Packet의 수신 여부를 관리합니다.

```cpp
uint32_t _rRecvHighestSeq;
uint32_t _rRecvBitfield;
```

`_rRecvHighestSeq`는 현재까지 수신한 가장 높은 Reliable Sequence입니다.

`_rRecvBitfield`는 그보다 이전에 수신한 Reliable Packet 상태를 기록합니다.

개념적으로 다음과 같은 구조입니다.

```text
Highest = 100

100          Highest
99           bit 0
98           bit 1
97           bit 2
...
```

이를 통해 최신 Sequence뿐 아니라 최근에 Out-of-order로 도착한 Reliable Packet의 수신 여부도 함께 유지할 수 있습니다.

### Out-of-order 수신

예를 들어 Packet이 다음 순서로 도착할 수 있습니다.

```text
100
102
101
```

`102`를 수신한 시점에는:

```text
Highest = 102

101 → not received
100 → received
```

상태를 Bitfield에 기록합니다.

이후 `101`이 도착하면 해당 위치의 Bit를 추가로 설정합니다.

즉 Reliable Packet이 반드시 Sequence 순서대로 도착한다고 가정하지 않습니다.

---

## 5. Duplicate Detection

ACK가 유실되면 송신자는 이미 전달된 Reliable Packet을 다시 보낼 수 있습니다.

```text
Sender                         Receiver

Packet N
   ├────────────────────────────►
   │                            Process N
   │
   │         ACK N
   │◄────────────── X
   │
   ▼
Retransmit N
   ├────────────────────────────►
                            Duplicate N
```

수신 측은 Sequence와 Receive Window를 이용하여 최근에 이미 처리한 Packet인지 판정합니다.

```cpp
if (rSeqNum == _rRecvHighestSeq)
    return false;

...

if (_rRecvBitfield & bit)
    return false;
```

따라서 추적 중인 Receive Window 안에서는 동일 Sequence의 재수신을 Transport Layer에서 Duplicate로 판정할 수 있습니다.

Application Packet Handler 역시 네트워크 재전송이나 중복 입력에 의해 동일한 Side Effect가 의도하지 않게 반복되지 않도록, 가능한 경우 멱등하게 동작하거나 중복 호출에 안전하도록 구성하는 것을 기본 방향으로 사용합니다.

---

## 6. Selective ACK

수신 측의 Reliable 상태는 다음 두 값으로 상대방에게 전달됩니다.

```text
ackRSeqNum
+
ackBitfield
```

`ackRSeqNum`은 가장 높은 Reliable Sequence를 나타내며, `ackBitfield`는 그 이전 Packet의 수신 여부를 나타냅니다.

송신 측에서는 ACK를 받으면 해당 Sequence의 Pending Packet을 제거합니다.

```cpp
void PlayerSession::ProcessIncomingAck(
    uint32_t ackSeqNum,
    uint32_t ackBitfield)
{
    auto releaseBySeq = [&](uint32_t seq) {
        auto it = _pendingReliable.find(seq);

        if (it != _pendingReliable.end()) {
            it->second->ReleaseThis();
            _pendingReliable.erase(it);
        }
    };

    releaseBySeq(ackSeqNum);

    if (_pendingReliable.empty() || ackBitfield == 0)
        return;

    for (int i = 0; i < 32 && ackBitfield != 0; i++) {
        if (ackBitfield & 1u) {
            releaseBySeq(
                ackSeqNum - static_cast<uint32_t>(i + 1));
        }

        ackBitfield >>= 1;
    }
}
```

이를 통해 하나의 ACK 상태로 최근 여러 Reliable Packet의 전달 여부를 확인합니다.

---

## 7. Pending Reliable Packet

ACK를 받지 못한 Reliable Packet은 `PlayerSession` 내부에서 Sequence별로 관리합니다.

```cpp
std::unordered_map<uint32_t, PendingPacket*> _pendingReliable;
```

각 Pending Packet에는 재전송에 필요한 상태를 저장합니다.

```cpp
class PendingPacket {
public:
    PendingPacket() { };
    PendingPacket(int32_t size) : allocSize(size) {};
    virtual ~PendingPacket() { };

    virtual void ReleaseThis() = 0;
    virtual unsigned char* GetData() = 0;

    uint32_t             seqNum;
    uint32_t             allocSize;
    sockaddr_in          destAddr;
    uint32_t             sentAtMs;
    bool                 isPool = false;
};
```

PendingPacket의 실제 구현체는 Buffer 크기별로 분리되어 있으며, Object Pool로서 관리합니다.

```cpp
class PendingPacket256 : public PendingPacket {
public:
    PendingPacket256(uint32_t size) : PendingPacket(size) {
        isPool = true;
    }

    void ReleaseThis() override;
    unsigned char* GetData() override { return data; }

    unsigned char data[256];
};
```


실제 Packet Data는 `data[]` 영역에 함께 보관합니다.

```text
Pending Reliable Packet

- Sequence
- Packet Data
- Destination
- Last Sent Time
```

ACK가 도착하면 Pending 상태를 해제하며, ACK가 도착하지 않은 상태에서 Retransmission Timeout을 초과하면 재전송 후보가 됩니다.

---

## 8. RTT Estimation

Retransmission Timeout은 단순한 고정값이 아니라 현재 Session에서 측정한 RTT를 반영합니다.

Header에는 다음 Timestamp가 포함됩니다.

```cpp
uint32_t timestamp;
uint32_t timestampEcho;
```

송신자가 보낸 `timestamp`를 상대방이 이후 Packet의 `timestampEcho`를 통해 그대로 헤더에 담아 돌려보냅니다.

```text
Server                            Client

timestamp = 1000
    ├──────────────────────────────►
    │
    │◄──────────────────────────────┤
                         timestampEcho = 1000

nowMs = 1080

RTT Sample
= 1080 - 1000
= 80ms
```

자신이 생성한 Timestamp를 다시 돌려받아 계산하므로 양측 Clock Synchronization은 필요하지 않습니다.

### RTT Smoothing

한 번의 RTT Sample을 그대로 사용하지 않고 EWMA 방식으로 평활화합니다.

```cpp
uint32_t rtt = nowMs - echoTs;

_rttMs = (_rttMs * 7u + rtt) / 8u;

if (_rttMs < 20u)
    _rttMs = 20u;
```

식으로 표현하면 다음과 같습니다.

```text
Smoothed RTT
= Previous RTT × 7/8
+ Latest RTT Sample × 1/8
```

하나의 RTT Sample 변화가 재전송 정책에 즉시 크게 반영되는 것을 줄이기 위한 구조입니다.

### Stale Timestamp Filtering

Reliable Packet이 재전송될 때 기존 Timestamp가 그대로 포함될 수 있습니다.

이전 Packet에서 생성된 오래된 Echo 값을 새로운 RTT Sample로 사용하면 RTT가 불필요하게 크게 측정될 수 있습니다.

따라서 이전에 처리한 Echo보다 오래된 Timestamp는 RTT 갱신에서 제외합니다.

```cpp
if (_lastEchoTs != 0 &&
    static_cast<int32_t>(echoTs - _lastEchoTs) <= 0)
{
    return;
}
```

---

## 9. Retransmission Timeout

재전송 Timeout은 평활화된 RTT를 기반으로 계산합니다.

```cpp
uint32_t timeout =
    std::min(
        std::max(_rttMs * 3u / 2u, 50u),
        1000u);
```

즉 현재 정책은 다음과 같습니다. (50ms의 하한을 넣은 이유는, 로컬 테스트 환경과 같은 응답속도가 빠른 환경의 경우, 의도치 않게 재전송이 빈번히 일어나는 것을 사전에 차단하기 위함입니다.)

```text
RTO = clamp(Smoothed RTT × 1.5,
            50ms,
            1000ms)
```

예를 들어:

| Smoothed RTT | 계산값 | RTO |
| ---: | ---: | ---: |
| 20ms | 30ms | 50ms |
| 100ms | 150ms | 150ms |
| 200ms | 300ms | 300ms |
| 800ms | 1200ms | 1000ms |

각 Pending Packet의 마지막 송신 시각과 현재 시각을 비교해 재전송 후보를 선택합니다.

```cpp
std::vector<PendingPacket*>
PlayerSession::GetRetransmitCandidates(uint32_t nowMs)
{
    uint32_t timeout =
        std::min(
            std::max(_rttMs * 3u / 2u, 50u),
            1000u);

    std::vector<PendingPacket*> result;

    for (auto& [seq, pending] : _pendingReliable) {
        uint32_t elapsed =
            nowMs - pending->sentAtMs;

        if (elapsed >= timeout)
            result.push_back(pending);
    }

    return result;
}
```

현재 방식은 표준 RTO 알고리즘에서 영감을 얻었지만, 동일하게 작동하지 않습니다.

RTT Variance를 별도로 계산하지 않고, **EWMA 기반 RTT 추정치에 고정 배수를 적용하는 단순한 RTT 기반 RTO 정책**입니다.

---

## 10. Retransmission Scheduling

RTO를 초과한 순간 즉시 별도의 Timer가 실행되는 구조는 아닙니다.

Dedicated Server의 Main Loop에서 주기적으로 재전송 후보를 검사합니다.

```text
Dedicated Main Loop
        │
        ▼
CheckRetransmits(nowMs)
        │
        ▼
GetRetransmitCandidates()
        │
        ▼
RTO 초과 Packet
        │
        ▼
Retransmit
```

검사는 약 `50ms` 간격으로 수행하지만, Dedicated Process의 작업량이 한순간 확 늘어나는 것을 원하지 않으므로, 한 번에 모든 `PlayerSession`을 검사하지 않습니다.

Session 배열의 홀수 / 짝수 인덱스를 번갈아 처리합니다.

```text
0ms
Session 0, 2, 4, 6 ...

50ms
Session 1, 3, 5, 7 ...

100ms
Session 0, 2, 4, 6 ...

150ms
Session 1, 3, 5, 7 ...
```

따라서 개별 Session은 실질적으로 약 **100ms 간격**으로 재전송 후보 검사를 받습니다.

한 Tick에 모든 Session의 Retransmission Scan이 몰리지 않도록 작업을 두 Phase로 나눈 구조입니다.

후보 Packet이 발견되면 보관하고 있던 원본 Packet을 새로운 SendBuffer에 복사한 뒤 다시 전송합니다.

```text
PendingPacket
      │
      │ RTO exceeded
      ▼
Acquire SendBuffer
      │
      ▼
Copy Original Packet
      │
      ▼
UDP Send
      │
      ▼
sentAtMs = nowMs
```

따라서 RTO는 Packet이 재전송 대상이 될 수 있는 최소 경과시간이며, 실제 재전송은 **RTO를 초과한 이후 해당 Session의 다음 Scan 시점**에 수행됩니다.

---

## 11. Lightweight Packet Signature

실시간 Packet Payload 전체에 별도의 암호화를 적용하지 않는 대신, 게임 접속 준비 단계에서 HTTPS를 통해 공유한 `securityKey`를 이용하여 Packet Signature를 검증합니다.

`UDPHeader`의 첫 8 Byte인 `signature`를 사용합니다.

송신 시 Signature를 먼저 `0`으로 만든 뒤 Application-level Packet 전체를 대상으로 다음 값을 계산합니다.

```cpp
XXH64(packet, size, securityKey)
```

```text
Packet 생성
    │
    ▼
signature = 0
    │
    ▼
XXH64(packet, size, securityKey)
    │
    ▼
signature 기록
    │
    ▼
UDP Send
```

수신 측에서도 동일한 방식으로 다시 계산합니다.

```text
UDP Receive
    │
    ▼
Received Signature 저장
    │
    ▼
signature = 0
    │
    ▼
XXH64(...)
    │
    ▼
Received == Calculated ?
     │
 ┌───┴────┐
YES      NO
 │        │
 ▼        ▼
Process   Drop
```

이 방식은 잘못된 Packet이나 현재 Session과 일치하지 않는 Packet을 낮은 비용으로 식별하기 위한 경량 검증 방식입니다.

`xxHash64`는 암호학적으로 안전한 MAC은 아니므로 강한 Packet Authentication을 제공하는 구조로 간주하지 않습니다. 현재 프로젝트에서는 잘못된 패킷이나 세션과 일치하지 않는 Packet을 낮은 비용으로 식별하기 위한 경량 검증 수단으로 사용합니다.

---

## 12. Heartbeat and Connection State

클라이언트는 Dedicated Game Server와 연결된 동안 주기적으로 Heartbeat Packet을 전송합니다.

Application Packet이 장시간 발생하지 않는 상황에서도 연결 상태를 지속적으로 확인하기 위한 목적입니다.

```text
Client                     Dedicated

Heartbeat
   ├──────────────────────────►

Heartbeat
   ├──────────────────────────►

Heartbeat
   ├──────────────────────────►
```

연결 상태 관리는 단순히 Socket 연결 여부만의 문제가 아닙니다.

Player의 연결 종료가 제때 반영되지 않으면:

- 종료된 Session에 대한 네트워크 상태가 불필요하게 유지될 수 있고
- Player의 GameRoom 이탈을 확정하기 어려워지며
- GameRoom의 자원 회수도 지연될 수 있습니다.

따라서 Connection State는 Player / GameRoom Lifecycle과 함께 관리합니다.

---

## 13. Player State Synchronization

실시간 Player State는 Unreliable Channel을 사용하는 대표적인 데이터입니다.

각 클라이언트는 약 `0.1초` 주기로 자신의 위치와 상태 정보를 Dedicated Game Server에 전달합니다.

```text
Client A
   │
   │ Position / State
   │ every 0.1 sec
   ▼
Dedicated Game Server
```

Dedicated Game Server는 수신한 정보를 Player State에 반영합니다.

GameRoom 역시 약 `0.1초` 주기로 Room 내부 Player들의 상태를 Broadcast합니다.

```text
                 Dedicated
                     │
               Player States
                 Broadcast
                     │
          ┌──────────┴──────────┐
          ▼                     ▼
      Client A               Client B
                               │
                               ▼
                        Remote Player
                        - Position
                        - Movement
                        - Animation
```

다른 클라이언트는 수신한 상태를 기반으로 상대 플레이어의 위치, 이동 상태 및 Animation을 갱신합니다.

---

## 14. `io_uring` Based Asynchronous I/O

Linux에서 UDP 및 Unix Domain Socket IPC를 비동기적으로 처리하기 위해 `io_uring`을 사용합니다.

`io_uring`을 단순히 `epoll`보다 빠르다는 이유로 선택한 것은 아닙니다.

이전 프로젝트에서 Windows IOCP를 사용하며 경험했던:

```text
I/O 작업 생성
    ↓
작업 제출
    ↓
Kernel I/O
    ↓
완료 통지
    ↓
후처리
```

형태와 유사한 Completion 중심의 구조를 Linux에서도 구성할 수 있다는 점을 고려했습니다.

전체 처리 흐름은 다음과 같습니다.

```text
IOTask
  ↓
SQE
  ↓
Submission Queue
  ↓
Kernel I/O
  ↓
CQE
  ↓
IOTask
  ↓
Callback / Post Processing
```

### IOTask Abstraction

UDP와 IPC는 Socket 종류와 작업 내용은 다르지만 `io_uring` 관점에서는 공통적으로:

```text
Prepare
   ↓
Submit
   ↓
Complete
   ↓
Post Process
```

흐름을 가집니다.

이를 통합하기 위해 `IOTask`라는 공통 인터페이스를 사용합니다.

`IOTask` 객체는 IOCP를 다루면서 사용했던 `OVERLAPPED`구조체에서 영감을 받아 만들었습니다.


```text
IOTask
  │
  ├── UDP Recv Task
  ├── UDP Send Task
  │
  ├── IPC Accept Task
  ├── IPC Recv Task
  └── IPC Send Task
```

각 Task는 I/O 완료 이후 필요한 상태와 Callback을 보관합니다.

I/O를 제출할 때 Task를 SQE의 `user_data`와 연결합니다.

```text
IOTask
   │
   ▼
SQE.user_data
   │
   ▼
Submission Queue
```

커널이 I/O를 완료하면 Completion Queue에 CQE가 생성됩니다.

CQE의 `user_data`를 통해 원래 제출했던 `IOTask`를 다시 찾아 후처리를 실행합니다.

```text
Completion Queue
       │
       ▼
      CQE
       │
       │ user_data
       ▼
    IOTask
       │
       ▼
Callback / Post Processing
```

Completion Queue에 `IOTask` 자체가 들어가는 것이 아니라 **CQE를 통해 제출 당시 연결했던 Task를 다시 찾는 구조**입니다.

### IOTask Object Pool

Recv / Send I/O Task는 서버 실행 중 높은 빈도로 생성됩니다.

각 작업마다 Task 객체를 새롭게 할당하고 해제하지 않도록 Object Pool을 사용합니다.

```text
Object Pool
     │
     ▼
Acquire IOTask
     │
     ▼
Submit I/O
     │
     ▼
Completion
     │
     ▼
Callback
     │
     ▼
Return to Pool
```

목적은 높은 빈도의 I/O Task 생성 과정에서 반복되는 동적 메모리 Allocation / Deallocation을 줄이는 것입니다.

---

## 15. Design Constraints & Trade-offs

Custom RUDP는 범용 Transport Library를 목표로 하지 않고 이 프로젝트의 Game Session 특성에 맞춰 구현했습니다.

### Bounded Reliable Receive Window

Reliable 수신 상태는 가장 높은 Sequence와 32-bit Bitfield를 이용하여 최근 Packet만 추적합니다.

따라서 Receive Window에서 크게 뒤처진 Packet까지 무제한으로 수신 상태를 추적하거나 복구하는 구조는 아닙니다.

송신 측에서는 ACK를 받지 못한 Reliable Packet을 별도의 Pending 영역에 보관하고 RTO를 초과한 경우 재전송하지만, 수신 측의 전달 상태 추적 범위 자체는 제한되어 있습니다.

따라서 이 프로젝트에서 `Reliable`은 절대적인 `exactly-once` 또는 무제한 Delivery Guarantee를 의미하지 않고, **현재 Game Session과 Receive Window 범위에서 전달 신뢰성을 높이는 Channel**을 의미합니다.

### Sequence Lifetime Assumption

Reliable Sequence는 `uint32_t`를 사용하고 단순 정수 비교를 기반으로 관리합니다.

하나의 Game Session은 최대 약 **10분** 동안 유지되며, 해당 시간 안에 Reliable Sequence가 `uint32_t` 범위를 소진하지 않는 것을 전제로 설계했습니다.

따라서 하나의 Game Session 안에서는 Reliable Sequence의 uint32_t wrap-around가 발생하지 않는 것을 전제로 했습니다.

### Simplified RTO

현재 RTO는 EWMA 기반 RTT에 고정 배수 `1.5`를 적용합니다.

RTT Variance나 보다 복잡한 Congestion 상태까지 추적하는 범용 Transport 수준의 RTO 알고리즘은 구현하지 않았습니다.

---

## 16. Possible Improvements

현재 구현에서 추가적으로 개선할 수 있는 영역은 다음과 같습니다.

### Server-side Movement Validation

Client가 전달한 Position에 대해 이동 거리, 속도, Collision 등을 검증하면 비정상적인 이동을 Server 측에서 제한할 수 있습니다.

### Cryptographic Packet Authentication

현재 `xxHash64` Signature보다 높은 수준의 Packet Authenticity가 필요하다면 HMAC 등 암호학적 MAC을 사용할 수 있습니다.