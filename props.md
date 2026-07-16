# Props — 상위 세션에서 전달된 컨텍스트

## 총알 발사 시스템 설계 (2026-07-16)

### 작업 의도

플레이어의 총알 발사 → 서버 검증 → 다른 플레이어에게 브로드캐스트하는 전체 파이프라인 구축.
루트 세션에서 proto 정의 + 양쪽 핸들러 뼈대까지 완료. 하위 세션에서 TODO로 남긴 세부 구현을 마무리할 것.

### 추가된 프로토콜

`Protocol/ExternalProtocol/External_Protocol.proto`에 다음 패킷이 추가됨:

- `C2D_REQUEST_WEAPON_FIRE` (PktId = 29)
- `D2C_BROADCAST_WEAPON_FIRE` (PktId = 30)

### 패킷 흐름

```
Client → C2D_REQUEST_WEAPON_FIRE → Server (DedicateProcess)
  - fire_sequence: 발사 시퀀스 (중복/리플레이 방지)
  - weapon_dbid: 장착 총기 blueprint_id
  - hit_point: Raycast 피격 좌표 (하늘에 쏜 경우 미설정, proto3 message 필드 null)
  - hit_object_id: 피격 대상 object_id (없으면 0xFFFFFFFF)

Server → D2C_BROADCAST_WEAPON_FIRE → 발사자를 제외한 다른 모든 플레이어
  - shooter_object_id: 발사자 object_id
  - hit_point: 탄착 좌표 (없으면 미설정)
```

### 이미 작업된 뼈대 (루트 세션에서 완료)

| 파일 | 변경 내용 |
|------|-----------|
| `Protocol/ExternalProtocol/External_Protocol.proto` | PktId enum 추가 + C2DRequestWeaponFire, D2CBroadcastWeaponFire 메시지 정의 |
| `src/DedicateProcess/enum.h` | PKT_ID_C2D_REQUEST_WEAPON_FIRE(29), PKT_ID_D2C_BROADCAST_WEAPON_FIRE(30), PKT_ID_MAX=31 |
| `src/DedicateProcess/Player.h` | `IncrementFireSequence()` 메서드 추가 |
| `src/DedicateProcess/PlayerSession.h` | `GetFireSequence()`, `IncrementFireSequence()` 위임 메서드 추가 |
| `src/DedicateProcess/GameRoom.h` | `GetPlayerSessions()` const 접근자 추가 |
| `src/DedicateProcess/ClientPacketHandler.h` | 핸들러 선언, Init() 등록, `MakeD2CBroadcastWeaponFireUnreliable()` 송신 헬퍼 |
| `src/DedicateProcess/ClientPacketHandler.cpp` | `Handle_C2D_RequestWeaponFire()` 뼈대 구현 |

### 서버 측 TODO (세부 구현)

`Handle_C2D_RequestWeaponFire()` 내부에 TODO로 표시된 항목들:

1. **weapon_dbid 검증**: 플레이어가 실제 장착 중인 총기의 blueprintId와 pkt.weapon_dbid() 비교. 현재 `PlayerObject`에 `GetCurrentWeaponId()`가 있음
2. **탄약 차감**: 서버 측 inventory의 magazine에서 1발 차감. 탄약 0이면 발사 거부(return false). `PlayerSession` → `GetInventoryMutable()` → magazine 슬롯 접근
3. **피격 처리**: hit_object_id가 유효한 플레이어인 경우 총기 스펙 기반 데미지 처리. HP 시스템이 아직 미구현이므로, 이 부분은 HP 구현 후 진행

### 의도적으로 하지 않는 것

- **거리/각도 검증**: 서버 자원 제약으로 생략. 대상 존재 검증만 수행
- **데미지 결과 패킷**: HP 시스템 미구현 상태. 추후 윤곽이 잡힌 후 별도 proto 정의 예정
- **reliable 전송**: 브로드캐스트는 unreliable로 전송. 총알 발사는 빈도가 높고 유실되어도 치명적이지 않음

### 탄약 동기화 정책

- 장전 시점: 엄격하게 수량 동기화
- 발사 중: 느슨한 동기화 허용 (클라이언트-서버 간 일시적 불일치 가능)
- 보장: 서버가 매 발사마다 차감하므로 장전된 수 이상 발사 불가
