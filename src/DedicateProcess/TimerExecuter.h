#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <tuple>
#include <vector>
#include <algorithm>

class TimerExecuter {
    struct TimerEntry {
        std::chrono::steady_clock::time_point execAt;
        std::function<void()> callback;
        bool operator>(const TimerEntry& o) const { return execAt > o.execAt; }
    };

    std::vector<TimerEntry> _heap;

public:
    void Add(uint32_t delayMs, std::function<void()> func);

    // 사용 측 클래스에 std::shared_ptr<bool> _aliveToken = std::make_shared<bool>(true); 선언 필요.
    template<typename Obj, typename Func, typename... Args>
    void Add(uint32_t delayMs, std::weak_ptr<bool> aliveToken, Obj* obj, Func func, Args&&... args) {
        auto execAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
        auto boundArgs = std::make_tuple(std::forward<Args>(args)...);
        _heap.push_back({ execAt, [aliveToken, obj, func, args = std::move(boundArgs)]() mutable {
            if (aliveToken.expired()) return;
            std::apply([obj, func](auto&&... a) {
                (obj->*func)(std::forward<decltype(a)>(a)...);
            }, args);
        }});
        std::push_heap(_heap.begin(), _heap.end(), std::greater<TimerEntry>{});
    }

    bool Tick();
};
