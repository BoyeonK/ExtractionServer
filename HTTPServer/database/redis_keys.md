# Redis Key Structures

| Key Pattern | Type | TTL (수명) | Description | Example Value / Fields |
| :--- | :--- | :--- | :--- | :--- |
| `item_meta` | **Hash** | 영구(None) | 아이템 마스터 정보 | `item_name: "돌", item_type: "resource", description: "그냥 돌"` (이름, 분류, 설명) |
| `sess:<UUID>` | **Hash** | 1시간 (3600s) | 클라이언트 인증용 세션 | `user_id: "tetepiti149", db_id: "13", user_type: "1", rating:"1500", aggression: "4"` (유저 ID) |
| `user_sess:<ID>` | **String** | 1시간 (3600s) | 중복 로그인 방지용 | `"sess_1234abcd..."` (세션 UUID) |
| `active_match:<db_id>` | **String** | 1시간 (3600s) — 백스톱, 아래 2번 참조 | 유저 중복 매칭 방지 락. 값은 ticketId, 게임 시작 후에는 `INGAME:<ticketId>` | `"ticket_xxxx"` / `"INGAME:ticket_xxxx"` |
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

2. `active_match:<db_id>` 의 해제

    이 락은 **"진행 중인 게임이 있음"** 을 뜻한다. 해제 경로는 셋이다.

    | 경로 | 시점 | 주체 |
    | :--- | :--- | :--- |
    | 정상 해제 | 플레이어의 룸 분리 (귀환 / 사망 / 연결 끊김) | `GameRoom::DetachPlayer()` → `D2MNotifyPlayerLeft` → `NotifyPlayerLeftRequest::Execute()` |
    | 대기 취소 | WAITING 티켓을 유저가 직접 취소 | `POST /match/cancel` (`matchCancel` Lua 의 `return 1`) |
    | 만료 정리 | 매칭이 성사되지 않고 티켓만 만료된 뒤의 `/cancel` | `matchCancel` Lua 의 `return 2` |

    유저 단위 락이므로 개인의 결과가 확정되는 시점에 푸는 것이 맞다. 룸 전체가 끝나기를 기다리지 않고,
    사망 유예(세션이 5초 더 살아 있는 구간)가 끝나기도 기다리지 않는다.

    TTL 3600초는 **백스톱**이다 — 이탈 통보가 유실되는 경우(데디 크래시, IPC 유실)에 락이
    영구 잔류하는 것을 막는다. 최대 게임 길이보다 길어야 하며, 짧으면 게임 도중에 락이 풀린다.

    티켓 TTL(300초)이 락 TTL보다 짧아 **"티켓은 없는데 락은 남은" 구간**이 생긴다. 이 구간은
    ① 매칭 실패 후 방치 ② 게임이 300초를 넘겨 진행 중, 두 경우로 갈리지만 티켓이 없어 상태로는
    구분할 수 없다. 그래서 `UpdateEntryTokenRequest::Execute()` 가 게임 시작 시점에 락 값을
    `INGAME:<ticketId>` 로 덮어쓰고, `matchCancel` 이 그 접두어를 보고 ②를 거부한다(`return 3`).

    DB 반영(MySQL)과 락 해제(Redis)는 공유 트랜잭션이 없다. **MySQL 먼저, 락 해제 나중** 순서를
    지킨다 — 락을 먼저 풀면 반영 실패 시 유저가 반영 안 된 인벤토리로 새 매치를 시작할 수 있다.
    DB 반영이 최종 실패하면 락은 풀고 페이로드를 error 로그로 남긴다 (레이드 하나 유실이
    영구 잠금보다 낫다는 판단).
