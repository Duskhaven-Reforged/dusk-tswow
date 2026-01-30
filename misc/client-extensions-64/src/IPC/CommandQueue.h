#pragma once
#include <mutex>
#include <queue>
#include <functional>

class CommandQueue {
public:
    void Push(std::function<void()> fn) {
        std::scoped_lock lk(mu_);
        q_.push(std::move(fn));
    }

    bool TryPop(std::function<void()>& out) {
        std::scoped_lock lk(mu_);
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop();
        return true;
    }

private:
    std::mutex mu_;
    std::queue<std::function<void()>> q_;
};
