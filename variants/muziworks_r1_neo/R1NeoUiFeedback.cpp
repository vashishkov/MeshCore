#include "R1NeoUiFeedback.h"

#ifndef USER_BTN_PRESSED
#define USER_BTN_PRESSED LOW
#endif

namespace {
static constexpr uint16_t HEARTBEAT_ON_MS = 10;
static constexpr uint16_t HEARTBEAT_OFF_MS = 3000;
static constexpr uint16_t SHUTDOWN_LEADUP_START_MS = 1000;
static constexpr uint16_t SHUTDOWN_LEADUP_INTERVAL_MS = 1000;
static constexpr uint16_t SHUTDOWN_CONFIRM_MS = 5000;
static constexpr uint8_t SHUTDOWN_LEADUP_TONES = 3;

struct LedSequence {
  const char* colors;
  const uint16_t* onMs;
  const uint16_t* offMs;
  uint8_t length;
};

static const char SEQ_BUZZER_ENABLED[] = "YB";
static const char SEQ_BUZZER_DISABLED[] = "BY";
static const char SEQ_GPS_ENABLED[] = "YBB";
static const char SEQ_GPS_DISABLED[] = "BYY";

// R1 feedback uses MeshCore's native C/E/G pitch family; timings mirror RTTTL durations.
static const uint16_t BUZZER_ON_ENABLED_MS[] = {94, 188};
static const uint16_t BUZZER_ON_DISABLED_MS[] = {188, 94};
static const uint16_t BUZZER_OFF_MS[] = {47};
static const uint16_t GPS_ON_ENABLED_MS[] = {94, 94, 188};
static const uint16_t GPS_ON_DISABLED_MS[] = {188, 94, 94};
static const uint16_t GPS_OFF_MS[] = {47, 47};

bool timeReached(uint32_t now, uint32_t target) {
  return (int32_t)(now - target) >= 0;
}

LedSequence sequenceFor(R1NeoUiFeedback::ActionSequence sequence) {
  switch (sequence) {
    case R1NeoUiFeedback::ActionSequence::BuzzerEnabled:
      return {SEQ_BUZZER_ENABLED, BUZZER_ON_ENABLED_MS, BUZZER_OFF_MS, sizeof(SEQ_BUZZER_ENABLED) - 1};
    case R1NeoUiFeedback::ActionSequence::BuzzerDisabled:
      return {SEQ_BUZZER_DISABLED, BUZZER_ON_DISABLED_MS, BUZZER_OFF_MS, sizeof(SEQ_BUZZER_DISABLED) - 1};
    case R1NeoUiFeedback::ActionSequence::GpsEnabled:
      return {SEQ_GPS_ENABLED, GPS_ON_ENABLED_MS, GPS_OFF_MS, sizeof(SEQ_GPS_ENABLED) - 1};
    case R1NeoUiFeedback::ActionSequence::GpsDisabled:
      return {SEQ_GPS_DISABLED, GPS_ON_DISABLED_MS, GPS_OFF_MS, sizeof(SEQ_GPS_DISABLED) - 1};
  }

  return {nullptr, nullptr, nullptr, 0};
}
}

const char* R1NeoUiFeedback::buzzerEnableMelody() {
  return "BuzOn:d=16,o=6,b=160:16c,32p,8g";
}

const char* R1NeoUiFeedback::buzzerDisableMelody() {
  return "BuzOff:d=16,o=6,b=160:8g,32p,16c";
}

const char* R1NeoUiFeedback::gpsEnableMelody() {
  return "GpsOn:d=16,o=6,b=160:16c,32p,16e,32p,8g";
}

const char* R1NeoUiFeedback::gpsDisableMelody() {
  return "GpsOff:d=16,o=6,b=160:8g,32p,16e,32p,16c";
}

void R1NeoUiFeedback::notifyUnreadChanged() {
  _unreadHeartbeatCleared = false;
  _heartbeatLedOn = false;
  _heartbeatNext = 0;
  setBlueLed(false);
}

void R1NeoUiFeedback::clearUnreadHeartbeat() {
  _unreadHeartbeatCleared = true;
  _heartbeatLedOn = false;
  _heartbeatNext = 0;
  setBlueLed(false);
}

void R1NeoUiFeedback::startActionSequence(ActionSequence sequence) {
  LedSequence led_sequence = sequenceFor(sequence);
  if (led_sequence.colors == nullptr || led_sequence.length == 0) return;

  _heartbeatLedOn = false;
  _heartbeatNext = 0;
  _actionLedOn = false;
  _actionSequence = led_sequence.colors;
  _actionOnMs = led_sequence.onMs;
  _actionOffMs = led_sequence.offMs;
  _actionSequenceLength = led_sequence.length;
  _actionSequenceIndex = 0;
  _actionNext = 0;
  setActionLed(0);
}

R1NeoUiFeedback::UpdateResult R1NeoUiFeedback::update(bool connected, int unreadCount) {
  UpdateResult result;
  result.leadUpTone = servicePowerHold(result.shouldShutdown);

  if (!result.shouldShutdown && !serviceActionLed()) {
    serviceUnreadHeartbeat(connected, unreadCount);
  }

  return result;
}

void R1NeoUiFeedback::setBlueLed(bool on) {
#ifdef LED_BLUE
  digitalWrite(LED_BLUE, on ? LED_STATE_ON : !LED_STATE_ON);
#endif
}

void R1NeoUiFeedback::setYellowLed(bool on) {
#ifdef LED_GREEN
  digitalWrite(LED_GREEN, on ? LED_STATE_ON : !LED_STATE_ON);
#endif
}

void R1NeoUiFeedback::setActionLed(char color) {
  switch (color) {
    case 'B':
      setYellowLed(false);
      setBlueLed(true);
      break;
    case 'Y':
      setBlueLed(false);
      setYellowLed(true);
      break;
    default:
      setBlueLed(false);
      setYellowLed(false);
      break;
  }
}

bool R1NeoUiFeedback::serviceActionLed() {
  if (_actionSequence == nullptr && !_actionLedOn) {
    return false;
  }

  uint32_t now = millis();
  if (_actionNext != 0 && !timeReached(now, _actionNext)) {
    return true;
  }

  if (_actionLedOn) {
    setActionLed(0);
    _actionLedOn = false;
    if (_actionSequenceIndex >= _actionSequenceLength) {
      _actionSequence = nullptr;
      _actionOnMs = nullptr;
      _actionOffMs = nullptr;
      _actionSequenceLength = 0;
      _actionSequenceIndex = 0;
      _actionNext = 0;
      _heartbeatNext = now + HEARTBEAT_OFF_MS;
    } else {
      uint8_t played_index = _actionSequenceIndex - 1;
      _actionNext = now + _actionOffMs[played_index];
    }
    return true;
  }

  if (_actionSequenceIndex >= _actionSequenceLength) {
    _actionSequence = nullptr;
    _actionOnMs = nullptr;
    _actionOffMs = nullptr;
    _actionSequenceLength = 0;
    _actionSequenceIndex = 0;
    _actionNext = 0;
    return false;
  }

  setActionLed(_actionSequence[_actionSequenceIndex]);
  uint16_t on_ms = _actionOnMs[_actionSequenceIndex];
  _actionSequenceIndex++;
  _actionLedOn = true;
  _actionNext = now + on_ms;
  return true;
}

void R1NeoUiFeedback::serviceUnreadHeartbeat(bool connected, int unreadCount) {
  if (connected || unreadCount <= 0 || _unreadHeartbeatCleared) {
    if (_heartbeatLedOn) {
      setBlueLed(false);
      _heartbeatLedOn = false;
    }
    _heartbeatNext = 0;
    return;
  }

  uint32_t now = millis();
  if (_heartbeatNext != 0 && !timeReached(now, _heartbeatNext)) {
    return;
  }

  if (_heartbeatLedOn) {
    setBlueLed(false);
    _heartbeatLedOn = false;
    _heartbeatNext = now + HEARTBEAT_OFF_MS;
  } else {
    setBlueLed(true);
    _heartbeatLedOn = true;
    _heartbeatNext = now + HEARTBEAT_ON_MS;
  }
}

const char* R1NeoUiFeedback::servicePowerHold(bool& shouldShutdown) {
  shouldShutdown = false;
  bool pressed = isPowerButtonPressed();
  uint32_t now = millis();

  if (pressed && !_powerButtonWasPressed) {
    _powerButtonWasPressed = true;
    _shutdownConfirmed = false;
    _shutdownLeadUpStep = 0;
    _powerButtonDownAt = now;
    _shutdownNextLeadUp = now + SHUTDOWN_LEADUP_START_MS;
    return nullptr;
  }

  if (pressed) {
    uint32_t held_ms = now - _powerButtonDownAt;
    const char* lead_up_tone = nullptr;
    if (_shutdownLeadUpStep < SHUTDOWN_LEADUP_TONES &&
        _shutdownNextLeadUp != 0 &&
        timeReached(now, _shutdownNextLeadUp)) {
      lead_up_tone = shutdownLeadUpTone(_shutdownLeadUpStep);
      _shutdownLeadUpStep++;
      _shutdownNextLeadUp = now + SHUTDOWN_LEADUP_INTERVAL_MS;
    }

    if (!_shutdownConfirmed && held_ms >= SHUTDOWN_CONFIRM_MS) {
      _shutdownConfirmed = true;
    }
    return lead_up_tone;
  }

  if (_powerButtonWasPressed) {
    shouldShutdown = _shutdownConfirmed;
    resetPowerHold();
  }

  return nullptr;
}

bool R1NeoUiFeedback::isPowerButtonPressed() const {
#ifdef PIN_USER_BTN
  return digitalRead(PIN_USER_BTN) == USER_BTN_PRESSED;
#else
  return false;
#endif
}

void R1NeoUiFeedback::resetPowerHold() {
  _powerButtonWasPressed = false;
  _shutdownConfirmed = false;
  _shutdownLeadUpStep = 0;
  _powerButtonDownAt = 0;
  _shutdownNextLeadUp = 0;
}

const char* R1NeoUiFeedback::shutdownLeadUpTone(uint8_t step) const {
  switch (step) {
    case 0:
      return "Hold1:d=16,o=5,b=160:c";
    case 1:
      return "Hold2:d=16,o=5,b=160:e";
    default:
      return "Hold3:d=16,o=5,b=160:g";
  }
}
