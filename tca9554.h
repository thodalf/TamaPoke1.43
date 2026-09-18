#pragma once
#include <stdint.h>

// Expansor I2C TCA9554 de la ESP32-S3-Touch-LCD-2.8C (direccion 0x20): detras
// de el van LCD_RESET, LCD_CS, TP_RESET, SD_CS, IMU_INT1/2 y RTC_INT (ver
// EXIO_* en pin_config.h). Puerto MINIMO: todo output, sin lectura de
// entradas -- esta placa solo lo usa para resetear/seleccionar, nunca para
// leer una IRQ a traves de el (séria tan lento como sondear por I2C
// directamente, así que no hay ninguna IRQ real detras del expansor).
bool tca9554Begin();                          // false si no responde por I2C
void tca9554Write(uint8_t exioPin, bool high); // exioPin = EXIO_* (1..8)
// Pulso de reset activo-bajo: alto -> bajo lowMs -> alto, con highMs de espera
// despues (el tiempo que el chip reseteado tarda en estar listo).
void tca9554Pulse(uint8_t exioPin, uint16_t lowMs, uint16_t highMs);
