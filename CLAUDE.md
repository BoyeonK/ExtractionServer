# CLAUDE.md

## 세션 경계

- 이 세션의 작업 범위는 서버 코드베이스로 한정한다.
- 클라이언트 디렉터리(`Extraction/Extraction/`)의 코드는 읽지 않는다.
  수정뿐 아니라 열람도 하지 않는다. 컨텍스트 오염을 막기 위함이다.
- 클라이언트 측 원인으로 보이는 문제는 이 세션에서 해결하지 않는다.
  원인이 클라이언트에 있다고 판단되면 그렇게 보고하고 멈춘다.
- 클라이언트 코드와의 비교·대조가 필요하다고 판단되면, 직접 열지 말고
  상위 세션에 해당 작업을 요청할지 사용자에게 물어본다.

## 프로젝트 개요

실시간 멀티플레이어 게임 서버. C++ 메인 프로세스, Node.js HTTP 서버, C++ 전용 게임 프로세스(Dedicate)가 IPC 소켓으로 통신하는 멀티 프로세스 아키텍처.

```
Client
  ├─ HTTPS → Node.js (HTTPServer/) — 인증, 매치메이킹 REST API
  └─ UDP   → C++ DedicateProcess — 게임 로직 (이동, 전투 등)

Node.js ──IPC──▶ Main C++ Process (src/)
                  ├─ io_uring 기반 비동기 I/O
                  ├─ DB Proxy (자식 프로세스 대신 Redis·MySQL 처리)
                  └─ DediManager (전용 게임 프로세스 생성/관리)

Main C++ ──IPC──▶ DedicateProcess (src/DedicateProcess/)
                  └─ UDP 게임 세션 관리
```

## 디렉터리별 상세 문서

| 디렉터리 | CLAUDE.md | 내용 |
|-----------|-----------|------|
| Main C++ 프로세스 | `src/CLAUDE.md` | IPC 라우팅, io_uring, DB 프록시, DediManager |
| Dedicate 프로세스 | `src/DedicateProcess/CLAUDE.md` | UDP 패킷 형식, 게임룸, 플레이어, 아이템 |
| Node.js HTTP 서버 | `HTTPServer/CLAUDE.md` | 인증, REST API, 인벤토리 슬롯 구조 |

## 환경 변수

`HTTPServer/.env` 파일에 Redis, MySQL 연결 정보, 서버 포트, 로컬 테스트 여부(`IS_LOCAL_TEST`) 등이 정의된다.

## 프로토콜 참조

- **IPC 프로토콜**: `Protocol/IPCProtocol/` (IPC_HTTP.proto, IPC_Dedicate.proto, IPC_enum.proto)
- **외부 패킷 프로토콜**: `Protocol/ExternalProtocol/` (External_Protocol.proto, External_Unity_Object.proto)
- **Redis 키 구조**: `HTTPServer/database/redis_keys.md`
- **MySQL 스키마**: `HTTPServer/database/schema.sql`

## 진행 상황 추적

`progress.md` (루트)에서 완료된 작업, 진행 중인 작업, 다음 할 일을 관리한다. 「진행 고려사항」은 **미결·보류·검증 대기 항목 전용**이다 — 확정된 불변식·규약·결정 배경은 해당 디렉터리의 CLAUDE.md에 적는다.

## 코드·문서 규율

- **`TODO:` / `TEMP:` / `OPTION:` 마커** — `TODO:`는 미구현, `TEMP:`는 테스트를 위한 임시 제한·차단(릴리스 전 되돌릴 것), `OPTION:`은 없어도 동작에 문제없는 개선(할 일 목록에 올리지 않고 주석으로만 남긴다). 릴리스 전 `grep -rn "TEMP" src HTTPServer`로 전수 확인한다. 코드를 주석으로 막을 때는 설명 문구보다 **마커를 먼저** 붙일 것 — 마커 없는 임시 코드는 이 전수 확인에서 빠진다.
- **코드 주석은 세 마커와 상수 유래뿐** — 허용은 TODO/TEMP/OPTION, 숫자만 남는 상수의 유래(슬롯 범위·페이로드 한계·프로토콜 반환 코드 등), 바이트 레이아웃. 흐름 서술("~를 전송한다"), 단계 나열, 구획 배너, 설계 배경은 코드에 쓰지 않는다. 코드만 봐서 알 수 없는 제약·불변식·결정 배경은 **해당 디렉터리의 CLAUDE.md**에 적는다. 예외는 `src/sample.h` 하나(학습 노트 — 정리한다면 파일째 삭제가 맞다).
- **통보의 대상·시점·보장 범위를 바꾸면 `.proto`의 `[작업사항]` 주석을 함께 훑을 것** — 이 주석은 서버 빌드에 영향이 없어 틀려도 아무것도 깨지지 않고, 깨지는 자리가 클라이언트 세션이라 발견이 늦다. 구현 diff에 `.proto`가 없으면 주석을 안 봤다는 뜻으로 읽는다. `[작업사항]` 블록을 지울 때는 그 안의 영속 계약(에코 규약 등)을 해당 CLAUDE.md로 **먼저 옮긴 뒤** 지운다.

## 데이터 원천 규율

- **스키마의 출처는 루트 `database/`** — 나머지 디렉터리와 별개로 관리하기로 확정됐고, `HTTPServer/database/schema.sql`과의 중복은 감수한다. **통합·삭제를 제안하지 말 것.** 값이 갈리면 실 DB에서 뽑은 `database/current_schema.sql`이 언제나 사실이다. **시드 데이터를 마이그레이션에 넣지 않는다**(공개 저장소에 실 DB 내용을 노출하지 않기 위함) — 제안하지 말 것. 따라서 빈 DB에 마이그레이션만 돌리면 환경이 완성되지 않는다(`items`·`weapon_specs`가 비면 아이템 캐시·샵·로드아웃 조회·수치 마이그레이션이 조용히 깨진다). 새 환경은 실 DB 덤프로 채운다 — 저장소 밖의 절차다.
- **아이템 수치의 출처는 DB이고 두 헤더는 생성 산출물이다** — `database/generate_script.py`(`.gitignore` 대상. 저장소 밖에서 받아와 접속 정보를 채워야 실행된다)가 `items`·`weapon_specs`·`armor_specs`에서 `database/ItemDataManager.h`(C++)·`database/ItemDBHelper.cs`(C#)를 생성하고, CMake `SyncRuntimeFiles`가 빌드마다 서버 트리로 복사한다. **수치 변경 순서: 마이그레이션 → `generate_script.py` → 빌드.** 어느 헤더도 직접 고치지 말 것(다음 빌드·생성에 덮인다). **기획 데이터 마이그레이션의 down에는 역방향 SQL을 두지 않는다** — 수치 사본이 하나 더 늘어난다. 롤백은 새 마이그레이션으로 하고 down에는 취지만 주석으로 남긴다. 자동화 밖 구간 셋: ① 스크립트 실행은 수동이라 마이그레이션만 돌리면 헤더가 낡는다 ② `ItemDBHelper.cs`의 클라이언트 반입 경로가 이 저장소에 없다 ③ 최대 HP(`PlayerObject::DEFAULT_MAX_HP` ↔ 클라이언트) 짝은 생성 경로 밖이라 손으로 맞춘다. 어긋나도 컴파일·통신은 전부 정상이고 표시만 조용히 틀어진다. 전투 수치는 전부 100배 스케일이며 그 규칙은 DB 값이 들고 있다.

## 개발 참고 사항

- **외부 라이브러리**: `myUtils` (BoyeonK/myUtils 깃 서브모듈), `abseil-cpp` (FetchContent)
- **third_party/**: 경량 의존성 소스 직접 포함 — `xxhash/` (xxhash.h, xxhash.c), `nlohmann/` (json.hpp)
- **배포**: Oracle Compute + MySQL HeatWave, Cloudflare 프론트엔드
- **언어**: 주석/변수명은 한국어 혼용
