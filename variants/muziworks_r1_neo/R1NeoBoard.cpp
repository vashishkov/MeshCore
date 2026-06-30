#include <Arduino.h>
#include <Wire.h>

#include "R1NeoBoard.h"

#ifdef NRF52_POWER_MANAGEMENT
// Static configuration for power management
// Values set in variant.h defines
const PowerMgtConfig power_config = {
  .lpcomp_ain_channel = PWRMGT_LPCOMP_AIN,
  .lpcomp_refsel = PWRMGT_LPCOMP_REFSEL,
  .voltage_bootlock = PWRMGT_VOLTAGE_BOOTLOCK
};

void R1NeoBoard::initiateShutdown(uint8_t reason) {
  digitalWrite(SX126X_POWER_EN, LOW);
  digitalWrite(PIN_SOFT_SHUTDOWN, LOW);
  digitalWrite(PIN_DCDC_EN_MCU_HOLD, LOW);

  pinMode(SX126X_POWER_EN, OUTPUT);
  pinMode(PIN_SOFT_SHUTDOWN, OUTPUT);
  pinMode(PIN_DCDC_EN_MCU_HOLD, OUTPUT);

  if (reason == SHUTDOWN_REASON_LOW_VOLTAGE ||
      reason == SHUTDOWN_REASON_BOOT_PROTECT) {
    configureVoltageWake(power_config.lpcomp_ain_channel, power_config.lpcomp_refsel);
  }

  enterSystemOff(reason);
}

#endif // NRF52_POWER_MANAGEMENT

void R1NeoBoard::begin() {
  digitalWrite(PIN_DCDC_EN_MCU_HOLD, HIGH);
  pinMode(PIN_DCDC_EN_MCU_HOLD, OUTPUT);
  digitalWrite(PIN_SOFT_SHUTDOWN, HIGH);
  pinMode(PIN_SOFT_SHUTDOWN, OUTPUT);

  // The user button is active-high and passed through from the I/O controller.
  pinMode(PIN_USER_BTN, INPUT);

  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, !LED_STATE_ON);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, !LED_STATE_ON);

  // Keep the radio off until the voltage check has passed.
  digitalWrite(SX126X_POWER_EN, LOW);
  pinMode(SX126X_POWER_EN, OUTPUT);

  // Battery pins must be ready before checkBootVoltage() samples VBAT.
  pinMode(PIN_BAT_CHG, INPUT_PULLUP);
  pinMode(PIN_VBAT_READ, INPUT);

#ifdef NRF52_POWER_MANAGEMENT
  checkBootVoltage(&power_config);
#endif

  NRF52BoardDCDC::begin();

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  Wire.setPins(PIN_WIRE_SDA, PIN_WIRE_SCL);
  Wire.begin();

  digitalWrite(SX126X_POWER_EN, HIGH);
  delay(10);   // give sx1262 some time to power up
}
