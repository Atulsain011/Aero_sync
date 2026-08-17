#include "aerosync/pairing_state_machine.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace aerosync {

PairingStateMachine::PairingStateMachine()
    : m_state(PairingState::UNPAIRED) {}

PairingStateMachine::~PairingStateMachine() {
    reset();
}

PairingState PairingStateMachine::getCurrentState() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_state;
}

std::string PairingStateMachine::getActivePin() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_activePin;
}

std::string PairingStateMachine::getSessionToken() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_sessionToken;
}

std::string PairingStateMachine::getRemoteDeviceId() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_remoteDeviceId;
}

bool PairingStateMachine::isSessionAuthenticated() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return (m_state == PairingState::AUTHENTICATED_SESSION || m_state == PairingState::TRANSFERRING) &&
           !m_sessionToken.empty();
}

void PairingStateMachine::setStateChangedCallback(PairingStateChangedCallback cb) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_stateCallback = cb;
}

std::string PairingStateMachine::generateRandom6DigitPin() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(100000, 999999);
    return std::to_string(dis(gen));
}

std::string PairingStateMachine::generateSecureSessionToken() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(16) << dis(gen)
       << std::setw(16) << dis(gen);
    return ss.str();
}

void PairingStateMachine::transitionTo(PairingState newState, const std::string& reason) {
    if (m_state == newState) return;
    m_state = newState;
    PairingStateChangedCallback cb = m_stateCallback;
    if (cb) {
        cb(newState, reason);
    }
}

bool PairingStateMachine::initiatePairing(const std::string& remoteDeviceId, const std::string& pin) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_state == PairingState::TRANSFERRING) {
        return false;
    }

    m_remoteDeviceId = remoteDeviceId;
    m_activePin = pin.empty() ? generateRandom6DigitPin() : pin;
    m_sessionToken.clear();
    m_requestTimestamp = std::chrono::steady_clock::now();

    transitionTo(PairingState::PAIRING_REQUESTED, "Initiating pairing with PIN " + m_activePin);
    return true;
}

bool PairingStateMachine::onPairingResponseReceived(PairingStatus status, const std::string& sessionToken, const std::string& reason) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (status == PairingStatus::PAIRING_ACCEPTED && !sessionToken.empty()) {
        m_sessionToken = sessionToken;
        transitionTo(PairingState::AUTHENTICATED_SESSION, "Pairing accepted by remote peer");
        return true;
    } else {
        std::string failReason = reason.empty() ? "Pairing rejected or failed" : reason;
        transitionTo(PairingState::DISCONNECTED, failReason);
        return false;
    }
}

bool PairingStateMachine::handleIncomingRequest(const std::string& remoteDeviceId, const std::string& remotePin) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_state == PairingState::TRANSFERRING) {
        return false; // Busy
    }

    m_remoteDeviceId = remoteDeviceId;
    m_activePin = remotePin;
    m_sessionToken.clear();
    m_requestTimestamp = std::chrono::steady_clock::now();

    transitionTo(PairingState::AWAITING_PIN_CONFIRMATION, "Incoming pairing request from " + remoteDeviceId);
    return true;
}

bool PairingStateMachine::confirmPinMatch(const std::string& enteredPin) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (enteredPin.empty() || enteredPin == m_activePin) {
        m_sessionToken = generateSecureSessionToken();
        transitionTo(PairingState::AUTHENTICATED_SESSION, "PIN verified, session established");
        return true;
    } else {
        transitionTo(PairingState::DISCONNECTED, "PIN mismatch");
        return false;
    }
}

bool PairingStateMachine::declineIncomingRequest(const std::string& reason) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_state != PairingState::AWAITING_PIN_CONFIRMATION && m_state != PairingState::PAIRING_REQUESTED) {
        return false;
    }

    transitionTo(PairingState::DISCONNECTED, reason);
    return true;
}

bool PairingStateMachine::startTransfer() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    transitionTo(PairingState::TRANSFERRING, "Transfer started");
    return true;
}

bool PairingStateMachine::finishTransfer(bool success) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_sessionToken.empty()) {
        transitionTo(PairingState::AUTHENTICATED_SESSION, success ? "Transfer completed successfully" : "Transfer failed");
    } else {
        transitionTo(PairingState::UNPAIRED, success ? "Transfer completed successfully" : "Transfer failed");
    }
    return true;
}

void PairingStateMachine::disconnect(const std::string& reason) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    transitionTo(PairingState::DISCONNECTED, reason);
}

void PairingStateMachine::reset() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_remoteDeviceId.clear();
    m_activePin.clear();
    m_sessionToken.clear();
    transitionTo(PairingState::UNPAIRED, "State reset to unpaired");
}

bool PairingStateMachine::checkTimeout() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_state == PairingState::PAIRING_REQUESTED || m_state == PairingState::AWAITING_PIN_CONFIRMATION) {
        auto now = std::chrono::steady_clock::now();
        auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now - m_requestTimestamp).count();
        if (elapsedSec >= PAIRING_TIMEOUT_SEC) {
            transitionTo(PairingState::DISCONNECTED, "Pairing operation timed out (30s)");
            return true;
        }
    }
    return false;
}

} // namespace aerosync
