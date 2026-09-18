#include "tca9554.h"
#include <Arduino.h>
#include <Wire.h>

// Puerto de un solo chip: TCA9554PWR.cpp/h del demo oficial Waveshare hace lo
// mismo con mas opciones (lectura de entradas, mascaras de 8 bits a la vez)
// que esta placa nunca usa -- ver PORTAGE_28ROUND.md.
#define TCA9554_ADDR 0x20
#define TCA9554_REG_OUTPUT 0x01
#define TCA9554_REG_CONFIG 0x03

static uint8_t gOutputShadow = 0xFF;  // todo alto = "no reseteado / no seleccionado"
static bool gFound = false;

static bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool tca9554Begin() {
  Wire.beginTransmission(TCA9554_ADDR);
  gFound = Wire.endTransmission() == 0;
  if (!gFound) {
    Serial.println("TCA9554 no detectado (LCD/SD/tactil dependen de el)");
    return false;
  }
  writeReg(TCA9554_REG_CONFIG, 0x00);  // los 8 pines en modo salida
  writeReg(TCA9554_REG_OUTPUT, gOutputShadow);
  return true;
}

void tca9554Write(uint8_t exioPin, bool high) {
  if (!gFound || exioPin < 1 || exioPin > 8) return;
  uint8_t bit = 1 << (exioPin - 1);
  gOutputShadow = high ? (gOutputShadow | bit) : (gOutputShadow & ~bit);
  writeReg(TCA9554_REG_OUTPUT, gOutputShadow);
}

void tca9554Pulse(uint8_t exioPin, uint16_t lowMs, uint16_t highMs) {
  tca9554Write(exioPin, true);
  delay(10);
  tca9554Write(exioPin, false);
  delay(lowMs);
  tca9554Write(exioPin, true);
  delay(highMs);
}
