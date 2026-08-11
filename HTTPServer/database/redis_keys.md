# Redis Key Structures

| Key Pattern | Type | TTL (수명) | Description | Example Value / Fields |
| :--- | :--- | :--- | :--- | :--- |
| `item_meta` | **Hash** | 영구(None) | 아이템 마스터 정보 | `item_name: "돌", item_type: "resource", description: "그냥 돌"` (이름, 분류, 설명) |
| `sess:<UUID>` | **Hash** | 1시간 (3600s) | 클라이언트 인증용 세션 | `user_id: "tetepiti149", db_id: "13", user_type: "1", rating:"1500", aggression: "4"` (유저 ID) |
| `user_sess:<ID>` | **String** | 1시간 (3600s) | 중복 로그인 방지용 | `"sess_1234abcd..."` (세션 UUID) |
| `active_match:<db_id>` | **String** | 5분 (300s) — **TEMP**, 아래 2번 참조 | 유저 중복 매칭 방지 락. 값은 해당 유저의 ticketId | `"ticket_xxxx"` |
| `ticket_<UUID>` | **Hash** | 5분 (300s) | 매치메이킹 대기열 티켓 및 상태 | 1. 매칭 티켓 참조 |
| `token_<UUID>` | **Hash** | 5분 (300s) | 인게임(UDP) 세션 인증용 세션 | `udp_server_ip: "xxx.xxx.xxx.xxx", port: "xxxx", security_key: "2^32미만의 숫자", fd: "xx", session_id: "xx", ticket: "ticket_xxxxx", loadout_type: "FREE" or "CUSTOM"` (ip, port, 인증키, 해당 token을 관리하는 프로세스 식별자, 프로세스 안에서의 session 식별자, 이 token에 해당하는 ticket (삭제 cascade구현용), 로드아웃 타입 (CUSTOM일 경우 /connect 시 인벤토리·장비 DB 삭제)) |

1. 매칭 티켓

    [초기 상태: /start 진입 시]

    - uid: "13" (데이터베이스 PK)
    - user_id: "tetepiti149" (로그인 ID)
    - rating: "1500" (매치메이킹 점수)
    - aggression: "4" (유저 성향)
    - map_id: "0" (선택한 맵 ID)
    - character_type: "0" (선택한 캐릭터 id)
    - loadout_type: "FREE" OR "CUSTOM" (프리로드아웃인지 아닌지)
    - inventory_items: '[{"itemId": 101, "quantity": 5, "inventorySlotId": 0}]' (소지 아이템 JSON 문자열)
    - equipment_items: '[{"itemId": 101, "equipmentSlotId": 0 }]' (장착 아이템 JSON 문자열)지
    - status: "WAITING"

    [매칭 완료 ~ 서버 준비중 : `MatchMaker::VerifyAndSetMatchStatus()` (`src/Matchmaker.cpp`) 의 인라인 Lua 가 그룹 전원을 한 번에 업데이트]

    - status: "INPROGRESS" (업데이트됨, 지금부터 플레이어의 매치 취소(/cancel)로 파기 불가능)

    [서버 준비 완 : `UpdateEntryTokenRequest::Execute()` (`src/RedisProxyRequest.cpp`) 가 ticket 과 token 을 함께 기록]

    - status: "SUCCESS" (업데이트됨)
    - token: "token_asdf1234asdf5678"

2. `active_match:<db_id>` 의 TTL

    이 락은 본래 **"진행 중인 게임이 있음"** 을 뜻하며, 플레이어의 사망 / 탈출(귀환) / 연결 끊김이
    확정될 때 해제되어야 한다.

    그 종료 처리가 아직 미구현이라, 같은 계정으로 재실험이 가능하도록 두 가지 임시 해제 경로를 두었다.

    - TTL 300초 자동 만료 (`POST /match/start`)
    - 만료된 티켓에 대한 `POST /match/cancel` 요청으로 강제 해제 (`matchCancel` Lua 의 `return 2`)

    TTL 이 게임 길이보다 짧으므로 **게임 도중에 락이 풀려 새 매칭을 걸 수 있다.** 이를 감수한 상태다.

    해제 조건 : 사망 처리 · 귀환 확정 처리 · `DisconnectSession` 완성 시,
    TTL 과 `return 2` 분기를 걷어내고 게임 종료 시점의 명시적 DEL 로 교체할 것.
