#include "rtcbat.h"
#include "pin_config.h"
#include <Wire.h>
#include <time.h>
#include <SensorPCF85063.hpp>

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

// ---------------------------------------------------------------------------
#if defined(TAMAPOKE_HAS_PMU)
// 1.75: PMU AXP2101 real -- bateria, rail de la pantalla y boton PWR.
// ---------------------------------------------------------------------------
#include <XPowersLib.h>

static XPowersPMU pmu;
static bool pmuOk = false;

bool batBegin() {
  pmuOk = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!pmuOk) Serial.println("AXP2101 no detectado");
  return pmuOk;
}

// Enciende la alimentacion de la AMOLED. En la Waveshare 1.75 el panel (OLED VDD)
// cuelga del rail BLDO1 a 3.3V del AXP2101. El firmware daba por hecho que estaba
// encendido; si el PMU se resetea (drenaje total), BLDO1 queda OFF y la pantalla
// se ve negra aunque el resto funcione. Hay que llamarla ANTES de gfx->begin().
void pmuEnablePanel() {
  if (!pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("AXP2101 no detectado (pmuEnablePanel)");
    return;
  }
  pmu.setBLDO1Voltage(3300);   // OLED VDD
  pmu.enableBLDO1();
}

// el estado de energia (I2C) se cachea ~2 s: leerlo en cada frame del loop
// metia trafico I2C inutil y podia oscilar (parpadeo de brillo)
static uint32_t powerCacheT = 0;
static int cachedPct = -1;
static bool cachedCharging = false, cachedUsb = true;

static void refreshPower() {
  uint32_t now = millis();
  if (powerCacheT && now - powerCacheT < 2000) return;
  powerCacheT = now ? now : 1;
  if (!pmuOk) { cachedPct = -1; cachedCharging = false; cachedUsb = true; return; }
  cachedPct = pmu.isBatteryConnect() ? pmu.getBatteryPercent() : -1;
  cachedCharging = pmu.isCharging();
  cachedUsb = pmu.isVbusIn();
}

int batPercent() { refreshPower(); return cachedPct; }
bool batCharging() { refreshPower(); return cachedCharging; }
bool usbPresent() { refreshPower(); return cachedUsb; }

void pwrSetup() {
  if (!pmuOk) return;
  pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  pmu.clearIrqStatus();
}

bool pwrShortPressed() {
  if (!pmuOk) return false;
  pmu.getIrqStatus();
  bool hit = pmu.isPekeyShortPressIrq();
  if (hit) pmu.clearIrqStatus();
  return hit;
}

#else
// ---------------------------------------------------------------------------
// 1.43 y 2.8" redonda: sin PMU (ni AXP2101 ni equivalente), solo un IC
// cargador simple (ETA6098 en ambas) y un divisor resistivo por ADC. Sin
// gestion de bateria por I2C, sin boton PWR dedicado -- estas funciones son
// stubs para no tener que tocar TamaPoke.ino, que las llama en varios puntos
// (brillo segun USB, icono de bateria, etc).
// ---------------------------------------------------------------------------

bool batBegin() { return true; }  // no hay PMU que iniciar, pero el ADC ya esta listo

void pmuEnablePanel() {
  // El rail de la pantalla se controla por GPIO (1.43: LCD_EN) o ya viene
  // siempre alimentado (2.8" redonda, sin pin de habilitacion en el
  // esquematico) -- no hace falta nada aqui en ninguna de las dos.
}

// Sin PMU no hay lectura de % por I2C, pero SI hay un divisor resistivo en
// BAT_ADC (100K/200K, igual en el esquematico de la 1.43 y el de la 2.8"
// redonda -- mismo diseno de referencia reutilizado). Se asume que el
// resistor de 200K va a GND (relacion 2/3: 4.2V de bateria llena -> 2.8V en
// el pin, aprovechando mejor el rango del ADC que la relacion 1/3 al reves,
// que dejaria solo 1.4V) -- ESTO NO ESTA CONFIRMADO CON UN VOLTIMETRO, es la
// orientacion mas probable segun el uso del ADC, no algo leido directo del
// esquematico. Si el porcentaje mostrado no cuadra con una bateria real,
// este es el primer numero a revisar (probar *3 en vez de *3/2).
static int readBatPercent() {
  uint32_t mv = analogReadMilliVolts(BAT_ADC);
  uint32_t battMv = mv * 3 / 2;
  // Curva LiPo aproximada (no lineal: la tension cae rapido al final). Sin
  // placa para calibrar contra un voltimetro real, es una aproximacion
  // razonable, no una medicion exacta.
  static const uint16_t PTS_MV[]  = { 3300, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200 };
  static const uint8_t  PTS_PCT[] = {    0,   10,   20,   35,   50,   65,   80,   90,  100 };
  const int N = sizeof(PTS_MV) / sizeof(PTS_MV[0]);
  if (battMv <= PTS_MV[0]) return 0;
  if (battMv >= PTS_MV[N - 1]) return 100;
  for (int i = 1; i < N; i++) {
    if (battMv <= PTS_MV[i]) {
      uint32_t span = PTS_MV[i] - PTS_MV[i - 1];
      uint32_t into = battMv - PTS_MV[i - 1];
      return PTS_PCT[i - 1] + (int)((PTS_PCT[i] - PTS_PCT[i - 1]) * into / span);
    }
  }
  return 100;
}

int batPercent() { return readBatPercent(); }
bool batCharging() { return false; }  // el STAT del cargador no llega a ningun GPIO en estas placas
bool usbPresent() { return true; }   // sin deteccion VBUS: asumimos alimentado

void pwrSetup() {
  // Sin boton PWR dedicado en estas placas (solo BOOT/RESET) -- nada que armar
}

bool pwrShortPressed() { return false; }

#endif
