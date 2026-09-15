# Game State & Persistence

이 문서는 Salvage Protocol 서버가 플레이어의 상태를 **어디에 저장하고**, Lobby와 Match 사이에서 **어떤 기준으로 상태를 전환하는지** 설명합니다.

Extraction Shooter는 Lobby에서 보유한 Item을 Match에 반입하고, 사망하면 잃거나 탈출하면 다시 가져오는 구조를 가집니다.

따라서 단순히 데이터를 저장하는 것보다 다음 상태 전환을 일관되게 관리하는 것이 중요했습니다.

```text
Persistent                         Ephemeral
(MySQL)                     (Redis / Process Memory)

- Account                    - Session
- Currency                   - Match Ticket
- Inventory                  - Match Lock
                             - Entry Token
                             - In-Game Player / Item State
```

MySQL은 계정과 Inventory처럼 **서비스 재시작 이후에도 유지되어야 하는 상태**를 관리합니다.

Redis와 Dedicated Process Memory는 Session, Matchmaking, In-Game State처럼 **현재 실행 중인 서비스와 Match에 종속되는 상태**를 관리합니다.

---

## 1. Persistent / Ephemeral State

### Persistent State — MySQL

계정이 존재하는 동안 유지되어야 하는 데이터는 MySQL에 저장합니다.

대표적으로 다음 상태가 포함됩니다.

```text
users
 ├── Account
 ├── Currency
 ├── Rating
 └── Aggression

user_inventory
 ├── Item
 ├── Slot
 └── Quantity
```

Item Master Data 역시 MySQL을 원천으로 사용합니다.

전투에 필요한 Item Spec은 빌드 과정에서 C++ Header로 생성하여 Game Server가 사용하지만, 원본 데이터의 기준은 Database에 둡니다.

### Ephemeral State — Redis

Redis에는 현재 서버 실행 상태와 연결된 데이터를 저장합니다.

대표적으로 다음과 같은 상태를 관리합니다.

```text
Session
Match Ticket
Active Match Lock
Game Entry Token
Item Metadata Cache
```

이 데이터는 영속적인 Player Data가 아니라 로그인, Matchmaking, Game 입장 과정에서 필요한 **일시적인 Coordination State**입니다.

### Ephemeral State — Dedicated Process Memory

Match가 시작된 이후의 Player와 Item State는 Dedicated Game Server의 메모리에서 관리합니다.

```text
Lobby                       Match                       Lobby

MySQL       ──────▶   Dedicated Memory   ──────▶       MySQL
Persistent                  Ephemeral                  Persistent
```

Match 내부에서 발생하는 Item 획득, 장착, 재장전, 탄약 소모 등의 Game Logic은 DB에 직접 접근하지 않습니다.

이를 통해 실시간 Game Logic이 Database I/O에 의존하지 않도록 했습니다.

대신 이 구조는 명확한 Trade-off를 가집니다.

Dedicated Process가 비정상 종료되면 해당 Match의 Player / Item State를 복구할 수 없습니다.

현재 프로젝트에서는 이러한 복구 기능을 구현하지 않고, Match State의 주기적인 영속화보다 **구현 단순성과 실시간 처리 경로의 분리**를 우선했습니다.

---

## 2. Item Lifecycle

### Lobby → Match → Lobby

Player가 Match에 가져간 Item은 다음 Lifecycle을 따릅니다.

```text
Lobby Inventory (MySQL)
        │
        │ Match Entry
        ▼
Dedicated Game Server Memory
        │
        ├── Death / Disconnect
        │       │
        │       ▼
        │    Item Lost
        │
        └── Successful Extraction
                    │
                    ▼
              Main DB Proxy
                    │
                    ▼
          Lobby Inventory (MySQL)
```

Lobby에서는 Item이 MySQL에 영속화되어 있습니다.

Match 진입이 확정되면 반입 대상 Item은 DB에서 제거되고, 이후 Match가 진행되는 동안 Dedicated Process의 메모리 상태로 존재합니다.

Player가 사망하거나 연결이 종료되면 해당 Item은 영속 상태로 돌아오지 않습니다.

정상적으로 탈출한 경우에만 Dedicated Process가 보유하고 있던 Inventory State를 Main Server의 DB Proxy를 통해 MySQL에 다시 반영합니다.

### Match Entry를 두 단계로 분리

Match 진입 과정에서 Item을 DB에서 제거하는 시점은 `/match/start`가 아니라 실제 Game 접속 준비 단계인 `/match/connect`입니다.

```text
/match/start                     /match/connect
     │                                │
     ▼                                ▼
Inventory Snapshot 검증          반입 Item DB 제거
반입 목록 확정                   Game Entry 준비
     │                                │
     ▼                                ▼
Matchmaking Waiting             Dedicated Server
```

`/match/start`에서 Item을 바로 제거하면 Matchmaking을 취소하거나 Ticket이 만료될 때 Inventory를 복구하는 별도의 Rollback 경로가 필요합니다.

반대로 실제 입장 직전까지 DB 상태를 유지하면 Waiting 상태에서 Match가 취소되더라도 되돌릴 작업이 없습니다.

다만 Matchmaking이 시작된 이후 Client가 반입 Item을 변경하지 못하도록, 반입 목록 자체는 `/match/start`에서 Match Ticket에 확정합니다.

즉, **반입 Item을 확정하는 시점과 실제로 DB에서 제거하는 시점을 분리했습니다.**

### Match 종료와 Item 반영

Player가 GameRoom에서 분리될 때 Dedicated Process는 Main Server에 이탈 결과를 전달합니다.

정상 탈출인 경우 현재 Inventory와 Equipment를 함께 전달하고, 사망 또는 연결 종료인 경우에는 영속화할 Item을 전달하지 않습니다.

Main Server의 DB 반영 로직은 이를 기준으로 Lobby의 Match 반입 영역을 다시 구성합니다.

이 구조에서는 Item Loss를 별도의 복잡한 삭제 규칙으로 처리하기보다, **“Match에서 돌아온 Item만 다시 영속화한다.”**는 규칙으로 표현합니다.

따라서 새로운 이탈 사유가 추가되더라도 DB 반영 로직 자체보다 **어떤 결과를 영속화할 것인가**를 결정하는 쪽에 책임을 둘 수 있습니다.

---

## 3. Inventory Consistency

### Server Authority와 Client Snapshot

Lobby Inventory는 두 종류의 정보를 가집니다.

```text
Item Quantity   → Server Authority
Slot Layout     → Client Snapshot
```

Item 이동, 정렬, Stack 분할처럼 **전체 Item 수량을 변경하지 않는 조작**은 Client가 Local에서 처리합니다.

이러한 조작마다 HTTP 요청을 보내거나 DB를 갱신하지 않습니다.

대신 구매, 판매, Match 시작처럼 실제 Item 수량이나 영속 상태가 변경되는 요청을 보낼 때 Client가 현재 Inventory Snapshot을 함께 전달합니다.

```text
Move / Sort
    │
    ▼
Client Local Only


Purchase / Sell / Match Start
    │
    │ Inventory Snapshot
    ▼
Server Validation
    │
    ▼
Database Update
```

Server는 Snapshot의 Slot 배치 자체를 신뢰하기보다, 기존 DB Inventory와 **Item별 전체 수량**을 비교합니다.

Client 단독 조작으로 Slot 배치는 바뀔 수 있지만 Item의 전체 수량은 바뀔 수 없기 때문입니다.

검증에 성공한 경우에만 Snapshot을 영속 상태에 반영하고 요청받은 Item 변경 작업을 처리합니다.

이를 통해 Lobby UI의 단순 조작에는 Server Round Trip을 발생시키지 않으면서도, **Item Quantity 변경의 최종 결정은 Server가 담당합니다.**

### Snapshot 검증과 동시 쓰기

Snapshot 검증에는 별도의 경쟁 조건이 존재합니다.

```text
Request A

Inventory Read
     │
     │       Request B
     │       Inventory Update
     │             │
     ▼             ▼
Snapshot Rewrite
     │
     ▼
B의 변경 유실
```

Server가 기존 Inventory를 조회한 뒤 Snapshot을 다시 쓰기 전에 다른 요청이 Inventory를 변경할 수 있기 때문입니다.

이 경우 먼저 완료된 변경을 오래된 Snapshot이 덮어쓰는 문제가 발생할 수 있습니다.

따라서 Inventory를 검증하는 시점부터 재작성하는 시점까지를 하나의 Transaction으로 처리하고, Inventory 조회에는 `FOR UPDATE`를 사용합니다.

```sql
BEGIN;

SELECT item_id, quantity
FROM user_inventory
WHERE uid = ?
FOR UPDATE;

-- Snapshot validation

-- DELETE / INSERT / Currency update

COMMIT;
```

Inventory와 Account Data를 함께 변경해야 하는 경로에서는 Lock 획득 순서를 일관되게 유지합니다.

이 구조를 통해 구매·판매, Match 시작, Match 종료 반영처럼 동일한 Inventory에 접근할 수 있는 여러 경로가 서로의 변경을 오래된 Snapshot으로 덮어쓰지 않도록 했습니다.

---

## 4. Session & Match State

### Redis Session

로그인 이후 Client는 Password를 반복해서 전달하지 않고 Redis에 생성된 Session을 통해 인증합니다.

Session State는 일정 TTL을 가지며, 인증된 HTTP 요청이 들어올 때 갱신됩니다.

Lobby와 Match 사이에는 일정 시간 동안 HTTP 요청이 발생하지 않는 구간이 존재하므로 Session TTL은 한 Match의 최대 진행 시간을 고려하여 설정했습니다.

Match 종료 이후 Session이 이미 만료된 경우에는 Client가 다시 로그인하는 것을 허용합니다.

Inventory 결과는 이미 MySQL에 반영된 상태이므로 Session 만료 자체가 Game Result의 유실로 이어지지는 않습니다.

> Account Authentication과 Password 저장 방식의 상세 내용은 별도의 Authentication 문서에서 다룹니다.

### `active_match` Lock

하나의 Account가 동시에 여러 Match에 참여하는 것을 막기 위해 Redis에 User 단위의 `active_match` Lock을 사용합니다.

```text
POST /match/start
      │
      ▼
Acquire active_match
      │
   ┌──┴──┐
Success  Fail
   │       │
   ▼       ▼
Ticket    Reject
Create
```

Matchmaking Waiting 상태와 실제 In-Game 상태는 같은 Lock을 사용하지만 서로 구분할 수 있도록 상태를 관리합니다.

이는 Match Ticket과 Active Match Lock의 Lifetime이 서로 다르기 때문입니다.

Match Ticket이 먼저 만료되었더라도 Player가 이미 Game에 진입했다면 Lock을 해제해서는 안 됩니다.

따라서 Server는 **“Ticket이 존재하는가”와 “Player가 실제 Match에 참여 중인가”를 별개의 상태로 판단합니다.**

### Player Leave와 상태 확정

Player의 Match 결과는 Network Session이 최종적으로 파괴되는 시점이 아니라 **GameRoom에서 Player가 분리되는 시점**에 확정합니다.

```text
Player Leave
    │
    ▼
Detach From GameRoom
    │
    ├── Match Result 확정
    ├── Inventory Persistence
    └── active_match 해제
    │
    ▼
Session Cleanup
```

사망 이후 Spectating과 같이 Session이 일정 시간 더 유지될 수 있기 때문입니다.

Game Result와 Redis Lock 해제를 Session의 실제 종료 시점까지 미루면, 이미 Game에서 이탈한 Player가 Lobby로 복귀한 뒤에도 재매칭이 차단될 수 있습니다.

따라서 **Gameplay 상의 이탈 시점과 Network Session의 Lifetime을 분리**했습니다.

이탈 결과가 확정된 이후에는 Player의 Inventory를 더 이상 변경할 수 없도록 하여, Main Server에 전달된 상태가 이후 Game Logic에 의해 바뀌지 않도록 구성했습니다.

---

## 5. Startup & Failure Model

### Redis State Reset

현재 구조에서 Redis는 Persistent Storage가 아니라 실행 중인 Server Process와 연결된 Coordination State를 저장합니다.

Main Server가 재시작되면 기존 Redis State가 가리키던 Session, MatchMaker Queue, Dedicated Process 등의 메모리 상태는 더 이상 유효하지 않습니다.

예를 들어:

```text
Session        → 이전 Server 실행의 인증 상태
Match Ticket   → 사라진 MatchMaker Queue
Entry Token    → 사라진 Dedicated Process
Active Lock    → 더 이상 해제할 주체가 없는 Match
```

따라서 현재 단일 Server Instance 구성에서는 Main Server 시작 시 기존 Ephemeral Keyspace를 초기화하고 필요한 Cache를 다시 구성합니다.

이 결정에는 다음 전제가 있습니다.

```text
Main Server Restart
        │
        └── 기존 Login / Match State 무효화

Single Server Instance
        │
        └── 하나의 Redis Keyspace만 사용
```

즉 현재 방식은 단일 Compute Instance 규모에 맞춘 설계이며, 여러 Main Server Instance가 동일한 Redis를 공유하는 구조로 확장하려면 가장 먼저 변경해야 하는 부분 중 하나입니다.

---

## 6. Design Constraints & Trade-offs

### In-Game State는 복구하지 않는다

Match 중 Player와 Item State는 Dedicated Process Memory에만 존재합니다.

따라서 Dedicated Process가 비정상 종료되면 해당 Match의 진행 상태를 복구할 수 없습니다.

이를 해결하려면 Match Snapshot이나 Event Log를 주기적으로 외부 저장소에 남기는 별도의 복구 구조가 필요하지만, 현재 프로젝트의 범위에는 포함하지 않았습니다.

현재 구조에서는 이러한 복구 가능성보다 **실시간 Game Logic과 Persistent Storage의 분리, 그리고 구현 구조의 단순성**을 우선했습니다.

### Client Snapshot은 Slot의 진위를 검증하지 않는다

Inventory Snapshot 검증은 Item별 전체 수량을 기준으로 합니다.

따라서 동일한 수량을 유지하는 한 Slot 배치 자체는 Client가 결정할 수 있습니다.

이는 Lobby에서 Item 이동이나 정렬을 Local UI Operation으로 처리하기 위한 의도적인 선택입니다.

Slot 배치까지 Server Authority로 만들려면 모든 Inventory 이동을 Server에 전달해야 하며, 현재 구조는 그 비용보다 Lobby 조작의 단순성과 반응성을 우선했습니다.
