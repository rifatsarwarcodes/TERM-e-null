#pragma once

#include "session/Event.hpp"

#include <vector>

namespace ovc::session {

// Anything that reacts to session events: the evidence recorder, the penalty
// engine and the student notifier.
class IEventListener {
public:
    virtual ~IEventListener() = default;
    virtual void onEvent(const Event& event) = 0;
};

// Observer pattern: publishers do not know who is listening. A listener may
// publish while handling an event (the penalty engine reports RULE_HIT);
// such events are queued and delivered after the current one, so every
// listener sees events in the same order.
class EventBus {
public:
    void subscribe(IEventListener* listener);
    void unsubscribe(IEventListener* listener);
    void publish(const Event& event);

private:
    std::vector<IEventListener*> listeners_;
    std::vector<Event> queue_;
    bool delivering_ = false;
};

} // namespace ovc::session
