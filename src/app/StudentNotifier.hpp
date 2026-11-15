#pragma once

#include "session/EventBus.hpp"

#include <functional>
#include <string>

namespace ovc::app {

// Turns penalty level changes into short, neutral notices for the student.
// Students never see a running score; they see that rules apply and can
// read them with :rules.
class StudentNotifier final : public session::IEventListener {
public:
    using Show = std::function<void(std::string)>;

    explicit StudentNotifier(Show show)
        : show_(std::move(show))
    {
    }

    void onEvent(const session::Event& event) override
    {
        if (event.type() != session::EventType::LevelChange)
            return;
        const std::string level = event.getOr("level");
        if (level == "WARNING")
            show_("The paste rules of this exam apply to your session. Type :rules to read them.");
        else if (level == "FLAGGED")
            show_("Your session will be reviewed by the examiner at the viva.");
    }

private:
    Show show_;
};

} // namespace ovc::app
