#pragma once
#include <Arduino.h>

// RTC PCF85063: hora persistente mientras la placa tenga alimentacion
bool rtcBegin();
uint32_t rtcEpoch();             // segundos unix; 0 si el RTC no es valido
void rtcSetEpoch(uint32_t e);

// Bateria: en la 1.43 NO hay PMU (AXP2101) como en la 1.75 -- estas
// funciones quedan como stubs sin efecto / con valores "sin bateria" para
// que el resto del firmware (TamaPoke.ino) no necesite modificarse.
bool batBegin();                 // no-op, devuelve false
void pmuEnablePanel();           // no-op (el rail de pantalla se activa por LCD_EN en TamaPoke.ino)
int batPercent();                // siempre -1 (sin bateria/PMU detectable)
bool batCharging();              // siempre false
bool usbPresent();                // siempre true (asumimos alimentacion USB constante)

// No hay boton PWR dedicado en esta placa (solo BOOT/RESET) -- no-ops
void pwrSetup();
bool pwrShortPressed();          // siempre false
