#pragma once

#include <sys/types.h>

namespace lx {

enum class ProcessSignal {
    Terminate,
    Kill,
};

enum class SignalMechanism {
    PidFd,
    Kill,
};

struct SignalDelivery {
    pid_t pid = -1;
    ProcessSignal signal = ProcessSignal::Terminate;
    SignalMechanism mechanism = SignalMechanism::Kill;
};

struct SignalCapabilities {
    bool signalingAvailable = true;
    bool pidFdAvailable = false;
};

} // namespace lx
