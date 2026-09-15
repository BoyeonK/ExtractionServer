# Authentication & Session Management

이 문서는 Salvage Protocol에서 **Account 인증과 Session을 어떻게 관리하는지**, 그리고 동일 Account의 중복 로그인이나 Match 참여 상태와 인증 흐름이 충돌하지 않도록 어떤 규칙을 사용하는지 설명합니다.

계정과 관련된 처리는 Node.js HTTP API Server가 담당하며, 실시간 게임을 처리하는 Dedicated Game Server는 Password와 같은 Account Credential을 직접 다루지 않습니다.

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
   │
   ├── MySQL   ── Account / Password / Persistent Data
   │
   └── Redis   ── Session / Active Match State
```

Client가 직접 접근하는 공개 API는 Cloudflare를 통해 HTTPS로 노출합니다.

Origin HTTP API Server의 ingress는 Cloudflare의 공개 IP Range만 허용하여 Cloudflare를 우회한 직접 HTTP 접근을 제한합니다.

---

## 1. Authentication Flow

### HTTP API에서 Account 처리

Account 생성, Login, Logout 및 Session 복구는 HTTP API를 통해 처리합니다.

대표적인 Endpoint는 다음과 같습니다.

```text
POST /api/signup
    └── Account 생성 + Session 발급

POST /api/login
    └── Password 인증 + Session 발급

POST /api/guest
    └── 비영속 Guest Session 발급

POST /api/logout
    └── Session 파기

POST /api/session/resume
    └── 기존 Session을 사용해 Lobby 상태 복구
```

Match와 관련된 API 역시 동일한 Session 인증을 사용하지만, Matchmaking과 Game State 자체는 별도의 문서에서 다룹니다.

### Login 이후 인증

Password는 Account 생성과 Login에서만 사용합니다.

Login에 성공하면 Server는 Redis에 Session을 생성하고 이후 HTTP 요청은 Client가 전달하는 Session ID를 통해 인증합니다.

```text
Login
  │
  │ ID / Password
  ▼
HTTP API Server
  │
  ├── MySQL Password 검증
  │
  ▼
Redis Session 생성
  │
  ▼
Session ID
  │
  ▼
이후 HTTP 요청의 인증 수단
```

따라서 일반적인 Game API 요청에서 Password를 반복해서 전달하지 않습니다.

---

## 2. Password Handling

### Password 저장

Password는 평문으로 저장하지 않고 **bcrypt Hash만 MySQL에 저장**합니다.

```text
Signup
   │
   │ Password
   ▼
bcrypt.hash()
   │
   ▼
users.password
```

bcrypt 결과는 알고리즘 정보, Cost, Salt와 Hash를 포함하는 고정된 형식을 사용하므로 `users.password`는 `CHAR(60)`으로 저장합니다.

Salt 역시 bcrypt Hash 문자열에 포함되어 있으므로 별도의 Salt Column을 두지 않습니다.

### Password 검증

Login에서는 입력받은 Password를 DB의 Hash와 직접 비교하지 않고 bcrypt를 통해 검증합니다.

```text
Login
   │
   │ Password
   ▼
bcrypt.compare(password, users.password)
   │
 ┌─┴───────┐
 │         │
Match    Mismatch
 │         │
 ▼         ▼
Login     401
Flow
```

Password Hash를 조회하는 경로는 Login 처리로 제한하며, Hash 자체가 Client Response에 포함되는 경우는 없습니다.

Login 이후에는 Redis Session을 사용하기 때문에 Password가 다시 Game API의 인증 정보로 사용되지 않습니다.

### 입력 형식

현재 Account 입력에는 다음 형식 제약을 둡니다.

```text
ID
- 4 ~ 16 characters
- [a-zA-Z0-9]

Password
- 4 ~ 16 characters
- [a-zA-Z0-9!@#$%^&*()]
```

Account 생성뿐 아니라 Login에서도 같은 형식을 검증합니다.

형식 자체가 유효하지 않은 입력을 bcrypt 비교 단계까지 전달할 필요가 없기 때문입니다.

---

## 3. Redis Session

### Session Key

하나의 Login에는 Redis의 두 Key가 사용됩니다.

```text
sess:<UUID>
    │
    ├── user_id
    ├── db_id
    ├── user_type
    ├── rating
    └── aggression


user_sess:<login_id>
    │
    └── sess:<UUID>
```

`sess:<UUID>`는 **Session ID를 기준으로 Account 정보를 조회하기 위한 Key**입니다.

반대로 `user_sess:<login_id>`는 Account에서 현재 유효한 Session을 찾기 위한 역방향 Key입니다.

```text
Session ID  ──▶ Account
      sess:<UUID>

Account     ──▶ Current Session
      user_sess:<login_id>
```

역방향 Key를 통해 동일 Account의 기존 Session을 찾고 파기할 수 있으며, **Account당 유효한 Session을 하나로 제한**합니다.

### Session TTL

두 Session Key는 동일한 TTL을 가집니다.

Session 인증이 필요한 HTTP Endpoint에서는 공통 **Auth Middleware**가 요청에 포함된 Session ID를 검증합니다.

Session이 유효한 것으로 확인되면 Middleware는 요청을 다음 Handler로 전달하면서 `sess:<UUID>`와 `user_sess:<login_id>`의 TTL을 함께 갱신합니다.

```text
Authenticated HTTP Request
        │
        ▼
Auth Middleware
        │
        ├── Session Validation
        │
        └── Session TTL Refresh
        │       ├── sess:<UUID>
        │       └── user_sess:<login_id>
        │
        ▼
Request Handler
```

따라서 Session은 최초 발급 시점으로부터 고정된 시간 후 만료되는 방식이 아니라, **인증된 HTTP 요청이 계속 발생하는 동안 수명이 연장되는 Sliding Expiration 방식**으로 관리합니다.

두 Key 중 하나만 먼저 만료되면 Account와 Session 사이의 양방향 관계가 어긋날 수 있기 때문에 두 Key의 TTL을 함께 갱신합니다.

### Match 중 HTTP 공백

Session TTL은 Client가 지속적으로 HTTP API를 호출한다는 가정만으로 결정하지 않았습니다.

Match에 진입하면 Client는 Dedicated Game Server와 Custom RUDP로 통신하므로 일정 시간 HTTP 요청이 발생하지 않습니다.

```text
POST /match/connect
        │
        ▼
     Game Loading
        │
        ▼
       Match
        │
        ▼
   Result Screen
        │
        ▼
POST /api/session/resume
```

따라서 Session TTL은 한 Match의 최대 진행 시간과 전후의 Loading / Result 구간을 포함할 수 있도록 설정했습니다.

Match 종료 후 Session이 이미 만료되었다면 Client는 일반 Login Flow로 돌아갑니다.

Game Result와 Inventory는 Session과 별도로 Persistent State에 반영되므로, Session 만료 자체가 Match 결과의 유실을 의미하지는 않습니다.

---

## 4. Duplicate Login

### 기본 정책

동일 Account에서 새로운 Login이 발생했을 때 무조건 기존 Session을 거부하거나 무조건 교체하지 않습니다.

현재 정책은 다음과 같습니다.

> **이미 Game을 진행 중이라면 새로운 Login을 거부하고, 그렇지 않다면 기존 Session을 새 Login으로 인수인계합니다.**

판정에는 Redis의 `active_match` 상태를 사용합니다.

```text
New Login
    │
    ▼
Password Validation
    │
    ▼
Check active_match
    │
 ┌──┴───────────────┐
 │                  │
In Game         Not In Game
 │                  │
 ▼                  ▼
Reject          Old Session
Login            Invalidate
                     │
                     ▼
                 New Session
```

### Password 검증 이후 Match 상태를 확인

`active_match` 판정은 **Password 검증이 성공한 뒤** 수행합니다.

인증 전에 Match 상태를 확인하여 서로 다른 Response를 돌려준다면, Password를 모르는 Client에게도 특정 Account가 현재 Game을 진행 중인지와 같은 상태를 노출할 수 있기 때문입니다.

따라서 순서는 다음과 같이 유지합니다.

```text
Account 존재 확인
       │
       ▼
Password 검증
       │
       ▼
Match State 확인
       │
       ▼
Session 처리
```

### Matchmaking Waiting 상태

`active_match`가 존재한다고 해서 모든 경우에 Login을 거부하지는 않습니다.

Player가 이미 실제 Game에 진입한 상태라면 Login을 거부하지만, 아직 Matchmaking을 기다리고 있는 상태라면 기존 Matchmaking을 취소하고 새로운 Login을 허용합니다.

```text
active_match
     │
     ├── In Game
     │      └── Login Reject
     │
     └── Waiting
            │
            ├── Match Ticket Cancel
            ├── MatchMaker Cancel IPC
            ├── Existing Session Invalidate
            │
            └── New Session Create
```

Waiting 상태까지 새로운 Login을 막으면 Client가 Matchmaking 중 비정상 종료된 경우 남아 있는 Lock 때문에 일정 시간 Account에 다시 접속하지 못할 수 있습니다.

따라서 **실제 Game 진행 상태와 Matchmaking 대기 상태를 구분하여 Login 정책을 적용**합니다.

Match Ticket을 제거할 때는 Redis 상태만 정리하지 않고 C++ MatchMaker에도 IPC로 Cancel을 전달합니다.

Redis Ticket만 제거하면 MatchMaker의 Memory Queue에는 더 이상 유효하지 않은 Ticket이 남을 수 있기 때문입니다.

---

## 5. Authentication Failure Response

Session 인증 실패는 Client 관점에서 다음과 같은 여러 원인을 가질 수 있습니다.

```text
Session Authentication Failure
        │
        ├── TTL Expired
        ├── Duplicate Login으로 Session 무효화
        └── Invalid / Forged Session ID
```

이 경우 Server는 세 원인을 Client에게 구분해서 전달하지 않고 동일한 `401` Response로 처리합니다.

세 경우 모두 Client가 해야 할 동작은 다시 Login하는 것이기 때문입니다.

또한 Invalid Session과 과거에 존재했던 Session을 Response만으로 구분할 수 있게 만들 필요도 없습니다.

반면 Login 요청에서 Account가 존재하지 않는 경우와 Password가 일치하지 않는 경우에는 현재 별도의 실패 사유를 반환합니다.

이는 현재 서비스의 User Experience와 구현 범위에 따른 정책입니다.

---

## 6. Guest Account

Salvage Protocol은 별도의 Account 생성 없이 Game Flow를 확인할 수 있도록 Guest Login도 지원합니다.

Guest는 MySQL에 Account Row를 생성하지 않습니다.

```text
POST /api/guest
       │
       ▼
Guest UID 발급
       │
       ▼
Negative db_id
(-1, -2, -3 ...)
       │
       ▼
Redis Session 생성
```

정식 Account의 `uid`는 양수를 사용하고, Guest에는 음수 `uid`를 할당하여 두 영역을 구분합니다.

Guest의 `uid`는 `user_inventory.uid`의 Foreign Key가 참조하는 정식 Account 영역에 존재하지 않기 때문에 Guest Inventory를 MySQL에 영속화할 수 없습니다.

따라서 Guest의 Inventory와 Currency는 Session 및 Game 실행 범위에서만 의미를 가집니다.

이 구분은 각 API에서 Guest 여부를 확인하는 조건문에만 의존하지 않고, **Database Schema의 Foreign Key 제약으로도 Guest Data의 영속화를 제한**합니다.

---

## 7. Session Resume

Match가 끝난 Client가 Lobby로 돌아올 때 정상적인 경우에는 ID와 Password를 다시 입력할 필요가 없습니다.

```text
Match End
    │
    ▼
POST /api/session/resume
    │
    ├── Session Validation
    ├── TTL Refresh
    │
    ▼
Lobby Data
```

`/api/session/resume` 역시 Session 인증이 필요한 요청이므로 Auth Middleware를 통해 기존 Session의 유효성을 검증하고 TTL을 갱신합니다.

Session이 아직 유효하다면 기존 Session을 그대로 사용해 Lobby State를 다시 가져옵니다.

Session이 만료된 경우에는 `401`을 반환하고 Client는 일반 Login Flow로 돌아갑니다.

따라서 Match 전후의 Account 인증과 Game Session은 분리되어 있으면서도, 정상적인 흐름에서는 사용자에게 반복 Login을 요구하지 않습니다.

---

## 8. Current Scope

### Password Recovery

현재 Password 변경 및 복구 기능은 구현 범위에 포함하지 않았습니다.

`users`에는 Email과 같이 Account 복구에 사용할 별도의 식별자가 존재하지 않기 때문에, Password Recovery를 추가하려면 Account Schema와 외부 인증 또는 전달 수단을 함께 확장해야 합니다.

### Transport Boundary

Client와 Cloudflare 사이의 공개 인터넷 구간은 HTTPS를 사용합니다.

현재 Cloudflare와 Origin HTTP API Server 사이에는 HTTP를 사용하며, Origin ingress를 Cloudflare의 공개 IP Range로 제한합니다.

따라서 현재 구조에서는 Password를 포함한 Credential이 직접 Client와 Origin 사이를 평문으로 이동하지는 않지만, Cloudflare 이후 Origin 구간까지 TLS를 유지하는 구성은 적용하지 않았습니다.

이는 현재 Public Cloud Deployment 구성의 제약이며, 배포 구조에 대한 자세한 내용은 Main README의 **Public Cloud Deployment** 항목에서 다룹니다.

