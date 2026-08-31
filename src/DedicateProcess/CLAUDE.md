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

## 불변식·확정 결정

### 세션·이탈·사망 처리

- **이탈 통보는 `DetachPlayer()`에서 한 번만 나간다** — `D2MNotifyPlayerLeft`가 Main의 인벤토리 DB 반영과 `active_match` 락 해제를 촉발하므로 통보 시점이 곧 게임 결과 확정 시점이다. 세션 종료가 아니라 **룸 분리 시점**에 보내는 이유: 사망 유예 5초를 기다리면 그동안 락이 잡혀 먼저 로비로 간 클라이언트가 재매칭 409를 맞는다. 중복 방지는 `PlayerSession::NotifyLeftOnce()`의 `_leftNotified` 하나이고 `FinalizeLeave()` 쪽 호출은 분리를 건너뛴 세션용 백스톱. 통보 시점을 옮기려면 페이로드 동결 조건을 먼저 볼 것 — 지금 안전한 이유는 분리 시점부터 상향 요청이 차단돼 인벤토리가 변할 수 없어서다.
- **`SessionState::SPECTATING`은 "하향만 받고 상향은 전부 버린다"** — 사망 유예 5초 상태. 차단은 `IsActiveState()`에 이 상태가 없다는 사실 하나에 전적으로 의존하므로, 새 C2D 핸들러에서 그 게이트를 빠뜨리면 죽은 플레이어가 조작 가능해지고 컴파일도 정상 플레이도 그것을 드러내지 않는다. 의도적 예외는 `Handle_C2D_HeartBeat` 하나(막으면 유예의 목적이 깨진다) — 상태를 바꾸지 않는 요청에만 허용할 것. 유예를 끝내는 것은 `FinalizeLeave()`의 `LEFT` 전환이다.
- **지연 콜백은 예외 없이 sessionId 재조회 규약을 따를 것** — `TimerExecuter`에 취소 API가 없어 정리 후에도 잔여 콜백이 실행될 수 있다. `PlayerSession*`를 캡처하지 말고 sessionId로 매번 재조회하며 uid·generation을 검증할 것(`RecallTick()` 방식) — 그러면 정리 시 `_players[id] = nullptr`만으로 잔여 콜백이 스스로 포기한다. **한 곳이라도 raw 포인터를 캡처하면 무너진다** — sessionId가 FIFO로 재사용되므로 규약을 어긴 콜백은 살아 있는 남의 세션을 잡는다.

### UDP 신뢰성·시퀀스

- **reliable in-flight 33개 초과 시 영구 ACK 불가** — ACK 커버리지는 `ackRSeqNum` 1개 + 비트필드 32개 = 33개. `_pendingReliable`에 34개 이상 쌓이면 가장 오래된 것은 클라이언트가 이미 받았어도 알릴 수단이 없어 세션 끝까지 재전송된다(수신 측이 `diff > 32`로 버려 정확성은 유지, 대역폭만 낭비). `RegisterReliable()`의 34장 진입 로그가 유일한 신호다. 현재 reliable 브로드캐스트는 사망 1건당 3장(시신 스폰 + 킬 피드 + 플레이어 디스폰) 수준이라 여유가 있지만, 대량 통보는 개별 전송 대신 청킹으로 묶을 것. 근본 해결(비트필드 64비트 확장 또는 송신 창 제한)은 별도 작업.
- **끊김 감지는 `PlayerSession::_lastEchoTs` 하나에 달려 있다** — `MarkLeaving(DISCONNECTED)`는 `GameRoom::DetectDisconnectedSessions()`가 `timestampEcho` 6초 미갱신을 볼 때만 불린다. 전제 셋: ① 클라이언트가 0.1초 주기로 상태를 보낸다(6초 = 연속 60회 유실. 주기를 늘리면 마진이 깎인다) ② 갱신 지점은 `UpdateRtt()` 하나이고 **역행하는 에코는 버린다**(재전송분이 낡은 값을 실어 온다) ③ `echo == 0`이면 판정을 건너뛰므로 클라이언트가 안 채우면 감지가 조용히 무력화된다(`OPTION:`으로 남김). 수신 시각 기반으로 바꾸면 `_clientAddr`이 갱신되지 않아 NAT 리바인딩 시 하향만 죽는 경우를 놓친다.
- **재전송 패킷의 헤더는 최초 송신 시점 값이다(양방향)** — 보관 버퍼를 그대로 복사해 보내므로(헤더를 고치면 서명이 전체 패킷 해시라 재서명이 따라온다) 재전송분의 `timestamp`·`timestampEcho`·ACK 필드는 신뢰할 수 없다. 방어는 수신 측 **단조 갱신** 두 곳이 전부다 — `UpdateRtt()`의 `_lastEchoTs`와 `SetLastRecvTimestamp()`의 `_lastRecvTimestamp` — 되돌리면 끊김 오판 킥이 즉시 되살아난다. 재스탬프+재서명의 재검토 조건은 클라이언트가 "중복 패킷이어도 헤더 필드는 반영한다"를 확정하는 시점이며, 그때는 중복 reliable을 헤더 처리 전에 버리는 조기 `return`(`ClientPacketHandler.h`)도 함께 볼 것.
- **응답 없는 세션의 재전송 큐는 이탈 처리로만 정리된다** — 개별 제거는 `ProcessIncomingAck()`, 일괄 정리는 `DetachPlayer()`의 `ClearPendingReliableExcept()`뿐. 살아있는 세션에는 무한 재전송하므로 큐의 상한을 실질적으로 정하는 것은 끊김 감지 6초다 — 임계값을 늘리면 죽은 세션의 큐가 그만큼 오래 남아 `GetRetransmitCandidates()`가 매번 전체를 순회한다.
- **`uSeqNum`은 모든 unreliable 패킷이 공유한다** — `UpdateURecvState()`가 최고 시퀀스 이하를 전부 드롭하므로, 드물게 나가는 unreliable 패킷은 매 틱 나가는 이동 패킷보다 조금만 늦어도 핸들러에 도달하지 못한다. **unreliable로 오는 값에 "직전 값 + 1" 연속성을 요구하지 말 것** — 단조 증가만 요구하고 간격은 받아들인다.
- **reliable C2D 채널은 순서를 보장하지 않는다(중복만 거른다)** — 늦게 도착한 낮은 시퀀스도 처음 보는 것이면 처리되고, reliable/unreliable 시퀀스 공간은 분리돼 두 채널 사이에 순서 개념이 없다. **상태를 바꾸는 C2D 요청은 토글·증분이 아니라 절대 지정으로 설계할 것.** 다른 조작과의 순서 역전은 `my_inventory_version` 가드로 막는다(`C2DRequestEquipItem`·`C2DRequestSwitchWeapon`의 관용구. `PlayerInventory`의 변경 함수 전부가 `_inventoryVersion`을 올리는 것이 전제다). **D2C 방향도 같다** — 상태 전체를 절대값으로 싣고 클라이언트가 헤더 `rSeqNum`으로 낡은 통보를 버리게 한다(현재 `D2CNotifyWeaponChanged` 하나. 새 통보에 이 규약이 필요해지면 서버가 `.proto`에 지시를 적어 전달한다).
- **브로드캐스트 페이로드 재직렬화는 의도적 보류** — N명 전송 시 페이로드가 N번 직렬화되지만 절약분(4인/100ms 기준 룸당 ~3μs)이 같은 경로의 `sendto` 비용에 압도되고, `Make*FromPayload` 짝 함수가 패킷마다 늘어나 `pktId`/`reliable`이 어긋날 여지가 생기는 쪽이 손해다. 재검토 조건: 룸 인원 10명 초과 / 틱 25~50ms 상향 / 동시 룸 세 자릿수.

### 패킷·프로토콜 설계 규칙

- **패킷을 추가하면 `enum.h`와 `.proto`의 `enum PktId`가 짝이다** — 서버는 `.proto` PktId를 참조하지 않으므로 빠뜨려도 서버는 정상이고, 생성 코드를 쓰는 클라이언트만 id 상수를 얻지 못한다. `PKT_ID_MAX`도 함께 올릴 것 — 핸들러 배열 크기이자 수신 범위 검사 기준이라 빠뜨리면 새 C2D 패킷이 조용히 버려진다. 이 규율은 두 번 깨진 적이 있다 — 나머지를 다 맞추면 "다 했다"는 착각이 들고 빌드도 통과하므로, **패킷 추가 시 `enum PktId` 확인을 마지막 단계로 따로 둘 것.**
- **가해자 id 필드는 예외 없이 명시적으로 채울 것** — `attacker_object_id`·`killer_object_id`의 "없음"은 `0xFFFFFFFF`(`CombatObject::NO_ATTACKER`)다. proto3 기본값 0은 실재하는 object_id라 설정을 빠뜨리면 0번에 오귀속된다 — 가해자 없는 피해 경로(회복·낙하·지역 피해)가 붙는 시점에 걸린다. 이름 쪽은 반대로 빈 문자열 하나로 통일 — `killer_object_name`은 가해자 부재와 조회 실패를 구분하지 않는다(`killer_object_id`가 이미 판별자다). 조회 실패는 정상 플레이에서 실제로 발생한다(사망 유예 중 가해자 퇴장).
- **와이어에서 "비어 있음"은 `0xFFFFFFFF`이고 `0`이 아니다** — proto3 기본값 0을 센티널로 쓰면 "명시적으로 비웠다"와 "필드를 안 실었다"가 구분되지 않는다. 현재 일가: `PLAYER_OBJECT_ID_SENTINEL`·`INVENTORY_VERSION_NOT_SET`(`ClientPacketHandler.cpp` 상단)·`CombatObject::NO_ATTACKER`. 서버 내부 `Slot::IsEmpty()`의 `blueprintId == 0`은 와이어에 나간 적 없는 값이므로 혼동하지 말 것.
- **중계 통보 설계 규칙 셋** — ① C2D 패킷에 발신자 자신의 `object_id`를 싣지 말 것 — 서버가 세션에서 이미 안다. 필드가 없으면 스푸핑이 구조적으로 표현 불가능해진다. `weapon_dbid`·`recall_spot_index`처럼 서버가 모르는 값이거나 불일치 자체가 의미를 갖는 에코와 다르다 ② 재시작하는 단계 번호를 unreliable에 올려도 되는 판단 기준은 "수신 측이 직전 단계 도착을 전제하는가"다 — 전제하면 reliable, 각 단계가 독립이면 unreliable로 충분하다(재장전 연출이 후자 — 번호가 상태가 아니라 연출 큐라서다). 상태를 옮기는 번호는 여전히 단조 증가여야 한다 ③ 한 사건의 권위 있는 결과(reliable)와 연출 신호(unreliable)는 채널을 나눠도 되지만 **발행 지점이 갈리면 안 된다** — 서버가 발행하는 값(재장전 완료 단계 15)이 클라이언트에서 오면 버릴 것. 안 그러면 거부된 재장전이 남의 화면에서 완료로 보인다.
- **조작 요청의 `object_id` 일치 검사는 진단이 아니라 교차 오염 방지다** — 조작·장착 핸들러는 대상 컨테이너를 패킷이 아니라 **세션의** `_interactingContainerId`로 해석한다. 세션이 Y를 가리키는 동안 늦게 도착한 X 대상 요청을 걸러내지 않으면 **X 기준의 슬롯 인덱스가 Y의 슬롯에 적용된다.** 버전 가드는 대체재가 못 된다 — `_containerVersion`은 컨테이너마다 0에서 시작하는 개별 카운터라 방금 연 둘은 값이 같기 쉽고, 그러면 검사를 그대로 통과한다. 막는 것은 이 id 일치 검사(`DENY_CONTAINER_NOT_OPEN`) 하나뿐이니 리팩터링에서 지우지 말 것.
- **`DENY_SERVER_INTERNAL`은 재시도가 무의미한 진짜 내부 오류 전용이다** — reliable C2D 채널이 순서를 보장하지 않으므로 `C2DCloseContainer`가 앞선 컨테이너 조작보다 먼저 처리되는 역전은 정상 플레이에서 나온다. 그 다섯 자리(interact 3곳·equip 2곳)는 `DENY_CONTAINER_NOT_OPEN`(0x0400)로 갈라져 있다 — 클라이언트가 "재시도 무의미한 서버 오류"와 "컨테이너가 이미 닫혔으니 UI만 되돌릴 것"을 구분해야 해서다. 새 deny 사유를 추가하면 `enum.h`의 `DenyReason`과 `.proto` 두 Deny 메시지의 주석 목록이 짝이다.
- **`D2CFullInventorySync`는 청킹도 크기 검사도 하지 않기로 확정** — 최악 834B로, 실 구속인 수신 버퍼 1024B(`recvfrom`은 초과분을 단편화가 아니라 **절단**으로 처리한다)의 85%다. 최악값 재계산 시 주의 둘: ① `instance_uid`는 BIGINT AUTO_INCREMENT라 varint 3~5B에 머문다 ② 탄창 두 칸의 `slot_index = -1`이 음수 int32 varint 10B로 가장 비싼 슬롯이다. 인벤토리 칸이나 장착 슬롯을 늘리는 변경이 오면 재계산할 것.

### 전투·인벤토리 설계 확정

- **`damageReductionRate`는 실드 잔량이 있을 때만 유효하다** — 실드 소진 후 penetrated 경로에는 적용되지 않으며 의도된 설계다. armor는 "상시 방어력"이 아니라 "실드가 버티는 동안의 감쇄"로 읽을 것 (`CombatObject.h`).
- **남의 HP·실드 수치와 방어구 정보는 클라이언트에 보내지 않는다** — `D2CNotifyHealthChange`(hp·shield 절대값)는 피격 당사자에게만 간다. 방어구는 외형에 드러나지 않고 `blueprint_id` 하나가 `ArmorSpec` 전체와 동치라 id만 흘려도 수치 노출이다. **새 D2C 패킷에 남의 hp·실드·방어구를 싣지 말 것.** `weapon_id`는 외형에 보이므로 예외이고, 히트마커처럼 bool만 돌려주는 것은 충돌하지 않는다.
- **방어구 착용 시 실드 0은 의도된 설계다** — 만충이나 비율 유지로 바꾸면 방어구를 쟁여두고 스왑해 실드 파괴 리스크를 지지 않는 플레이가 가능해진다. 만충 예외는 스폰 시 `ChargeShield(GetMaxShield())` 하나, 이후 회복 수단은 `RegenShield()`뿐. **피격 후 재생 지연은 없다** — `ArmorSpec`에 지연 필드가 없어서이며, 필요해지면 필드 하나와 `_lastDamagedMs` 추가로 열린다.
- **사격 거부는 조용하다** — `weapon_dbid` 불일치·`fire_sequence` 역행·탄약 부족 셋 모두 deny 없이 `return false`이고, 거부가 브로드캐스트보다 앞이라 남에게 가는 발사 이펙트도 사라진다. 증상 제보를 받으면 서버 로그의 `[Handle_C2D_RequestWeaponFire]` 줄부터 볼 것. **잔탄 0 통보는 붙이지 않기로 확정** — 클라이언트는 트리거 시 차감, 서버는 수락 시만 차감이라 서버 잔탄 ≥ 클라이언트 잔탄이 항상 성립하고, "클라이언트는 남았다고 믿는데 서버가 0"인 상태는 정상 클라이언트에서 발생할 수 없다.
- **컨테이너 열기 실패가 조용한 것은 확정이다** — `Handle_C2D_RequestOpenContainer`의 다섯 갈래(룸 없음·오브젝트 못 찾음·`Container` 타입 아님·요청자가 범위 밖·남이 점유 중)가 deny 패킷도 로그도 없이 `return false`다. 앞의 셋은 정상 플레이에서 나오지 않고, 넷째는 클라이언트가 2m 기준으로 상호작용 표시를 띄우지 않는 거리다 — 유저에게 보이는 침묵은 "남이 점유 중" 하나이고, 3m 안이라 정황으로 읽힌다. 요청 도달 자체는 헤더 ACK로 확인되므로(`UpdateRRecvState()`는 핸들러 결과와 무관하게 돌고 ACK는 다음 하향 패킷에 실린다) 클라이언트는 "도달했는데 무응답 = 거절"과 "유실"을 구분할 수 있다. **deny 패킷 신설을 제안하지 말 것.** 감수하는 예외 하나 — 엄폐물 뒤 시신은 3m 안이어도 점유자가 안 보인다.
- **탄창 폐기와 낙관적 탄종 기대는 둘 다 설계 확정이다** — ① `UnloadMagazineToInventory()`가 자리 없으면 탄창을 폐기하고 `true`를 반환하는 것은 의도다(장전된 탄은 소비된 것에 준함. `return false` 경로가 없어 호출부 가드와 `DENY_MAGAZINE_UNLOAD_FAILED`는 죽은 코드지만 방향 전환 여지로 유지) ② `LoadMagazineFromInventory()`에 탄창 자신의 탄종 검사가 없는 것도 의도다 — 무기 슬롯 점유자를 바꾸는 두 경로(`EquipWeaponFromSlot`·`UnequipWeaponToSlot`) 전부가 같은 호출 안에서 unload를 부르므로 탄창은 "그 슬롯 무기의 탄종" 아니면 "빈 것"뿐이다. **unload 없이 스왑하는 경로를 만들면 이 기대가 무너진다** ③ 재장전 결과는 클라이언트가 미러할 수 없다 — 어느 칸에서 몇 발씩 빠지는지가 서버의 스캔 순서에 달려 있어서이고, 재장전 응답이 인벤토리 전체 스냅샷(`D2CFullInventorySync` 중첩)을 싣는 이유다.
- **탄약 차감의 규약 둘** — ① `_inventoryVersion`을 올리지 않는다 — 발사마다 올리면 교전 중 장착·무기 교체가 전부 `DENY_VERSION_MISMATCH`로 거부된다. 클라이언트-서버 잔탄 어긋남은 명시적 동기화 전까지 정상이다 — **보정 로직을 넣지 말 것.** 서버가 최종 판정 권한을 갖는다 ② 잔량 0인 슬롯을 남기지 않는다 — 차감 직후 `Clear()`. `Slot::IsEmpty()`가 `quantity <= 0 && blueprintId == 0`이라 0발 점유 슬롯이 전리품 컨테이너·인벤토리에 문제를 만들므로, 수량을 줄이는 코드를 새로 쓸 때마다 같은 처리를 붙일 것.
- **장비 슬롯 조작은 거부가 아니라 스왑이고, 타입 검사는 들어가는 쪽에만 건다** — 상대편 칸이 차 있으면 `DENY_SLOT_NOT_EMPTY`(0x0004, 컨테이너 get 전용 비트)를 쓰지 않고 자리를 맞바꾼다. 검사 대상은 **장비 슬롯으로 들어갈 아이템**이다 — 장착은 `srcSlot`, 해제는 `dstSlot`을 보고 무기/방어구가 아니면 `DENY_ITEM_TYPE_MISMATCH`. 그래서 탄약 칸으로 무기를 해제하는 것은 거부되고, 무기 슬롯에 탄약이 들어앉는 경로는 없다(별칭 버그로 그런 상태가 만들어졌던 것은 아래 항목에서 막았다 — 원인이 다르니 혼동하지 말 것). 맨손 금지는 목적지가 빈 칸일 때만 검사한다 — 스왑은 언제나 무기를 남긴다. **`.proto`의 0x0040 설명이 "장갑 슬롯에 무기"만 예로 들던 탓에 클라이언트가 "해제 방향은 검사하지 않는다"로 읽은 적이 있다**(적용 범위를 명시하도록 고쳤다) — 거부 사유를 좁게 적지 말 것.
- **무기 슬롯 조작 뒤 그 슬롯의 탄창은 예외 없이 비어 있다** — 언로드가 네 갈래(빈 탄창·합침·이동·폐기) 전부에서 `magSlot.Clear()`로 끝나고, 자동 장전은 없다(`LoadMagazineFromInventory()`의 호출부는 생성자와 `ReloadMagazine()`뿐). 인벤토리에 놓인 무기는 탄창 상태를 들고 다니지 않는다 — `Slot`에 그런 필드가 없고 탄창은 장비 슬롯 두 개에만 딸려 있다. **귀결: 무기를 뺐다 다시 끼우면 탄약이 초기화되고 수동 재장전이 필요하다.** 클라이언트의 탄창 표시 계산은 이 성질에 기댄다.
- **`UnequipWeaponToSlot()`은 무기를 옮긴 **뒤에** 언로드한다 — 목적지 칸 별칭 때문이다** — `dstSlot`은 `_inventorySlots[k]`의 참조이고, 언로드를 먼저 하면 목적지가 최소 인덱스 빈 칸일 때 규칙 ②가 그 칸을 골라 채운다. 그러면 뒤따르는 `dstSlot.IsEmpty()` 분기가 뒤집혀 swap으로 빠지고 **무기 슬롯에 탄약이 들어앉는다**(`SetWeapons()`가 탄약 id를 무기로 방송하고, 재장전·발사가 조용히 죽는다). 검사 시점엔 목적지가 비어 있어 타입 검사와 맨손 금지도 함께 우회된다. `EquipWeaponFromSlot()`은 `srcSlot`이 항상 점유 상태라 같은 문제가 없어 순서를 그대로 뒀다. **언로드 위치를 되돌리지 말 것.**
- **`_firstEmptySlotIndex`는 인벤토리 칸의 점유를 바꾸는 모든 경로에서 갱신해야 한다** — 규칙 ②의 목적지를 정하는 값이라 낡으면 두 가지로 깨진다. 실제보다 작은 쪽을 가리키면(찬 칸을 가리키면) 언로드가 **거기 있던 아이템을 덮어써 소멸시키고**, 큰 쪽을 가리키거나 `-1`이면 서버가 클라이언트와 다른 칸을 고르거나 빈 칸이 있는데도 폐기한다 — 규칙 공유가 배치 단계에서 갈린다. 갱신이 빠져 있던 `*Slot` 계열 넷에 붙였다. 새 조작 경로를 만들면 `Clear()`·대입·`swap` 뒤에 `UpdateFirstEmptySlotIndex()`가 짝이다.
- **무기 슬롯 조작의 탄창 언로드 결과는 "규칙 공유 + 검산 필드"로 통보한다** — `D2CResponseEquipItem`은 조작의 좌표만 싣고, 클라이언트가 `UnloadMagazineToInventory()`와 **같은 규칙**(같은 탄종 스택을 인덱스 오름차순으로 스캔해 첫 일치 칸에 합침 → 없으면 최소 인덱스 빈 칸 → 빈 칸도 없으면 폐기)으로 배치를 계산한다. 논거는 요청의 버전 가드가 "조작 직전 양측 상태가 같았다"를 보장하고 언로드가 순수 함수라는 것이다. **그러나 버전은 연산 횟수를 셀 뿐 내용을 증명하지 않는다** — 탄약 차감이 버전 밖이라(위 항목) 서버 잔탄 ≥ 클라이언트 잔탄이 정상인데, 언로드는 그 오차를 **버전이 붙은** 인벤토리 칸으로 세탁한다. 배치는 같고 수량만 갈린 채 양측이 나란히 다음 버전이 되므로 이후 버전 비교로는 영영 드러나지 않는다. `unloaded_ammo_slot`(결과 칸의 최종 상태)이 그 한 곳을 막고, 덤으로 규칙 드리프트 탐지를 겸한다 — 클라이언트가 계산한 칸과 다르면 구현이 갈라진 것이니 전체 재동기화가 정답이다. 재장전이 응답에 전체 스냅샷을 싣는 것도 뿌리가 같다(`LoadMagazineFromInventory()`가 버전 밖 탄창 수량으로 결과를 정한다). 귀결 둘: **발사 시 버전을 올리는 변경이 오면 이 필드는 불필요해진다**, 그리고 규칙이 서버·클라이언트 양쪽에 **각각 구현돼 있다**. 클라이언트 반영이 끝난 뒤 `.proto`에서 규칙 설명을 걷어냈으므로(2026-08-30), 저장소 안에는 규칙이 바뀌었음을 클라이언트 쪽에 알릴 신호가 남아 있지 않다 — **언로드 규칙·계산 순서를 건드리는 변경은 상위 세션에 클라이언트 동반 수정을 요청할 것.** 어긋나면 컴파일도 통신도 정상이고 `unloaded_ammo_slot` 검산에서 전체 재동기화가 반복되는 형태로만 드러난다.

### 게임룸·오브젝트

- **룸 수명은 생성 시각으로부터 10분이고 만료 처리는 전원 사망이다** — `GameRoom::ROOM_LIFETIME_MS`. 기산점이 첫 입장이 아니라 룸 생성이라 클라이언트 로딩 시간이 플레이 시간에서 깎이고, 대신 아무도 접속하지 않은 룸까지 상한이 덮는다. 검사 지점은 `ProcessLeaves()` 맨 위 하나다 — `Update()`에 두지 않은 이유는 그쪽이 "override 하면 마지막에 베이스를 부를 것" 규율에 기대는 자리라 규율이 깨진 파생 룸만 조용히 상한을 잃기 때문이고, 맨 위에 둬야 `AllKill()`이 찍은 마킹이 같은 틱의 이탈 루프에서 소비된다. `AllKill()`은 새 파괴 경로가 아니라 전 세션에 `MarkLeaving(DEAD)`를 찍는 루프이고 뒤는 기존 파이프라인(`DetachPlayer` → 유예 5초 → `FinalizeLeave` → `CheckAllLeft` → `ReserveRoomDestroy`)이 그대로 처리한다 — 만료부터 룸 회수까지 약 6초, 락 해제는 그보다 앞선 `DetachPlayer` 시점이다. 딸린 사실 넷:
  - **`ROOM_LIFETIME_MS < ACTIVE_MATCH_TTL_SEC × 1000`은 프로세스를 가로지르는 불변식이다**(`src/CLAUDE.md`의 락 항목). 상대편 값은 `match.js`·`DBProxyRequest.h`에 있어 `static_assert`로 묶을 수단이 없다.
  - **미접속(INIT) 세션도 몰수 대상이고 의도된 결정이다** — `NotifyPlayerLeftRequest::ApplyInventoryToDb()`가 이탈 사유를 보지 않고 `slot_index >= 80`을 지우는데 DEAD 는 슬롯을 싣지 않으므로, 접속조차 못 한 유저의 인벤토리·장착이 비워진다. 가르려면 사유를 나눠야 하고 그때 `PlayerSession::LeaveReason`과 `IPC_enum.proto`의 `LeaveReason`이 짝이다(`static_cast`로 넘긴다).
  - **만료 사망에는 가해자가 없다** — `killer_object_id`는 `NO_ATTACKER`, 이름은 빈 문자열, `DespawnReason`은 `DESPAWN_DEAD`. 클라이언트가 일반 사망과 구분해야 해지면 통보를 갈라야 한다.
  - **AllKill 은 인당 reliable 3장(전리품 스폰·킬 피드·디스폰)을 한 틱에 낸다** — in-flight 33장 한도에 가장 가까이 가는 경로이니 룸 인원을 늘리거나 룸 자연사 직전에 통보를 더할 때 같이 볼 것. 전원 사망이라 아무도 줍지 못할 전리품 컨테이너를 스폰하는 낭비는 단순함을 위해 감수한다.
- **잔여 수명은 스폰 응답에만 실린다** — `D2CResponseSpawnMeSpawnSpot::remaining_life_ms`. 이후 재동기화는 없고 클라이언트가 받은 값에서 30초를 깎아 자체 카운트한다(서버 마감보다 먼저 끝나게 하는 여유분). 마감을 넘겨 도달한 요청은 0 을 받는다 — `GetRemainingLifetimeMs()`의 클램프가 없으면 무부호 언더플로다.
- **인원 0 인 룸도 회수된다** — `CheckAllLeft()`의 빈 맵 조기 반환을 없앴다. 룸이 `_gameRooms`에 들어가는 것은 세션 등록을 마친 뒤라 정상 경로에 빈 룸이 틱을 도는 구간은 없다.
- **표시명은 `UnityGameObject::GetObjectName()` 하나에서 나오고, 플레이어의 이름은 로그인 userId다** — userId 노출은 은닉 방침의 예외가 아니라 의도된 설계(총격음과 킬 로그가 맞물려 선택지를 만든다) — 별도 닉네임 도입을 제안하지 말 것. 타입당 고정 이름은 베이스의 `ObjectTypeToName(objectType)`(비-플레이어 저장 비용 0B), 인스턴스별 이름만 override(현재 `PlayerObject` 하나). 새 `ObjectType`을 추가하면 `ObjectTypeToName()`의 case가 짝이다 — 빠뜨리면 "None"이 조용히 나간다. 헤더의 이름 상수는 예외 없이 `inline`(네임스페이스 `const`는 internal linkage라 TU마다 사본·주소 불일치). `objectName`을 데이터 멤버로 되돌리려면 비-플레이어당 32B 비용과 포인터 dangling 문제를 먼저 볼 것.
- **플레이어 전리품 컨테이너의 용량에는 여유가 0이다** — 인벤토리 25 + 장착·탄창 5 = `Container::DEFAULT_CONTAINER_VOLUME`(30). 칸을 늘리며 같이 안 늘리면 초과분이 조용히 버려진다 — `PlayerLootContainer.h`의 `static_assert`가 컴파일 타임에 잡으므로 빌드가 깨지면 두 상수를 같이 볼 것.
- **`GameRoom::Update()`를 override하면 마지막에 베이스를 부를 것** — 베이스가 `BroadcastPlayerStates()`를 담당한다. 빠뜨리면 그 룸에서 플레이어가 서로 움직이지 않고, 먼저 부르면 파생 로직의 변화가 한 틱 밀린다. NVI는 인지 비용 판단으로 의도적으로 채택하지 않았다 — 규율로 지킨다.
- **파생 룸의 주기 동작은 전부 같고, 그것이 현재의 의도다** — `TestGameRoom`·`WinchesterGameRoom` 어느 쪽도 `Update()`를 override하지 않는다. 룸마다 다르게 동작시킬 계획은 없으며 `virtual`은 여지로만 남긴 것이다 — 윈체스터에 뭔가 빠졌다고 읽지 말 것. 갈리는 것은 생성자뿐이다(테스트 룸만 `InitTestGameRoom()`으로 `TestItemBox`를 정적 스폰한다).
- **AI는 서버에서 다루지 않기로 했다** — 맵 고유 로직을 설계할 때 선택지에서 뺄 것.
- **`CombatObject`는 `_dynamicObjects`에만 둔다** — 회수 경로인 `DestroyDeadObject()`가 그쪽만 보므로 정적 등록된 전투 오브젝트는 죽어도 회수되지 않는다. 입구는 `CanBeStaticObject()`가 막지만 우회하면 성립한다.
- **컨테이너 점유는 세션과 컨테이너 양쪽에 적히고 둘은 항상 짝이다** — `PlayerSession::_interactingContainerId` ↔ `Container::_interactingPlayerId`(플레이어 **objectId**. 세션 id는 FIFO 재사용이라 못 쓴다). 짝을 유지하는 자리는 넷뿐이다 — 열기(앞선 점유를 풀고 새로 잡는다), 닫기와 이탈(`GameRoom::ReleaseInteractingContainer()`), 소유권 이전(옛 점유자의 세션도 함께 되돌린다). **다섯 번째 자리를 만들지 말 것** — 짝이 어긋나면 두 명이 같은 컨테이너를 동시에 조작한다. 조작·장착 핸들러가 소유권을 다시 검사하지 않는 근거가 이 불변식이다.
- **열기 핸들러에서 앞선 점유의 해제는 반드시 모든 검증 뒤에 온다** — 위로 올리면 열기에 실패한 요청(범위 밖·남이 점유 중 등)이 **열려 있던 컨테이너까지 잃게 만든다.** 순서는 검증 → `ReleaseInteractingContainer()` → 새 점유 설정이다. 이 순서 덕에 같은 컨테이너 재열기가 스냅샷 복구 경로가 되기도 한다.
- **점유 락의 해제는 거리 검사가 대신한다 — 단, 좌표가 갱신되는 동안만이다** — 열기 요청이 왔을 때 기존 점유자가 상호작용 범위 밖이면(오브젝트가 사라진 경우 포함) 소유권을 가져온다. 해제를 `C2DCloseContainer` 하나에만 의존하면 그 패킷의 유실·순서 역전으로 컨테이너가 매치 끝까지 잠기는데, 이 이전 규칙이 그 교착을 없앤다 — **점유 조건을 바꿀 때 이 성질을 깨지 말 것.** 하트비트만 돌고 상태 갱신이 멈춘 클라이언트는 끊김 판정도 안 나고 좌표도 굳어 거리 기반 이전이 성립하지 않지만, **점유에 시간 상한은 두지 않기로 확정했다** — 연결이 끊기면 이탈 처리가 점유를 풀고, 살아서 굳어 있는 쪽은 굳은 좌표 그대로 사살돼 같은 경로로 풀린다. **클라이언트 쪽 귀결**: 컨테이너 UI가 열려 있다고 점유가 유지된다는 보장이 없다 — 거리 밖에 나가 있는 동안 넘어갔을 수 있고, 그 사실은 조작이 `DENY_CONTAINER_NOT_OPEN`으로 거부될 때에야 드러난다(그때 이 비트의 가장 흔한 원인이 이것이다).
- **컨테이너 거리 검사는 세 자리에 있고 실패 방식이 갈린다** — `GameRoom::IsPlayerNearContainer()`를 열기(`Handle_C2D_RequestOpenContainer`)·조작(`Handle_C2D_RequestInteractContainerObject`)·장착(`Handle_C2D_RequestEquipItem`)에서 부른다. 열기 실패는 조용하고(그 핸들러의 규약), 나머지 둘은 `DENY_OUT_OF_RANGE`(0x0800)를 돌려준다. 조작마다 검사하므로 열어둔 채 멀어지면 그 시점부터 거부된다. **`CONTAINER_INTERACT_RANGE_SQ`(3m)는 클라이언트의 상호작용 표시 거리(2m)와 짝이고 1m가 여유분이다** — 서버 쪽이 좁아지면 열 수 있다고 보여준 컨테이너가 조용히 안 열린다. 이 검사는 안티치트가 아니다(좌표가 클라이언트 주장값이다).
- **파괴 가능하면서 컨테이너인 오브젝트는 만들지 않는다** — 만들면 약탈 중이던 세션의 `_interactingContainerId`가 사라진 오브젝트를 가리킨다. 필요해지면 제거 시 해당 id를 보는 세션을 찾아 `-1`로 되돌리는 처리가 함께 와야 한다.
- **플레이어 전리품 컨테이너에는 TTL이 없다** — 룸 소멸까지 유지되고 `~GameRoom()`이 회수한다. 만료를 붙이면 둘이 따라온다: ① 약탈 중 세션의 `_interactingContainerId` 되돌리기 ② 잔여 콜백의 objectId 재조회 규약(세션의 sessionId 재조회와 같은 형태).

### 귀환

- **귀환의 설계 의도 셋은 미비점이 아니다** — ① 자발적 취소 패킷이 없다 — 존 이탈이 유일한 취소이고 재요청은 "진행 중"으로 무시된다. 필요해지면 별도 패킷으로 추가 ② 존 검사는 `RecallTick`(1초 간격)에서만 한다 — 매 상태 갱신마다 검사하면 소수를 위해 룸 전체가 비용을 무는 구조가 되므로 틱 사이의 이탈·복귀는 허용 ③ `RECALL_RESULT_PLAYER_DEAD` 경로는 현재 도달 불가지만 남겨둔다 — 멀티스레드 환경에서는 발생 여지가 있다.
- **귀환 유효성 검사는 안티치트가 아니다** — 검사에 쓰는 `PlayerObject::position`은 클라이언트가 보낸 좌표 그대로라 좌표 조작 클라이언트는 항상 통과한다. 실질 차단에는 서버 측 이동 검증(속도/텔레포트 체크)이 별도로 필요하다.

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
| 맵 데이터 (귀환 영역 등 맵별 정적 테이블, 정적 클래스) | `MapDataManager.h` |
| Unity 게임 오브젝트 (베이스) | `UnityGameObjects/UnityGameObject.h/cpp` |
| 전투 오브젝트 (HP·Shield·TakeDamage·사망 판정 공통 베이스) | `UnityGameObjects/CombatObject.h` |
| Unity 플레이어 오브젝트 | `UnityGameObjects/PlayerObject.h/cpp` |
| Unity 게임 오브젝트 (구체 타입) | `UnityGameObjects/TestGameObjects.h/cpp` |
| Unity 컨테이너 오브젝트 | `UnityGameObjects/Container.h/cpp` |
| 플레이어 전리품 컨테이너 (사망 시 인벤토리·장착·탄창을 옮겨 담는 Container 파생) | `UnityGameObjects/PlayerLootContainer.h` |
| UDP 태스크 | `UDPTask.h/cpp` |
| 열거형 | `enum.h` |
| 외부 패킷 프로토콜 (컴파일 결과) | `ExternalProtocol/` |
