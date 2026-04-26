# Blueprint.md
사이즈 커지니까 세세한 부분을 자꾸 까먹음.
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

H1- 환경변수를 읽고, express서버 실행.
H2- Redis연결
H3- 메인 프로세스와 IPC통신 소켓 연결 [4]
H4- MySQL연결
이후, HTTP프로세스의 메인루프 동작

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

### 2. RedisProxy작업 진행
DedicateServer에서 Redis의 핸들을 사용하지 않도록 결정했다.
그렇지만서도 간혹 DedicateServer에서 Redis를 조작해야 하는 일이 있는데, 메인프로세스가 그 작업을 대리해서 처리해 주도록 설계했다. IPC를 통해서 메인프로세스는 Redis Proxy작업을 요청받는다. 필요한 경우 성공, 실패여부를 다시 IPC를 통해서 전달하는 방식이다. [8]

디자인 패턴은 위의 IOUring과 비슷하다.
RedisProxyService <= IOUring과 비슷한 역할.
PendingRedisRequest <= IOTask와 비슷한 역할, 이 인터페이스를 상속받는 여러 작업 종류들이 있음.

1. RedisProxyService의 내부 Queue에서 최상단의 PendingRedisRequest 가져옴.
2. 해당 PendingRedisRequest->Execute(_pRedis); 실행.
3. 최상단의 친구를 ObjectPool로 반환.

### 3. 매치메이킹 진행
// 너무길다. 나중에 작성

## HTTP프로세스에서의 루프


## DedicateServer프로세스에서의 루프

---
[6] cqe->res에는 보통, 처리된 IO의 크기(byte)가 들어있으므로, 이를 인자로 받아 callback처리.
[7] IOTask계열 객체는 모두 ObjectPool로서 관리됨, callback에는 반드시 Pool로 반환하는 로직 포함.
[8] 가능한 모든 경우에서 C++ 객체 사용, Redis는 기본적으로 싱글스레드 동작하기 때문에 여러 프로세스에서 같은 핸들을 사용하는 것이 병목이 될 거라고 개인적으로 판단함. 게임 로직을 담당하느라 초당 수백 수천단위의 패킷을 처리해야 하는 DedicateServer이기 때문에 Redis IO작업을 메인프로세스에 비동기방식으로 짬처리 시키기 위한 이유도 있다.

---