#pragma once

#include <vector>
#include <utility>
#include <new>

template <typename T>
class ObjectPool {
public:
    ObjectPool() = delete;

    template <typename... Args>
    static T* Acquire(Args&&... args) {
        T* ptr = nullptr;
        auto& pool = GetPool();

        if (pool.empty()) {
            void* raw = ::operator new(sizeof(T));
            ptr = static_cast<T*>(raw);
        } else {
            ptr = pool.back();
            pool.pop_back();
        }
        return new(ptr) T(std::forward<Args>(args)...);
    }

    static void Release(T* ptr) {
        if (!ptr) return;
        ptr->~T(); 
        GetPool().push_back(ptr);
    }

private:
    static std::vector<T*>& GetPool() {
        static std::vector<T*> _instance;
        return _instance;
    }
};