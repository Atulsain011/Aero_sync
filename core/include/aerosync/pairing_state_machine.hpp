#ifndef AEROSYNC_PAIRING_STATE_MACHINE_HPP
#define AEROSYNC_PAIRING_STATE_MACHINE_HPP

#include "types.hpp"
#include <string>
#include <mutex>
#include <chrono>
#include <functional>
#include <atomic>

namespace aerosync {

class PairingStateMachine {
public:
    PairingStateMachine();
    ~PairingStateMachine();

    // State queries
    PairingState getCurrentState() const;
    std::string getActivePin() const;
    std::string getSessionToken() const;
    std::string getRemoteDeviceId() const;
    bool isSessionAuthenticated() const;

    // Callbacks
    void setStateChangedCallback(PairingStateChangedCallback cb);

    // PIN Utilities
    static std::string generateRandom6DigitPin();
    static std::string generateSecureSessionToken();

    // Initiator Transitions (Outgoing)
    bool initiatePairing(const std::string& remoteDeviceId, const std::string& pin = "");
    bool onPairingResponseReceived(PairingStatus status, const std::string& sessionToken, const std::string& reason = "");

    // Responder Transitions (Incoming)
    bool handleIncomingRequest(const std::string& remoteDeviceId, const std::string& remotePin);
    bool confirmPinMatch(const std::string& enteredPin);
    bool declineIncomingRequest(const std::string& reason = "User declined pairing");

    // Session & Transfer Transitions
    bool startTransfer();
    bool finishTransfer(bool success);
    void disconnect(const std::string& reason = "Disconnected");
    void reset();

    // Timeout Check (30 seconds)
    bool checkTimeout();

private:
    void transitionTo(PairingState newState, const std::string& reason = "");

    mutable std::recursive_mutex m_mutex;
    PairingState m_state{PairingState::UNPAIRED};
    std::string m_remoteDeviceId;
    std::string m_activePin;
    std::string m_sessionToken;
    std::chrono::steady_clock::time_point m_requestTimestamp;
    PairingStateChangedCallback m_stateCallback;
};

} // namespace aerosync

#endif // AEROSYNC_PAIRING_STATE_MACHINE_HPP
