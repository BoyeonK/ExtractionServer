# src/DedicateProcess/ — 전용 게임 프로세스

역할: UDP를 통한 클라이언트 통신, 플레이어/세션/게임룸 관리, 아이템 시스템.

```
Client ──UDP──▶ DedicateProcess (src/DedicateProcess/)
                  └─ UDP 게임 세션 관리

Main C++ ──IPC──▶ DedicateProcess
                  (IPC_Dedicate.proto)
```

## UDP 패킷 형식 (클라이언트 ↔ Dedicate)

UDPHeader 35B 고정 헤더 + 페이로드(protobuf).
```
[8B: signature]
[2B: Packet ID][2B: Session ID]
[4B: rSeqNum][2B: uSeqNum][1B: Flags]
[4B: ackRSeqNum][4B: ackBitfield]
[4B: timestamp][4B: timestampEcho]
```
- `signature`: xxHash64(전체패킷(signature=0) + secKey) MAC. 서명 계산 시 이 필드는 0으로 세팅
- `rSeqNum`: reliable 채널 전용 시퀀스 (`FLAG_RELIABLE` 패킷에만 유효)
- `uSeqNum`: unreliable 채널 전용 시퀀스 (그 외 패킷에만 유효, wrap-around는 signed 차 비교로 처리)
- `ackRSeqNum`/`ackBitfield`: reliable 채널 ACK 피기백 (`FLAG_HAS_ACK` 세트 시 유효)

페이로드 정의는 `Protocol/ExternalProtocol/` 참조 (External_Protocol.proto, External_Unity_Object.proto).

## IPC 프로토콜

Main 프로세스와의 통신은 `Protocol/IPCProtocol/IPC_Dedicate.proto` 참조.

## 인벤토리 슬롯 구조 (`user_inventory.slot_index`)

| 범위 | 영역 |
|------|------|
| 0 ~ 79 | warehouse (창고) |
| 80 ~ 104 | inventory (인벤토리) |
| 105 ~ 107 | loadout (장착 슬롯) |

## 파일 목록

| 구성 요소 | 파일 |
|-----------|------|
| 프로세스 진입점 | `DedicateMain.h/cpp` |
| 전역 상태 (pDediServer, pTimerExecuter) | `DedicateGlobalVariable.h/cpp` |
| 게임 서비스 코어 | `DediServerService.h/cpp` |
| Dedicate 전용 세션 (D2MSession, D2CSession) | `DediSessions.h/cpp` |
| UDP 클라이언트 패킷 핸들러 | `ClientPacketHandler.h/cpp` |
| 플레이어 세션 | `PlayerSession.h/cpp` |
| 플레이어 상태 | `Player.h/cpp` |
| 플레이어 인벤토리·장비 관리 | `PlayerInventory.h/cpp` |
| 게임 룸 | `GameRoom.h/cpp` |
| 타이머 스케줄러 | `TimerExecuter.h/cpp` |
| 게임 아이템 | `Items.h/cpp` |
| 아이템 데이터 (타입·이름·스펙 조회, 정적 클래스) | `ItemDataManager.h` |
| Unity 게임 오브젝트 (베이스) | `UnityGameObjects/UnityGameObject.h/cpp` |
| 전투 오브젝트 (HP, Shield 공통 베이스) | `UnityGameObjects/CombatObject.h` |
| Unity 플레이어 오브젝트 | `UnityGameObjects/PlayerObject.h/cpp` |
| Unity 게임 오브젝트 (구체 타입) | `UnityGameObjects/TestGameObjects.h/cpp` |
| Unity 컨테이너 오브젝트 | `UnityGameObjects/Container.h/cpp` |
| UDP 태스크 | `UDPTask.h/cpp` |
| 열거형 | `enum.h` |
| 외부 패킷 프로토콜 (컴파일 결과) | `ExternalProtocol/` |
