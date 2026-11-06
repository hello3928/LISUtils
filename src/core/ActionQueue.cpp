#include "core/ActionQueue.h"

#include <utility>

namespace lis::core {

void ActionQueue::Post(Action action) {
    const std::scoped_lock lock(mutex_);
    pending_.push_back(std::move(action));
}

void ActionQueue::Drain() {
    std::vector<Action> ready;
    {
        const std::scoped_lock lock(mutex_);
        ready.swap(pending_);
    }
    for (const Action& action : ready) {
        action();
    }
}

}
