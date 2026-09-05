# Matchmaking

이 문서는 ExtractionServer의 Matchmaking 설계와 Queue 탐색 방식, Redis를 이용한 Ticket 상태 관리 및 Dedicated Game Server 할당까지의 흐름을 설명합니다.

Extraction Shooter에서는 단순히 실력이 비슷한 플레이어를 모으는 것보다, **다른 플레이어를 대하는 성향이 비슷한 사용자를 같은 Match에 배치하는 것**이 게임 경험에 더 적합하다고 판단했습니다.

이를 위해 플레이어의 공격적인 성향을 나타내는 `aggression`을 주요 Matchmaking 지표로 사용합니다.

---

## 1. Matchmaking Design Goal

ARC Raiders를 플레이하며 체감한 플레이어 간 우호적 / 공격적 성향에 따른 게임 경험에서 아이디어를 얻었습니다.

Extraction Shooter에서는 다른 플레이어와 반드시 교전해야 하는 것은 아니며, 상대와 협력하거나 전투를 피하는 선택도 게임 플레이의 일부가 될 수 있습니다.

따라서 단순한 전투 능력이나 장비 수준보다:

> 다른 플레이어를 얼마나 공격적으로 대하는가

를 나타내는 `aggression`을 주요 Matchmaking 지표로 사용했습니다.

MatchMaker의 기본 목표는 다음과 같습니다.

```text
1. 비슷한 aggression을 가진 플레이어를 우선적으로 배치

2. Match 품질 때문에 사용자를 무한히 기다리게 하지 않음

3. 대기시간이 길어질수록 필요한 인원 조건은 완화

4. aggression 허용 범위는 일정 수준 이상 완화하지 않음

5. 충분한 인원이 모이지 않으면 Solo Match 허용
```

즉:

```text
Match Quality
      ↕
Waiting Time
```

사이의 균형을 맞추되, 지나치게 다른 플레이 성향의 사용자를 억지로 같은 Room에 배치하는 것은 피하도록 설계했습니다.

---

## 2. Match Size

하나의 `GameRoom`은 최대 **4명의 Player**를 수용합니다.

```text
GameRoom

┌─────────────────────┐
│ Player 1            │
│ Player 2            │
│ Player 3            │
│ Player 4            │
└─────────────────────┘
```

현재 프로젝트는 제한된 클라우드 자원과 소규모 Extraction Shooter의 게임 구조를 기준으로 설계했기 때문에 대규모 Match보다 작은 Room을 사용합니다.

대기시간이 증가하면 반드시 4명을 채우는 것을 고집하지 않고 Match 시작에 필요한 인원 조건을 단계적으로 완화합니다.

최종적으로는 조건을 만족하는 다른 플레이어가 없더라도 Solo Match를 시작할 수 있습니다.

```text
Wait Time 증가
      │
      ▼
Required Player Count 완화
      │
      ▼
...
      │
      ▼
Solo Match 가능
```

반면 `aggression` 차이는 대기시간이 길어진다고 해서 무제한으로 완화하지 않습니다.

---

## 3. Match Ticket and State Consistency

### Match Ticket Registration

클라이언트가 Matchmaking을 요청하면 HTTP API Server가 Redis에 Match Ticket을 생성합니다.

Match Ticket은 Matchmaking 요청의 외부 상태를 표현하며, Main Server의 C++ MatchMaker 내부 Queue와 별도로 존재합니다.

전체적인 흐름은 다음과 같습니다.

```text
Unity Client
      │
      │ Match Request
      ▼
HTTP API Server
      │
      ▼
Redis
Match Ticket 생성
status = WAITING
      │
      │ IPC
      ▼
Main Server
      │
      ▼
MatchMaker Queue 등록
```

HTTP API Server는 Main Server에 새로운 Ticket이 만들어졌음을 IPC로 전달합니다.

Main Server는 IPC를 받았다는 사실만으로 Ticket을 신뢰하지 않고, Redis의 Ticket 상태가 여전히 Matchmaking 가능한 상태인지 확인한 후 내부 Queue에 등록합니다.

즉:

```text
Redis Ticket
   │
   └── 외부 Match 상태

C++ MatchMaker Queue
   │
   └── 실제 Match 탐색을 위한 메모리 구조
```

로 역할을 구분합니다.


### Redis as Match State Source of Truth

사용자는 Matchmaking 도중 Cancel 요청을 보낼 수 있습니다.

문제는 HTTP API Server와 Main Server가 서로 다른 Process라는 점입니다.

예를 들어 다음 상황이 발생할 수 있습니다.

```text
Main MatchMaker                  HTTP API

Match 후보 탐색
      │
      │                         Cancel 요청
      │                              │
      │                              ▼
      │                         Redis Ticket 삭제
      │
      ▼
Match 확정 시도
```

Main Server의 메모리에 Ticket이 존재한다고 해서 해당 Ticket이 여전히 유효하다고 보장할 수 없습니다.

따라서 **Redis의 Match Ticket을 최종 Match 상태를 확인하는 Source of Truth로 사용합니다.**

이를 통해서 경쟁 상태를 해결할 뿐만 아니라,

- Match 요청 상태
- Match Cancel 여부
- Match 확정 여부
- Dedicated Server 할당 결과

를 여러 Process 사이에서 공유하기 위한 상태 저장소 역할을 합니다.


### Match Ticket State Machine

Ticket의 주요 상태는 다음과 같습니다.

```text
WAITING
   │
   │ Match Group 확정
   ▼
INPROGRESS
   │
   │ Dedicated Server 할당
   │ GameRoom 편입 완료
   ▼
SUCCESS
```

### WAITING

Matchmaking Queue에서 대기 중인 상태입니다.

이 상태에서는 사용자가 Match Cancel을 요청할 수 있습니다.

```text
WAITING
   │
   ├── MatchMaker
   │
   └── User Cancel
```

### INPROGRESS

Main Server가 해당 Ticket을 Match Group의 일부로 확정한 상태입니다.

이 시점부터는 사용자의 Match Cancel을 허용하지 않습니다.

`INPROGRESS` 이후에는 Match Group의 상태를 Main Server가 주도하며 Dedicated Server 할당과 GameRoom 편입이 진행됩니다.

이 단계에서 사용자 Cancel을 다시 허용하면 이미 확정된 Group과 후속 할당 작업을 되돌리는 별도의 Rollback 흐름이 필요하므로, `WAITING → INPROGRESS` 전환 이후에는 Cancel을 허용하지 않도록 했습니다.

Match Group의 소유권을 Main Server가 가져온 상태로 보고, 이후 Dedicated Server 할당과 GameRoom 생성까지 하나의 제어 흐름에서 처리합니다.

### SUCCESS

Dedicated Server 할당 및 GameRoom 편입까지 완료된 상태입니다.

클라이언트는 매치메이킹을 진행중인 경우, 2초 간격으로 status 요청을 보내게 되는데, 서버는 SUCCESS상태인 경우, 해당 플레이어에 맞는 token을 제공합니다.

이후 클라이언트가 콜백으로 token을 가지고 `/connect` 요청을 보내면, 해당 플레이어에게 할당된 서버의 IP와 Port, 그리고 securityKey를 응답으로 전송합니다.


### Atomic Match Commit with Redis Lua

MatchMaker가 메모리에서 Match Group을 구성했다고 해서 바로 Match가 확정되는 것은 아닙니다.

후보를 찾는 동안 사용자 중 한 명이 Match Cancel을 요청했을 수 있기 때문입니다.

예를 들어:

```text
MatchMaker Memory

Ticket A  WAITING
Ticket B  WAITING
Ticket C  WAITING
Ticket D  WAITING

        │
        │ 후보 Group 생성
        ▼

그 사이 HTTP API에서
Ticket C Cancel
```

이 상태에서 C++ 메모리만 보고 Match를 확정하면 이미 Cancel된 사용자를 GameRoom에 넣을 수 있습니다.

이를 방지하기 위해 Match Group 전체의 상태를 Redis Lua Script를 통해 원자적으로 검증합니다.

개념적인 동작은 다음과 같습니다.

```text
Match Group
A B C D
   │
   ▼
Lua Script
   │
   ├── A == WAITING ?
   ├── B == WAITING ?
   ├── C == WAITING ?
   └── D == WAITING ?
          │
       ┌──┴──┐
      YES    NO
       │      │
       ▼      ▼
모두      Match Commit
INPROGRESS   실패
```
구현 코드는 아래와 같습니다.

```cpp
bool MatchMaker::VerifyAndSetMatchStatus(const TicketVector& matchedGroup) {
    if (matchedGroup.empty() || pRedis == nullptr) return false;

    std::string luaScript = R"(
        for i = 1, #KEYS do
            local status = redis.call('HGET', KEYS[i], 'status')
            if status ~= 'WAITING' then
                return 0
            end
        end
        
        for i = 1, #KEYS do
            redis.call('HSET', KEYS[i], 'status', ARGV[1])
        end
        return 1
    )";

    std::vector<std::string> keys;
    keys.reserve(matchedGroup.size());
    for (MatchTicket* ticket : matchedGroup) {
        keys.push_back(ticket->ticketId); 
    }

    std::vector<std::string> args = {"INPROGRESS"}; 

    try {
        long long result = pRedis->eval<long long>(
            luaScript, 
            keys.begin(), keys.end(), 
            args.begin(), args.end()
        );
        
        return result == 1; // 1이면 전원 성공, 0이면 누군가 취소함
        
    } catch (const sw::redis::Error& e) {
        std::cerr << "[Redis Error] Lua 스크립트 실행 실패: " << e.what() << std::endl;
        return false;
    }
}
```

모든 Ticket이 여전히 `WAITING`인 경우에만 Group 전체를 `INPROGRESS`로 전환합니다.

검증과 상태 변경을 하나의 Lua Script 안에서 수행하므로:

```text
Check
   +
State Transition
```

사이에 다른 요청이 끼어드는 것을 방지합니다.


### Match Cancel

Match Cancel은 HTTP API Server에서 Redis의 Ticket 상태를 기준으로 처리합니다.

`WAITING` 상태인 Ticket만 취소할 수 있습니다.

```text
Cancel Request
      │
      ▼
Redis Ticket
      │
      ├── WAITING
      │      ↓
      │    Cancel
      │
      └── INPROGRESS / SUCCESS
             ↓
          Cancel Reject
```

이미 `INPROGRESS`로 넘어간 Match는 Main Server가 후속 처리를 진행 중인 상태이므로 사용자가 중간에 취소할 수 없습니다.

HTTP API Server에서 Cancel이 발생하더라도 Main Server의 C++ Queue에 별도의 Cancel IPC를 반드시 보낼 필요는 없습니다.

Main Server는 Match를 실제로 확정하는 시점에 Redis 상태를 다시 검증하기 때문입니다.

유효하지 않은 Ticket은 이후 Queue 정리 과정에서 제거됩니다.

---

## 4. Match Search Algorithm

### Aggression Buckets

전체 Matchmaking Queue를 하나의 Vector에서 반복 탐색하기보다 `aggression` 값에 따라 Bucket을 분리합니다.

```text
aggression

0  → [Ticket][Ticket][Ticket]...
1  → [Ticket][Ticket]...
2  → [Ticket][Ticket][Ticket][Ticket]...
3  → [Ticket]...
...
```

각 Bucket은 기본적으로 대기시간 순서를 유지합니다.

```text
Bucket[a]

Front
  │
  ▼
[Oldest][     ][     ][Newest]
```

따라서 Bucket의 가장 앞에 있는 Ticket은 해당 `aggression`에서 가장 오래 기다린 사용자입니다.


### Pivot-based Search

Match Cycle에서는 `aggression`이 낮은 Bucket부터 높은 Bucket까지 순서대로 탐색합니다.

```text
aggression 0
    ↓
aggression 1
    ↓
aggression 2
    ↓
...
```

각 Bucket에서는 가장 오래 기다린 Ticket을 **Pivot**으로 사용합니다.

```text
Bucket[aggression = A]

[Pivot][Ticket][Ticket][Ticket]
   ↑
해당 aggression에서
가장 오래 기다린 Ticket
```

Pivot을 기준으로 현재 Match 조건을 만족하는 Candidate를 탐색합니다.

같은 `aggression` 안에서는 Pivot보다 뒤에 있는 Ticket의 대기시간이 항상 더 짧습니다.

예를 들어:

```text
Pivot     40 sec
Ticket B  30 sec
Ticket C  20 sec
Ticket D  10 sec
```

현재 조건에서 가장 오래 기다린 Pivot조차 Match를 만들 수 없다면, 그보다 대기시간이 짧은 B/C/D 역시 동일한 Match Cycle에서 더 유리한 조건을 가질 수 없습니다.

따라서 특정 `aggression`의 Pivot이 Match에 실패하면 해당 `aggression`의 실패 상태를 같은 Cycle 동안 기억하고, 같은 Bucket에서 새로운 Pivot을 다시 선택하지 않습니다.

```text
aggression 0 → Match 실패
      ↓
같은 aggression의 추가 Pivot 탐색 생략
      ↓
aggression 1 탐색
      ↓
aggression 2 탐색
```

이 규칙은 `aggression = X`에서 실패했을 때 더 높은 aggression의 탐색까지 중단한다는 의미가 아닙니다.

**동일한 aggression Bucket 안에서 더 짧게 기다린 Ticket을 다시 Pivot으로 선택하여 같은 실패 경로를 반복하는 것을 방지하기 위한 최적화**입니다.


### Searching Similar Aggression

Pivot을 선택한 이후에는 Pivot과 동일하거나 가까운 `aggression`을 가진 Ticket을 Candidate로 탐색합니다.

```text
                 Pivot
                   │
                   ▼
aggression ... [A-1] [A] [A+1] ...
```

허용되는 `aggression` 차이는 처음에는 `±0`에서 시작하며, Pivot의 대기시간이 증가할수록 단계적으로 확장합니다.

따라서 초기에는 가능한 한 동일한 플레이 성향의 사용자를 우선적으로 Match합니다.

대기시간이 길어지면 인접한 aggression Bucket까지 Candidate 범위를 넓히지만, 매우 다른 플레이 성향의 사용자가 강제로 같은 Match에 배치되지 않도록 허용 범위에는 상한을 둡니다.


### Waiting Time Relaxation

Matchmaking의 목표는 Match 품질을 유지하는 것뿐 아니라 사용자가 지나치게 오래 기다리지 않고 게임을 시작할 수 있도록 하는 것입니다.

따라서 Pivot의 대기시간이 증가할수록 Match 조건을 단계적으로 완화합니다.

```text
Short Wait
   │
   ▼
가능하면 최대 인원 Match
   │
   ▼
Longer Wait
   │
   ▼
Aggression 검색 범위 확장
및 필요한 Player 수 완화
   │
   ▼
Longest Wait
   │
   ▼
Solo Match 허용
```

다만 두 종류의 조건은 동일하게 완화하지 않습니다.

```text
Waiting Time ↑

Required Player Count
        ↓
점진적으로 완화

Allowed Aggression Difference
        ↓
일정 범위까지만 확장
```

충분한 인원이 없을 경우 매우 다른 성향의 Player를 억지로 추가하는 것보다, 적은 인원으로 Match를 시작하는 것을 우선합니다.

---

## 5. Match Commit Failure and FIFO Preservation

### FIFO Preservation

Matchmaking에서 오래 기다린 사용자가 반복적으로 뒤로 밀리는 것을 방지하기 위해 Bucket 내부의 시간 순서를 유지합니다.

특히 Match Commit 과정에서 일부 Ticket이 Redis 검증에 실패할 수 있습니다.

```text
Candidate Group

A B C D
    │
    ▼
Redis Validation

C = Cancelled
```

이 경우 유효하지 않은 Ticket은 제거하지만, 다시 Queue에 남는 Ticket들의 기존 대기 순서는 유지하는 것을 기본 원칙으로 합니다.

```text
Before

A B C D E F

C invalid

After

A B D E F
```

이렇게 함으로써 실패한 Match 시도 때문에 기존 Ticket의 우선순위가 불필요하게 바뀌지 않도록 합니다.


### Match Commit Failure

Lua Script를 통한 Group Commit이 실패했다면 후보 중 하나 이상의 Ticket 상태가 이미 변경된 것입니다.

```text
C++ Candidate Group
       │
       ▼
Redis Commit
       │
      FAIL
       │
       ▼
Ticket 상태 재확인
```

이 경우 더 이상 유효하지 않은 Ticket은 Queue에서 제거하고, 여전히 Matchmaking 가능한 Ticket은 기존 우선순위를 최대한 유지한 채 다시 탐색 대상으로 남깁니다.

즉 Match 실패 자체 때문에 정상 Ticket을 모두 버리지는 않습니다.

---

## 6. Dedicated Server Handoff

Match Group이 Redis에서 `INPROGRESS`로 확정되면 MatchMaker의 후보 구성 단계는 종료되고, Main Server가 Dedicated Game Server와 GameRoom 할당을 진행합니다.

```text
Redis Commit SUCCESS
        │
        ▼
INPROGRESS
        │
        ▼
Dedicated / GameRoom 할당
        │
        ▼
Player 편입 완료
        │
        ▼
SUCCESS
```

Player의 Dedicated Server 편입까지 성공적으로 완료된 뒤 Match Ticket을 `SUCCESS` 상태로 전환합니다.

Dedicated Process의 선택, 생성, IPC 연결 및 GameRoom 할당 과정은 `dedicated-server.md`에서 자세히 설명합니다.

---

## 7. Search Cost and Complexity Trade-off

### Failure-heavy Path

Matchmaking Queue를 `aggression` Bucket으로 분리하고 Pivot 실패를 기억하는 이유 중 하나는 장기간 Queue에 남는 Ticket의 반복 탐색을 줄이기 위해서입니다.

Match에 성공한 Ticket은 결국 Queue에서 제거됩니다.

반면 Match에 실패한 Ticket은 다음 Cycle에도 계속 남아 Queue 크기를 증가시킬 수 있습니다.

따라서 설계 시 특히 중요하게 본 것은:

> Match 실패가 누적되는 상황에서 같은 Ticket들을 불필요하게 반복 탐색하지 않는 것

입니다.

```text
Success Ticket
     │
     ▼
Queue에서 제거

Failed Ticket
     │
     ▼
다음 Cycle에도 유지
     │
     ▼
장기 Queue 크기에 영향
```

Pivot Memorization을 통해 같은 `aggression`의 실패 경로를 한 Cycle에서 반복 탐색하지 않도록 합니다.


### Success-heavy Path

Pivot Memorization의 목적은 Match 실패가 누적되는 경로에서 동일한 `aggression`을 반복 탐색하는 비용을 제한하는 것입니다.

다만 현재 MatchMaker 전체가 항상 O(N)으로 동작하는 것은 아닙니다.

한 Bucket에서 Match 성공이 집중되면 이미 Match된 Ticket이 Cycle 종료까지 Container에 남아 이후 탐색에서 반복적으로 Skip될 수 있어, 특정 패턴에서는 최악의 경우 **O(N²)** 수준의 탐색이 발생할 수 있습니다.

```text
Bucket

[M][M][M][M][ ][ ][ ][ ]
 ↑
이미 Match된 Ticket

다음 탐색
→ 앞쪽부터 다시 Skip
```

현재 예상 Queue 규모에서는 이를 우선적인 병목으로 보지 않았으며, 장기간 Queue에 누적될 수 있는 실패 경로의 반복 탐색을 먼저 줄이는 방향을 선택했습니다.

이는 현재 구현의 명시적인 Trade-off입니다.

---

## 8. Design Constraints & Trade-offs


### Periodic Batch Matching

Matchmaking은 모든 Ticket 입력마다 즉시 전체 Queue를 재탐색하지 않고 주기적인 Match Cycle에서 Batch 형태로 처리합니다.

Ticket 입력마다 즉시 Queue 전체를 재탐색하지 않고, 일정한 Match Cycle 단위로 요청을 처리하기 위한 선택입니다.

### Single-threaded MatchMaker

Queue 상태 관리와 Match Group 구성의 복잡도를 줄이기 위해 현재 MatchMaker는 단일 실행 흐름에서 동작합니다.

현재 예상 규모에 맞춘 선택이며 대규모 Queue를 여러 Worker에서 동시에 처리하는 구조를 목표로 하지 않습니다.

### Redis State Verification

Main Server 메모리만을 최종 상태로 사용하지 않고 Redis Ticket 상태를 Match Commit 시 다시 확인합니다.

이는 추가적인 Redis 접근 비용을 발생시키지만 HTTP Cancel과 Main MatchMaker가 서로 다른 Process에서 동작하는 상황의 상태 일관성을 우선한 선택입니다.
