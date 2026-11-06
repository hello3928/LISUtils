#pragma once

#include <functional>
#include <mutex>
#include <vector>

namespace lis::core {

class ActionQueue {
public:
    using Action = std::function<void()>;

    void Post(Action action);
    void Drain();

private:
    std::mutex mutex_;
    std::vector<Action> pending_;
};

}
