# windows로는 AWS EC2 프리티어 쌀먹을 할 수가 없어!

- TODO: 현재 진행중
  - 메인프로세스

  - HTTP 프로세스

  - 게임 서버(Dedicate Process)
    - ~~최초 연결 확인시, 매칭 완료된 플레이어를 다룰 가상 세션 생성.~~
      - ~~최초 연결 확인 : 매칭 완료시, status를 SUCCESS로 변환하고 token을 Redis에 저장~~
      - ~~클라이언트가 HTTPS 요청으로 ticket으로 token을 가져간 후, 해당 token으로서 HTTPS요청을 한번 더 보내도록 유도(/connect)~~
      - /connect요청을 통해 받은 IP를 Dedicate Process에서 생성된 Session과 매칭, 해당 EndPoint에서 온 요청을 해당 Session의 요청으로서 처리 
        - 해당 ticket에 해당하는 유저의 인벤토리에서 가져온 아이템만큼 차감 (HTTP서버로 IPC 요청 전송)
      - ~~해당 클라이언트에게는 sessionId를 발급 (프로세스 별로 unique한 값)~~
      - 클라이언트는 앞으로 패킷을 보낼 때, 헤더에 반드시 sessionId를 첨부해야함.
        - 헤더
          - packetId (2byte)
          - sessionId (2byte)
          - sequenceNum (4byte) : 도착순서보장용
          - securityKey (4byte) : /connect요청을 보낸 당사자가 맞는지 검증용, Session에 저장되며 매 패킷 검증.
          - flags (1byte) : 중요한 패킷의 경우, 재 전송 혹은 재 전송 요청을 위해 준비해 둠.
        - sessionId와 등록된 ip가 맞지 않으면, 서버 선에서 응답하지 않음.
    - 플레이어의 조작(이동, 사격, 장전, 상호작용)을 프로토콜 정하고 에코 테스트 진행하기.

- TODO 
  - 정상적으로 UDP연결이 성공한 경우, 해당 유저에 해당하는 Redis table처리
  - 아이템 테이블에 유의미한 아이템 채우기.
  - 총알 발사 및 플레이어 이동 테스트하기
  - 특정 몬스터의 어그로를 끈 경우, 어그로 끈 플레이어의 클라이언트에게 해당 몬스터 처리 맡기기
    - 서버의 자원을 많이 사용하지 않는다.
    - 그 몬스터의 행동은 클라이언트의 주인 플레이어에 대한 공격 or 서치만 허용함.(치팅방지)
    - 어그로 풀림 or 어그로 넘어감 잘 구현하기


- 발견된 문제점
  - D2CRecvTask의 callback함수에서 readbytes가 0이거나 음수인 경우의 예외처리 누락.
  - 최초의 D2CRecvTask가, ip가 session에 바인딩 되기 전에 일어나서 버려지는 것으로 추정됨.