#pragma once
#include <functional>

class Event {
public:
    void Connect(std::function<void()> callback) {
        callbacks.push_back(callback);
    }

    void Fire() {
        for (auto& callback : callbacks) {
            callback();
        }
    }

private:
    std::vector<std::function<void()>> callbacks;
};
