# src/DedicateProcess/ — 전용 게임 프로세스

## 역할

UDP를 통한 클라이언트 통신, 플레이어/세션/게임룸 관리,
매치메이킹 알고리즘 실행, 아이템 시스템.

## 주요 파일 위치

| 구성 요소 | 파일 |
|-----------|------|
| 프로세스 진입점 | `DedicateMain.h/cpp` |
| 게임 서비스 코어 | `DediServerService.h/cpp` |
| UDP 세션 관리 | `DediSessions.h/cpp` |
| UDP 클라이언트 패킷 핸들러 | `ClientPacketHandler.h/cpp` |
| 플레이어 세션 | `PlayerSession.h/cpp` |
| 플레이어 상태 | `Player.h/cpp` |
| 게임 룸 | `GameRoom.h/cpp` |
| 타이머 스케줄러 | `TimerExecuter.h/cpp` |
| 매치메이킹 알고리즘 | `Matchmaker.h/cpp` |
| 게임 아이템 | `Items.h/cpp` |
| Unity 게임 오브젝트 (베이스) | `UnityGameObjects/UnityGameObject.h/cpp` |
| Unity 게임 오브젝트 (구체 타입) | `UnityGameObjects/TestGameObjects.h/cpp` |
| UDP 태스크 | `UDPTask.h/cpp` |
| 열거형 | `enum.h` |
| 외부 패킷 프로토콜 (컴파일 결과) | `ExternalProtocol/` |
