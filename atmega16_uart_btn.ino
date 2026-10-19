/*
 * ATmega16 — button → UART to ESP32
 * MightyCore: ATmega16, Internal 8 MHz, USBasp, Upload Using Programmer
 * PB0 button→GND, PB1 LED optional, PD1 TX → divider → ESP32 GPIO16
 */
const uint8_t BTN_BIT = PB0;
const uint8_t LED_BIT = PB1;
const unsigned long DEBOUNCE_MS = 40;
const unsigned long COOLDOWN_MS = 800;
bool lastStableHigh = true;
unsigned long lastChangeMs = 0;
unsigned long lastSendMs = 0;
void pulseLed(unsigned long ms) {
  PORTB |= (1 << LED_BIT);
  delay(ms);
  PORTB &= ~(1 << LED_BIT);
}
void setup() {
  DDRB |= (1 << LED_BIT);
  DDRB &= ~(1 << BTN_BIT);
  PORTB |= (1 << BTN_BIT);
  PORTB &= ~(1 << LED_BIT);
  Serial.begin(9600);
  delay(100);
  Serial.println(F("AVR online"));
  pulseLed(120);
}
void loop() {
  const bool pinHigh = (PINB & (1 << BTN_BIT)) != 0;
  const unsigned long now = millis();
  if (pinHigh != lastStableHigh && (now - lastChangeMs) > DEBOUNCE_MS) {
    lastChangeMs = now;
    lastStableHigh = pinHigh;
    if (!pinHigh && (now - lastSendMs) > COOLDOWN_MS) {
      lastSendMs = now;
      Serial.println(F("BTN"));
      pulseLed(60);
    }
  }
}