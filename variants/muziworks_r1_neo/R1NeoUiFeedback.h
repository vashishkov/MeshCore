#pragma once

#include <Arduino.h>

class R1NeoUiFeedback {
public:
  enum class ActionSequence {
    BuzzerEnabled,
    BuzzerDisabled,
    GpsEnabled,
    GpsDisabled
  };

  struct UpdateResult {
    const char* leadUpTone = nullptr;
    bool shouldShutdown = false;
  };

  static const char* buzzerEnableMelody();
  static const char* buzzerDisableMelody();
  static const char* gpsEnableMelody();
  static const char* gpsDisableMelody();

  void notifyUnreadChanged();
  void clearUnreadHeartbeat();
  void startActionSequence(ActionSequence sequence);
  UpdateResult update(bool connected, int unreadCount);

private:
  bool _unreadHeartbeatCleared = false;
  bool _heartbeatLedOn = false;
  unsigned long _heartbeatNext = 0;

  bool _actionLedOn = false;
  const char* _actionSequence = nullptr;
  const uint16_t* _actionOnMs = nullptr;
  const uint16_t* _actionOffMs = nullptr;
  uint8_t _actionSequenceLength = 0;
  uint8_t _actionSequenceIndex = 0;
  unsigned long _actionNext = 0;

  bool _powerButtonWasPressed = false;
  bool _shutdownConfirmed = false;
  uint8_t _shutdownLeadUpStep = 0;
  unsigned long _powerButtonDownAt = 0;
  unsigned long _shutdownNextLeadUp = 0;

  void setBlueLed(bool on);
  void setYellowLed(bool on);
  void setActionLed(char color);
  bool serviceActionLed();
  void serviceUnreadHeartbeat(bool connected, int unreadCount);
  const char* servicePowerHold(bool& shouldShutdown);
  bool isPowerButtonPressed() const;
  void resetPowerHold();
  const char* shutdownLeadUpTone(uint8_t step) const;
};
