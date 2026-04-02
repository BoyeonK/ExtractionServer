# windows로는 AWS EC2 프리티어 쌀먹을 할 수가 없어!

- TODO: 현재 진행중
  - 메인프로세스

  - HTTP 프로세스

  - 게임 서버(Dedicate Process)
    - ~~최초 연결 확인시, 매칭 완료된 플레이어를 다룰 가상 세션 생성.~~
      - ~~최초 연결 확인 : 매칭 완료시, status를 SUCCESS로 변환하고 token을 Redis에 저장~~
      - ~~클라이언트가 HTTPS 요청으로 ticket으로 token을 가져간 후, 해당 token으로서 HTTPS요청을 한번 더 보내도록 유도(/connect)~~
      - ~~/connect요청을 통해 받은 IP를 Dedicate Process에서 생성된 Session과 매칭, 해당 EndPoint에서 온 요청을 해당 Session의 요청으로서 처리~~
        - 해당 ticket에 해당하는 유저의 인벤토리에서 가져온 아이템만큼 차감 (HTTP서버로 IPC 요청 전송)
      - ~~해당 클라이언트에게는 sessionId를 발급 (프로세스 별로 unique한 값)~~
    - 프로토콜 정하고 에코 테스트 진행하기.

- TODO 
  - DedicateProcess에 TimerActor만들기
  - 아이템 테이블에 유의미한 아이템 채우기.

- 발견된 문제점
  - D2CRecvTask의 callback함수에서 readbytes가 0이거나, 음수인 경우의 예외처리 누락.