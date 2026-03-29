# Redis Key Structures

| Key Pattern | Type | TTL (수명) | Description | Example Value / Fields |
| :--- | :--- | :--- | :--- | :--- |
| `item_meta` | **Hash** | 영구(None) | 아이템 마스터 정보 | `item_name: "돌", item_type: "resource", description: "그냥 돌"` (이름, 분류, 설명) |
| `sess:<UUID>` | **Hash** | 1시간 (3600s) | 클라이언트 인증용 세션 | `user_id: "tetepiti149", db_id: "13", user_type: "1", rating:"1500", aggression: "4"` (유저 ID) |
| `user_sess:<ID>` | **String** | 1시간 (3600s) | 중복 로그인 방지용 | `"sess_1234abcd..."` (세션 UUID) |
| `active_match:<db_id>` | **String** | 5분 (300s) | 유저 중복 매칭 방지 락. 값은 해당 유저의 ticketId | `"ticket_xxxx"` |
| `ticket_<UUID>` | **Hash** | 5분 (300s) | 매치메이킹 대기열 티켓 및 상태 | 1. 매칭 티켓 참조 |
| `token_<UUID>` | **Hash** | 5분 (300s) | 인게임(UDP) 세션 인증용 세션 | `udp_server_ip: "xxx.xxx.xxx.xxx", port: "xxxx", security_key: "2^32미만의 숫자", fd: "xx", session_id: "xx"` (ip, port, 인증키, 해당 token을 관리하는 프로세스 식별자, 프로세스 안에서의 session 식별자) |

1. 매칭 티켓

    [초기 상태: /start 진입 시]

    - uid: "13" (데이터베이스 PK)
    - user_id: "tetepiti149" (로그인 ID)
    - rating: "1500" (매치메이킹 점수)
    - aggression: "4" (유저 성향)
    - map_id: "0" (선택한 맵 ID)
    - loadout_type: "FREE" OR "CUSTOM" (프리로드아웃인지 아닌지)
    - items: '[{"itemId": 101, "quantity": 5}]' (장착 아이템 JSON 문자열)
    - status: "WAITING"

    [매칭 완료 ~ 서버 준비중 : C++ 서버가 matchSuccess Lua 실행 시 업데이트 됨.]

    - status: "INPROGRESS" (업데이트됨, 지금부터 플레이어의 매치 취소(/cancel)로 파기 불가능)

    [서버 준비 완 : Dedicate process가 Success Lua 실행 시 추가됨]

    - status: "SUCCESS" (업데이트됨)
    - token: "token_asdf1234asdf5678"
