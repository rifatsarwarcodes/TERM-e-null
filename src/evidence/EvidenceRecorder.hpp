#pragma once

#include "evidence/HashChain.hpp"
#include "session/EventBus.hpp"

namespace ovc::evidence {

// Writes every published event to the chained events.log as it happens, so
// the record survives a crash up to the last action.
class EvidenceRecorder final : public session::IEventListener {
public:
    explicit EvidenceRecorder(ChainedLogWriter& eventsLog)
        : log_(eventsLog)
    {
    }

    void onEvent(const session::Event& event) override { log_.append(event.encode()); }

private:
    ChainedLogWriter& log_;
};

} // namespace ovc::evidence
