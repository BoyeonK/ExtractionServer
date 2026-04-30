#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <tuple>
#include <vector>

class TimerExecuter {
    struct TimerEntry {
        std::chrono::steady_clock::time_point execAt;
        std::function<void()> callback;
        bool operator>(const TimerEntry& o) const { return execAt > o.execAt; }
    };

    std::priority_queue<TimerEntry,
                        std::vector<TimerEntry>,
                        std::greater<TimerEntry>> _queue;
public:
    // ① 람다 등록
    void Add(std::chrono::steady_clock::time_point execAt,
             std::function<void()> func);

    // ② 멤버함수 + Alive Token 등록
    // 등록 시점에 유효한 obj가 실행 시점에 소멸되어 있으면 콜백을 스킵한다.
    // 사용 측 클래스에 std::shared_ptr<bool> _aliveToken = std::make_shared<bool>(true); 선언 필요.
    template<typename Obj, typename Func, typename... Args>
    void Add(std::chrono::steady_clock::time_point execAt,
             std::weak_ptr<bool> aliveToken,
             Obj* obj, Func func, Args&&... args)
    {
        auto boundArgs = std::make_tuple(std::forward<Args>(args)...);
        Add(execAt, [aliveToken, obj, func, args = std::move(boundArgs)]() mutable {
            if (aliveToken.expired()) return;
            std::apply([obj, func](auto&&... a) {
                (obj->*func)(std::forward<decltype(a)>(a)...);
            }, args);
        });
    }

    // 현재 시각 이하인 항목을 전부 꺼내 실행한다. 메인루프에서 호출.
    void Tick();
};

extern TimerExecuter* pTimerExecuter;
