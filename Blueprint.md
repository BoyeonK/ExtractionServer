# Blueprint.md
사이즈 커지니까 세세한 부분을 자꾸 까먹음, 프로젝트 완성 전에는 README.md와 동일하게 관리
휴먼 에러 방지용으로 나를 위해서 작성, 나중에 포트폴리오 정리할 때도 도움이 될 거임.
본문이 길어지는 것을 방지하기 위해서 나무위키식 미주 사용함.
가능하다면, 이 파일을 베이스로 내가 보기 편하게 웹페이지 형식으로 만들까 고민중.

## 목차
1. 실행 전, 빌드 환경
2. 최초 실행시 동작 요약
3. 각 프로세스의 메인루프에서의 동작 요약

## 실행 전, 빌드 환경
Ubuntu noble 24.04, AMD64
C++ 17
Node.js
MySQL [1]
Redis

공개 환경에서는, 클라이언트의 HTTPS요청이 Cloudflare를 통해서 AWS EC2 인스턴스로 들어감. Cloudflare의 Flexible설정 적용.
클라이언트 -> Cloudflare는 HTTPS, Cloudflare -> AWS는 HTTP.[2]
로컬테스트환경에서는 클라이언트 -> 서버 HTTP로 즉시 쏨.

## 최초 실행 시
C = 메인프로세스(C++), H = HTTP서버(javascript), D = DedicateServer(C++) [3]

C1- 환경 변수 로드, 글로벌 변수 정의, IOUring객체 생성, Redis연결 및 핸들 생성.
C2- MySQL연결 및 핸들 생성, DB에서 메타데이터 가져와서 Redis에 캐싱. (만료시간 없음)
C3- HTTPS서버를 자식 프로세스로서 실행. (유일함)
C4- 인게임 로직 담당 DedicateServer를 자식 프로세스로서 1개 미리 띄워놓음. (유일하지 않음, 플레이어 수에 따라 유동)
이후, 메인프로세스의 메인루프 동작

H1- 환경변수를 읽음.
H2- Redis연결
H3- 메인 프로세스와 IPC통신 소켓 연결 [4]
H4- MySQL연결
이후, express서버 실행. HTTP프로세스의 메인루프 동작

D1,2- 환경 변수 로드, 글로벌 변수 정의, IOUring객체 생성.
D3- 메인 프로세스와 IPC통신 소켓 연결 [5]
D4,5- 유저의 UDP통신을 처리할 PacketHandler 초기화
이후, 데디프로세스의 메인루프 동작

---

[1] 공개환경 AWS RDS연동, 로컬테스트환경에서는 랩탑에 설치.
[2] Cloudflare의 공인 ipv4 주소를 모두 TCP 인바운드 규칙으로 지정함, 추후에 ipv6를 지정하지 않아서 생길 수 있는 문제는 고려하지 않음.
[3] 다른 프로세스끼리는 실행 순서를 보장하지 않음. 같은 프로세스안에서는 실행 순서 보장됨.
[4] Unix Domain Socket 사용.
[5] Unix Domain Socket 사용, 위의 경우보다 훨씬 복잡함. HTTP서버의 경우에는 유일하면서 메인프로세스와 생명 주기가 같기 때문에 고려할 사항이 별로 없으나, DediServerProcess는 여러개 존재할 수 있으면서 생명 주기가 다르기 때문에(플레이어 수에 따라서 늘었다 줄었다 함), 해당 프로세스를 다룰 FD(pid)와 해당 프로세스와 통신할 때 사용할 FD(socketFd) 2가지의 상관관계를, 최초 DedicateServer가 전달해준 pid정보를 통해 연결하고, 그 두 조합의 생명주기를 메인프로세스에서 관리해야 함.

---

## 메인프로세스에서의 루프
크게 분류해서 3가지 일을 한다.
1. IOUring에 쌓인 작업 진행. (네트워크 IO)
2. RedisProxy작업 진행.
3. 매치메이킹 진행.

### 1. 메인프로세스의 IOUring작업
CompletionQueue에 완료된 작업이 있는 경우, 해당 작업의 후처리를 진행한다.

이 작업을 진행하기 위해 IOTask라는, IO 완료된 작업을 추상화한 클래스를 만들었다.
이 인터페이스를 가상상속하여 Recv, Send등의 여러 작업들을 추상화 해 두었고, 그 작업들의 처리를 callback함수로서 일괄 작동하게 된다.
Windows IOCP의 overlapped를 사용한 경험을 토대로 만들었다.

1. CQ의 최상단의 cqe포인터를 IOTask*로 reinterpret_cast.
2. 해당 IOTask->callback(cqe->res); 실행. [6][7]
3. CQ의 최상단의 친구를 pop. (방금 작업 끝났으므로)

---

[6] cqe->res에는 보통, 처리된 IO의 크기(byte)가 들어있으므로, 이를 인자로 받아 callback처리.
[7] IOTask계열 객체는 모두 ObjectPool로서 관리됨, callback에는 반드시 Pool로 반환하는 로직 포함.

---

### 2. RedisProxy작업 진행
DedicateServer에서 Redis의 핸들을 사용하지 않도록 결정했다.
그렇지만서도 간혹 DedicateServer에서 Redis를 조작해야 하는 일이 있는데, 메인프로세스가 그 작업을 대리해서 처리해 주도록 설계했다. IPC를 통해서 메인프로세스는 Redis Proxy작업을 요청받는다. 필요한 경우 성공, 실패여부를 다시 IPC를 통해서 전달하는 방식이다. [8]

디자인 패턴은 위의 IOUring과 비슷하다.
RedisProxyService <= IOUring과 비슷한 역할.
PendingRedisRequest <= IOTask와 비슷한 역할, 이 인터페이스를 상속받는 여러 작업 종류들이 있음.

1. RedisProxyService의 내부 Queue에서 최상단의 PendingRedisRequest 가져옴.
2. 해당 PendingRedisRequest->Execute(_pRedis); 실행.
3. 최상단의 친구를 ObjectPool로 반환.

---

[8] 가능한 모든 경우에서 C++ 객체 사용, Redis는 기본적으로 싱글스레드 동작하기 때문에 여러 프로세스에서 같은 핸들을 사용하는 것이 병목이 될 거라고 개인적으로 판단함. 게임 로직을 담당하느라 초당 수백 수천단위의 패킷을 처리해야 하는 DedicateServer이기 때문에 Redis IO작업을 메인프로세스에 비동기방식으로 짬처리 시키기 위한 이유도 있다.

---

### 3. 매치메이킹 진행 (지루하고 현학적임)
1. HTTP서버에서 유효성 검사를 통과한 매칭 정보를 패킹에서 Redis에 저장. (Hash : 'ticket_UUID')
2. HTTP서버에서 메인 프로세스로 'ticket_UUID'의 매치메이킹을 유도하도록 IPC로 패킷 전송.
3. 메인프로세스에서 해당 유저의 agression수치와 매치 시작시간을 기준으로 매치 진행.
4. 매칭이 성공한경우(유효한 조합의 유저 그룹을 묶는데 성공한 경우), 현재 실행중인 DedicateServer중에서 해당 매칭의 인원을 수용할 수 있는 프로세스를 찾아 해당 유저 그룹을 할당.
    - 4-1. ticket_UUID의 조합을 묶어서 IPC로 전송함. 동시에 이 조합에 해당하는 유저를 나타내는 ticket_UUID에 status필드를 "INPROGRESS"로 전환(트랜잭션)하여 이 이후부터는 게임이 시작된 걸로 간주. 클라이언트가 매치 취소할 수 없음. 한명이라도 실패한 경우 (트랜잭션 롤백) 문제가 있는 ticket과 구조체를 파기하고 나머지 인원은 다시 매치대기열로 돌려보냄.
    - 4-2. 할당 가능한 DedicateServer가 없을 경우 새 프로세스를 실행함. 이 때, 새 프로세스가 온전히 준비되기 전까지 할당 로직이 작동하지 않으므로, 해당 유저 리스트들을 Queue에 넣어놓기만 하고 리턴함. 해당 DedicateServer가 준비 완료되었을 때에 메인 프로세스로 IPC요청을 보내는데, 그 IOTask의 콜백 함수로서 할당을 마저 진행함.
5. DedicateServer에서 매칭된 유저 그룹을 기준으로 GameRoom을 생성.
6. DedicateServer에서 매칭된 각 유저를 담당할 PlayerSession을 만들면서 접근 권한인 token을 생성함. 이제 /status요청에 응답하기 위해 메인프로세스에 'token_UUID' 필드 생성 및 ticket에 token을 연동하는 것을 요청.
7. DedicateServer의 응답을 받아 'token_UUID'필드를 만들고 ticket_UUID에 token정보를 포함시키고 status를 "SUCCESS"로 전환하여 다음 플레이어의 /status요청을 받을 준비를 완료함. token_UUID필드는 어떤 fd의 어떤 sessionID로서 playerSession이 만들어 졌는지에 대한 정보와, 만들어진 게임에 접속하기 위해 어떤 ip의 어떤 port에 해당 GameRoom이 준비되어 있는지를 포함하고 있음.
8. 올바른 접근 권한을 가진 /status HTTPS요청이 들어오면 응답으로서 token값을 돌려줌.
8. 이후, 올바른 접근 권한을 가진(token을 포함한) /connect HTTPS요청이 들어오면 응답으로서 클라이언트의 UDP통신에 필요한 'token_UUID'의 값[9]의 일부를 응답으로 돌려줌, 해당 값들을 토대로 클라이언트로 하여금 최초 UDP패킷을 보내도록 유도, 이때 /connect요청을 보낸 ip를 서버에서 기억해둔다.
9. 클라이언트로부터 받은 C2D_CHANNEL_OPEN패킷이 다음 조건에 모두 부합한다면 인증 성공으로 간주하며, 클라이언트의 포트를 PlayerSession에 바인딩한다.
    - 9-1. sessionId에 해당하는 PlayerSession에 바인딩된 IP주소가[10], 클라이언트의 IP주소와 일치.
    - 9-2. C2D_CHANNEL_OPEN의 헤더의 signature가 SecurityKey를 이용한 xxhash를 통해 만들어진 signature와 일치. (ACK bitfield를 이용하며, 유실됬을 경우 10번 까지 재전송함. 이 로직은 다른 중요한 UDP패킷에 모두 적용됨.)
10. 클라이언트의 IP와 인바운드로 열려있는 port도 아는 상황이다. 이제는 양방향 통신이 가능하여 게임 진행이 가능하다.
11. 매칭에 사용된 Redis의 'ticket_UUID'와 'token_UUID'를 파기한다.

---

[9] DedicateServer의 ip주소, 포트, 유저의 security_key, 유저의 session_id. 4가지.
[10] /connect 요청을 보낸 클라이언트의 ip주소가 세션에 미리 바인딩되어 있는 상황이다. 

---

## HTTP프로세스에서의 루프
Express 프레임워크를 사용함으로서, 백그라운드에 C++로 작성된 이벤트 루프가 돌기 시작함.
HTTPS요청이 들어오면,
1. 비동기 I/O 이벤트가 발생하고 (epoll, Windows환경이었다면 IOCP)
2. 이벤트에 맞는 자바스크립트 콜백 함수를 큐에서 꺼내고 (Node.js)
3. V8엔진이 자바스크립트를 초고속으로 기계어로 번역하여 실행한다.
어떤 요청을 처리하는지는 http-api-spec.yaml로 문서화 해 두었다.

## DedicateServer프로세스에서의 루프
서버를 하나의 거대한 프로세스단위로 실행할 경우, 에러 한방에 모든 것들이 정지할 가능성이 있다.
일정 수의 진행중인 게임을 다룰 독립적인 프로세스를 자식프로세스로 실행함으로서 만약에 생길 에러에 대한 파급력을 줄임과 동시에, 하나의 프로세스(스레드)에서 다룰 작업량을 적절하게 미리 분배해둠으로서 컨텍스트 스위칭 비용도 줄이기 위한 목적으로 만들어졌다.[11]

1. IOUring에 쌓인 작업 진행. (네트워크 IO)
2. ACK되지 않은 패킷에 대한 재전송 진행.
3. 작업이 없거나, 끝난 경우의 sleep

---

[11] 아직 허접이라 어느 수준으로 나누어야 합리적인 선인지는 모름.

---

### 1. DedicateServer의 IOUring작업
메인프로세스의 IOUring작업과 구조적으로 다르지 않다.
CompletionQueue에 완료된 작업이 있는 경우, 해당 작업의 후처리를 진행한다.
차이점이 있다면, UDP패킷을 다루는 부분이 추가되어 있고, UDP의 RecvTask를 시작부터 좀 많이 Pooling해놨다는 정도이다.

1. CQ의 최상단의 cqe포인터를 IOTask*로 reinterpret_cast.
2. 해당 IOTask->callback(cqe->res); 실행.
3. CQ의 최상단의 친구를 pop.

### 2. ACK되지 않은 패킷에 대한 재전송 진행.
일정 빈도로(현재 100ms), 이 프로세스에 할당된 PlayerSession에 ACK확인되지 않은 패킷에 대한 재전송을 진행한다.
한번에 모든 PlayerSession의 재전송을 진행하면 일종의 과부하가 생길 위험이 있기 때문에, SessionID별로 분할해서 진행한다. 현재 홀수, 짝수의 ID의 재전송을 번갈아 진행하는 방법으로 설계해 두었다.