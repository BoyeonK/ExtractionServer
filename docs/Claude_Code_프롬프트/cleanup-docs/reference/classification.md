# 분류 기준

## 의미 단위로 보기

Gate는 문서 전체나 문장 하나에 적용하지 않는다. 적용 단위는 **하나의 contract, invariant, decision, rationale처럼 독립적으로 의미를 갖는 정보 단위**다.

예를 들어 다음은 네 문장이지만 하나의 의미 단위다.

```
Chunk는 global pool로 반환한다.
반환 thread는 allocation thread와 다를 수 있다.
TLS cache를 사용하지 않는다.
자세한 선택 배경은 ADR-0004를 참고한다.
```

→ `Chunk 반환 정책과 그 제약` 하나로 본다.

반대로 한 문단에 서로 다른 역할이 섞여 있으면 먼저 분해한다.

```
현재 규칙        → Design 또는 Comment
결정 이유        → ADR
기각된 대안      → ADR
미결정 문제      → Open Questions
결정된 남은 작업 → Issue Tracker / Progress Log
```

---

## 목적지 매핑

```
repo-wide 작업 방법             → Bootstrap
현재 cross-file 구조            → Design
결정의 이유                     → ADR
local contract / invariant      → 코드 주석
cross-file contract / invariant → Design
미결정 설계                     → Open Questions
결정된 작업의 진행 상태         → Issue Tracker / Progress Log

과거 기록 (Historical Note)
  → 중요한 결정의 근거라면 ADR
  → 현재 이해나 향후 판단에 필요하지 않다면 Gate 적용 후 삭제/축약
```

---

## Bootstrap (`CLAUDE.md`, `AGENTS.md`)

역할: **이 저장소에서 어떻게 작업하고, 필요한 정보를 어디서 찾는가**

**남긴다**
- 이 repo가 무엇인가
- 주요 디렉터리와 역할
- build / test / lint 방법
- repo 전체 작업 원칙
- 변경 전후 검증 방법
- 다른 문서의 위치와 역할

**덜어낸다**
- 특정 상태 머신, 특정 mutex의 lock 순서
- 특정 allocator의 lifetime, 특정 scheduler 정책
- tuning parameter
- 특정 설계 선택의 긴 rationale

**판단 질문**: 이 컴포넌트를 전혀 수정하지 않는 작업에서도 이 정보를 처음부터 읽을 가치가 있는가? NO면 덜어낼 후보.

---

## Design

역할: **현재 시스템이 어떻게 동작하는가** — living document. 역사 기록이 아니라 현재형.

**남긴다** (파일 하나만 읽어서는 알 수 없는 현재 관계)
- 여러 객체 간 ownership
- cross-thread protocol
- lifecycle / shutdown sequence
- 상태 전이
- subsystem을 관통하는 data flow
- 여러 lock이 참여하는 synchronization
- 컴포넌트의 externally visible semantics
- 중요한 cross-file invariant
- 관련 테스트, 관련 ADR reference

**덜어낸다**
- 과거 구조에 대한 긴 설명
- 기각된 대안
- 당시 선택의 상세 rationale
- 완료된 TODO
- 현재 개발 진행 상황

**갱신 원칙**: 시스템이 바뀌면 기존 내용을 수정한다. 과거 버전을 문서 안에 누적하지 않는다.

---

## ADR

역할: **왜 중요한 결정을 그렇게 내렸는가** — 결정 당시의 Context와 판단을 보존.

**남길 가치가 높은 조건**
- 몇 달 뒤 이 결정을 다시 의심하거나 뒤집으려 할 가능성이 있는가?
- 그때 당시 이유를 알아야 같은 논의를 반복하지 않는가?

**대표 예**: ownership/lifetime 정책, DB/storage 선택, networking protocol, concurrency model, process architecture, authentication 방식, 중요한 public API contract

**다루지 않아도 되는 것**: 함수명, 작은 helper 구조, 단순한 코드 정리, 쉽게 되돌릴 수 있는 구현 세부, 단순 tuning 값

**기본 형태**
```
Status
Date

Context
Decision
Considered Alternatives
Consequences
Revisit Conditions (필요한 경우)
```

**정리 원칙**: Accepted ADR은 과거 기록이므로 현재 상태에 맞춰 본문을 고쳐 쓰지 않는다. 결정이 달라졌으면 새 ADR을 만들고 기존 것을 `Superseded` 처리한다. 많아졌다는 이유로 서로 다른 결정을 억지로 합치지 않는다.

---

## Design과 ADR의 경계

다음 형태가 자주 나온다.

> X는 반드시 Y여야 한다. 왜냐하면 Z이기 때문이다.

앞은 현재 규칙, 뒤는 결정 이유다. **한쪽에만 넣을 필요는 없다.**

- Design: 현재 규칙 + 그 규칙을 이해하는 데 필요한 짧은 이유
- ADR: 상세한 Context, 대안, trade-off

```
[Design]
Chunk ownership은 반드시 ChunkPool을 통해 이전한다.
worker 간 직접 전달은 금지한다.

이 규칙은 cross-thread ownership과 lifetime을 안전하게
유지하기 위한 것이다.
Detailed rationale: ADR-0004.
```

일부 중복은 허용한다. **Design은 현재 규칙을 자기완결적으로 설명하고, ADR은 결정 과정과 깊은 rationale을 보존한다.**

---

## Open Questions

역할: **아직 무엇을 할지 결정되지 않은 문제만** 유지.

**판단 질문**: 지금 당장 구현을 시작할 수 있을 정도로 방향이 결정되어 있는가?

- **NO** → Open Question으로 유지
- **YES** → Open Questions에서 제거하고, 성격에 따라 분산한다
  - 결정된 현재 규칙 → Design
  - 그 결정의 이유와 대안 → ADR
  - 남은 구현 작업 → Issue Tracker / Progress Log

예: "ownership을 Registry가 가질 것인가?"가 Registry ownership으로 결정됐다면, 규칙은 Design에, 왜 그렇게 정했는지는 ADR에, 아직 안 끝난 구현은 Issue Tracker에 간다.

**제거할 것**: 이미 해결된 질문, 결론이 ADR에 기록된 질문, 구현 완료된 질문, 더 이상 유효하지 않은 질문

과거 논의의 로그로 사용하지 않는다.

---

## Work Status / Progress

다음은 설계 문서가 아니라 작업 관리 정보다.

- 구현하기로 결정했지만 끝나지 않은 작업
- 진행 중인 작업, 후속 작업
- 일정, 담당자, 우선순위, 완료 여부

관리 위치: Issue Tracker, Project Board, `progress.md`, `TODO.md` 등 (프로젝트별로 다름)

**핵심**: 무엇을 할지가 결정된 뒤의 진행 상태를 Design이나 Open Questions에 남기지 않는다.

---

## 코드 주석

역할: **이 코드를 안전하게 읽거나 수정하기 위해 바로 이 위치에서 알아야 하는 정보**

핵심 원칙: **Local correctness는 local context에서 발견 가능해야 한다.** ADR이나 Design을 읽어야만 중요한 local invariant를 알 수 있는 상태는 피한다.

> **실행 범위**: 주석을 옮기거나 보강하기 위해 저장소 전체를 스캔하지 않는다. 문서에서 local invariant를 발견해 코드 가까이 옮겨야 할 때, 그 구현 범위만 읽는다. 주석은 수정하되 동작하는 코드는 건드리지 않는다.

**남길 가치가 높은 것**

API Contract
```cpp
// true는 메시지가 현재 시점에 accepted되었다는 의미다.
// 실제 처리를 보장하지 않는다.
bool Send(MessagePtr message);
```

Local Invariant
```cpp
// _offset은 current owner만 수정한다.
// 따라서 intentionally non-atomic이다.
size_t _offset;
```

Non-obvious Why
```cpp
// IDLE 전환 후 반드시 다시 확인한다.
// 마지막 drain 이후 producer가 enqueue했을 수 있다.
RecheckMailbox();
```

**줄일 후보**: "처음에는 A를 검토했고, B도 검토했고, C는 이런 문제로 버렸고..." 같은 긴 역사 설명 → ADR로 이동 검토

코드에는 `현재 지켜야 할 규칙 + 필요한 이유 + 더 깊은 문맥의 reference`를 남긴다.

단, `// See ADR-0004.`처럼 **현재 규칙 자체를 외부 문서에 숨기지 않는다.**

---

## 유지 여부 Gate

의미 단위를 계속 유지할지 애매하면 묻는다.

> **이 설명이 없다면, 다음 세션의 AI Agent나 다른 개발자가 현재 코드만 보고 자연스럽게 수정했을 때 중요한 contract 또는 invariant를 깨뜨릴 가능성이 높은가?**

- YES → 유지하거나(`KEEP`) 적절한 위치로 옮긴다(`RELOCATE`)
- NO → 삭제 또는 축약(`REMOVE / REDUCE`) — 옮길 목적지를 찾지 않는다

**이 기준은 의도적으로 엄격하게 사용한다.** 다음 정도의 이유만으로는 핵심 문서에 유지하지 않는다.

- 있으면 조금 더 이해하기 쉽다
- 언젠가 누군가 궁금해할 수 있다
- 모든 구현 선택에는 나름의 이유가 있다
- 설명이 있으면 오해 가능성이 조금 줄어든다

다만 중요한 결정의 역사를 장기 보존할 가치가 있다면 이 Gate와 별개로 ADR 기준을 적용한다.

---

## 중복에 대하여

중복 제거 자체를 목표로 하지 않는다. 필요하면 일부 정보를 반복해서라도 **각 문서가 자신의 역할 안에서 충분히 이해될 수 있게** 한다.
