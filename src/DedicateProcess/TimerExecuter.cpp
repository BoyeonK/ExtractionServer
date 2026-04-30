#include "TimerExecuter.h"

TimerExecuter* pTimerExecuter = nullptr;

void TimerExecuter::Add(uint32_t delayMs, std::function<void()> func) {
    auto execAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
    _queue.push({ execAt, std::move(func) });
}

void TimerExecuter::Tick() {
    auto now = std::chrono::steady_clock::now();
    while (!_queue.empty() && _queue.top().execAt <= now) {
        auto entry = _queue.top();
        _queue.pop();
        entry.callback();
    }
}
