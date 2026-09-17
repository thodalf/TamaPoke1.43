#include "rtcbat.h"
#include "pin_config.h"
#include <Wire.h>
#include <time.h>
#include <SensorPCF85063.hpp>

// NOTA PORTAGE 1.43: esta placa no tiene PMU AXP2101 (a diferencia de la
// 1.75), asi que toda la seccion XPowersLib/bateria/boton PWR del original
// fue eliminada. Las funciones se mantienen como stubs para no tener que
// tocar TamaPoke.ino, que las llama en varios puntos (brillo segun USB,
// icono de bateria, etc).

static SensorPCF85063 rtc;
static bool rtcOk = false;

bool rtcBegin() {
  rtcOk = rtc.begin(Wire, IIC_SDA, IIC_SCL);
  if (!rtcOk) Serial.println("PCF85063 no detectado");
  return rtcOk;
}

uint32_t rtcEpoch() {
  if (!rtcOk) return 0;
  RTC_DateTime t = rtc.getDateTime();
  if (t.getYear() < 2025 || t.getYear() > 2120) return 0;  // sin hora valida
  struct tm tmv = {};
  tmv.tm_year = t.getYear() - 1900;
  tmv.tm_mon = t.getMonth() - 1;
  tmv.tm_mday = t.getDay();
  tmv.tm_hour = t.getHour();
  tmv.tm_min = t.getMinute();
  tmv.tm_sec = t.getSecond();
  time_t e = mktime(&tmv);  // TZ por defecto = UTC, consistente con gmtime_r
  return e > 0 ? (uint32_t)e : 0;
}

void rtcSetEpoch(uint32_t e) {
  if (!rtcOk) return;
  time_t tt = e;
  struct tm tmv;
  gmtime_r(&tt, &tmv);
  rtc.setDateTime(RTC_DateTime(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                               tmv.tm_hour, tmv.tm_min, tmv.tm_sec));
}

// ---- stubs sin PMU (ver nota arriba) ----

bool batBegin() { return false; }

void pmuEnablePanel() {
  // En la 1.75 esto encendia el rail BLDO1 del AXP2101. En la 1.43 el rail
  // de la pantalla se controla directamente por GPIO (LCD_EN, ver
  // TamaPoke.ino: pinMode(LCD_EN, OUTPUT); digitalWrite(LCD_EN, HIGH);
  // ANTES de gfx->begin()) -- no hace falta nada aqui.
}

int batPercent() { return -1; }      // sin PMU: no se puede leer % de bateria
bool batCharging() { return false; }
bool usbPresent() { return true; }   // sin deteccion VBUS: asumimos alimentado

void pwrSetup() {
  // Sin boton PWR dedicado en esta placa (solo BOOT/RESET) -- nada que armar
}

bool pwrShortPressed() { return false; }
