#include "session/EventBus.hpp"

#include <algorithm>

namespace ovc::session {

void EventBus::subscribe(IEventListener* listener)
{
    if (std::find(listeners_.begin(), listeners_.end(), listener) == listeners_.end())
        listeners_.push_back(listener);
}

void EventBus::unsubscribe(IEventListener* listener)
{
    listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener), listeners_.end());
}

void EventBus::publish(const Event& event)
{
    queue_.push_back(event);
    if (delivering_)
        return; // delivered by the outer publish() call
    delivering_ = true;
    for (std::size_t i = 0; i < queue_.size(); ++i) {
        const Event current = queue_[i]; // copy: listeners may grow the queue
        const auto listeners = listeners_;
        for (auto* listener : listeners)
            listener->onEvent(current);
    }
    queue_.clear();
    delivering_ = false;
}

} // namespace ovc::session
