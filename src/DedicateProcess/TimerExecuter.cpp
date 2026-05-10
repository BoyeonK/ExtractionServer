#include "TimerExecuter.h"

void TimerExecuter::Add(uint32_t delayMs, std::function<void()> func) {
    auto execAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
    _heap.push_back({ execAt, std::move(func) });
    std::push_heap(_heap.begin(), _heap.end(), std::greater<TimerEntry>{});
}

void TimerExecuter::Tick() {
    auto now = std::chrono::steady_clock::now();
    while (!_heap.empty() && _heap.front().execAt <= now) {
        std::pop_heap(_heap.begin(), _heap.end(), std::greater<TimerEntry>{});
        TimerEntry entry = std::move(_heap.back());
        _heap.pop_back();
        entry.callback();
    }
}
