#include "audio.h"
#include "pin_config.h"
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>

// ---------------------------------------------------------------------------
#if defined(TAMAPOKE_HAS_AUDIO_ES8311)
// 1.75: codec ES8311 (DAC -> amplificador PA -> altavoz) por I2S. Init del
// ES8311 portado del driver oficial de Espressif (esp-bsp), fijado a
// MCLK=4.096MHz (256*fs), 16kHz, 16-bit, esclavo I2S. Los efectos son tonos
// cuadrados (estilo Game Boy) sintetizados en una tarea aparte para no
// bloquear el loop de juego.
// ---------------------------------------------------------------------------
#include "gbsynth.h"
#include "music.h"
#include <ESP_I2S.h>

#define ES8311_ADDR 0x18
#define SAMPLE_RATE 16000

static I2SClass i2s;
static bool gReady = false;
static bool gOn = true;
static QueueHandle_t gQ = nullptr;

// ---- I2C del codec ----
static bool esW(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static uint8_t esR(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ES8311_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

// Secuencia de init VERIFICADA EN PLACA (proyecto PlaneRadar2.0, misma
// Waveshare 1.75). Clave: reloj DERIVADO DEL BCLK (reg01=0xBF, sin MCLK externo)
// y referencia interna que alimenta el DAC (reg44=0x58); sin esos dos el codec
// respondia por I2C pero no salia audio. 16kHz, 16-bit, esclavo I2S.
static bool es8311Init() {
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) return false;

  // open()
  esW(0x0D, 0xFA); esW(0x44, 0x08); esW(0x44, 0x08);  // power up + quirk de 1a escritura
  esW(0x01, 0x30); esW(0x02, 0x00); esW(0x03, 0x10); esW(0x16, 0x24);
  esW(0x04, 0x10); esW(0x05, 0x00); esW(0x0B, 0x00); esW(0x0C, 0x00);
  esW(0x10, 0x1F); esW(0x11, 0x7F);
  esW(0x00, 0x80); esW(0x00, 0x80);                   // reset clock, esclavo
  esW(0x01, 0xBF);                                    // clk src = BCLK (sin MCLK externo)
  { uint8_t r = esR(0x06); r &= ~0x20; esW(0x06, r); }  // SCLK no invertido
  esW(0x13, 0x10); esW(0x1B, 0x0A); esW(0x1C, 0x6A);
  esW(0x44, 0x58);                                    // referencia interna -> alimenta el DAC

  // config_sample(): BCLK*8 = DIG_MCLK
  esW(0x02, 0x18); esW(0x05, 0x00); esW(0x03, 0x10); esW(0x04, 0x20);
  { uint8_t r = esR(0x07); r &= 0xC0; esW(0x07, r); }
  esW(0x08, 0xFF);
  { uint8_t r = esR(0x06); r &= 0xE0; r |= 0x03; esW(0x06, r); }  // bclk_div=4

  // formato I2S 16-bit
  esW(0x09, 0x0C); esW(0x0A, 0x0C);

  // start() DAC esclavo
  esW(0x00, 0x80); esW(0x01, 0xBF); esW(0x09, 0x0C); esW(0x0A, 0x0C);
  esW(0x17, 0xBF); esW(0x0E, 0x02); esW(0x12, 0x00); esW(0x14, 0x1A);
  esW(0x0D, 0x01); esW(0x15, 0x40); esW(0x37, 0x08); esW(0x45, 0x00);

  // volumen + unmute
  esW(0x32, 0xBF);                                    // volumen DAC ~0 dB
  { uint8_t r = esR(0x31); r &= 0x9F; esW(0x31, r); }  // unmute
  return true;
}

// ---- sintetizador de tono cuadrado ----
struct Note { uint16_t f, ms; };

static const Note N_TAP[]    = {{880, 35}};
static const Note N_EAT[]    = {{660, 45}, {0, 12}, {660, 45}};
static const Note N_PLAY[]   = {{784, 45}, {988, 60}};
static const Note N_HEART[]  = {{1047, 55}, {1319, 90}};
static const Note N_HATCH[]  = {{523, 80}, {659, 80}, {784, 110}, {1047, 170}};
static const Note N_EVOLVE[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 90}, {1319, 230}};
static const Note N_MEDAL[]  = {{784, 70}, {0, 25}, {784, 70}, {0, 25}, {1047, 200}};
static const Note N_DENY[]   = {{300, 110}, {200, 170}};
static const Note N_BYE[]    = {{784, 150}, {659, 150}, {523, 280}};
static const Note N_LEVEL[]  = {{784, 70}, {1047, 130}};

struct SfxDef { const Note *n; uint8_t len; };
// battle cues, deliberadamente cortas: caen entre turnos, no encima de ellos
static const Note N_HIT[]    = { {180, 40}, {120, 50} };
static const Note N_BEAM[]   = { {880, 30}, {1180, 30}, {1560, 60} };
static const Note N_STATUS[] = { {440, 60}, {370, 60}, {330, 90} };
static const Note N_SUPER[]  = { {1200, 40}, {1600, 40}, {2000, 80} };
static const Note N_FAINT[]  = { {520, 90}, {400, 110}, {300, 140}, {200, 200} };
static const Note N_VICTORY[] = { {784, 120}, {784, 120}, {784, 120}, {1047, 320},
                                  {880, 140}, {1047, 420} };

static const SfxDef SFX[SFX_COUNT] = {
  {N_TAP, 1}, {N_EAT, 3}, {N_PLAY, 2}, {N_HEART, 2}, {N_HATCH, 4},
  {N_EVOLVE, 5}, {N_MEDAL, 5}, {N_DENY, 2}, {N_BYE, 3}, {N_LEVEL, 2},
  {N_HIT, 2}, {N_BEAM, 3}, {N_STATUS, 3}, {N_SUPER, 3}, {N_FAINT, 4},
  {N_VICTORY, 6},
};

static volatile uint8_t gMusic = MUS_NONE;
static volatile uint8_t gVol = 7;

static int16_t buf[256 * 2];  // estereo intercalado (L=R)
static int16_t mono[256];
static GbSynth gSyn;

// Las tablas SFX estan en Hz; el sintetizador habla el registro de frecuencia
// del Game Boy. Convertir aqui evita reescribir las cues.
static uint16_t hzToGb(uint16_t hz) {
  if (!hz) return 0;
  int32_t f = 2048 - (int32_t)(131072u / hz);
  return (f < 0 || f > 2047) ? 0 : (uint16_t)f;
}

// Renderiza `ms` de audio y lo empuja al codec. Todo suena por aqui ahora --
// no hay un camino separado de "tocar un tono", que es lo que permite que la
// musica siga sonando mientras un efecto la corta por encima.
static void pump(uint32_t ms) {
  uint32_t left = (uint32_t)((uint64_t)ms * GB_RATE / 1000);
  while (left) {
    size_t n = left > 256 ? 256 : left;
    gSyn.render(mono, n, gVol);
    for (size_t i = 0; i < n; i++) { buf[i * 2] = mono[i]; buf[i * 2 + 1] = mono[i]; }
    i2s.write((uint8_t *)buf, n * 4);
    left -= n;
  }
}

static void audioTask(void *) {
  uint8_t id;
  // por donde va cada canal de musica, y cuando termina su nota actual
  uint16_t mi1 = 0, mi2 = 0;
  uint32_t at1 = 0, at2 = 0, clock = 0;
  uint8_t playing = MUS_NONE;
  bool ampOn = false;

  for (;;) {
    uint8_t m = gMusic;
    bool wantAudio = (m != MUS_NONE) || gSyn.busy();
    if (!gOn || !gReady) { gSyn.allOff(); m = MUS_NONE; wantAudio = false; }

    // Un efecto siempre gana la voz de melodia. Con tres voces una cue que se
    // mezclara educadamente por debajo seria solo barro; cortar por encima es
    // a la vez mas simple y la prioridad correcta.
    if (xQueueReceive(gQ, &id, wantAudio ? 0 : pdMS_TO_TICKS(40)) == pdTRUE) {
      if (gOn && gReady && id < SFX_COUNT) {
        if (!ampOn) { digitalWrite(PA, HIGH); delay(6); ampOn = true; }
        const SfxDef &d = SFX[id];
        for (uint8_t i = 0; i < d.len; i++) {
          uint16_t f = hzToGb(d.n[i].f);
          // percusion para las cues de impacto, un pulso para el resto
          if (id == SFX_HIT || id == SFX_FAINT)
            gSyn.noise(13, -1, 2, d.n[i].ms, (uint16_t)(4 + i * 4));
          else if (f)
            gSyn.note(0, f, 1, 14, -1, 4, d.n[i].ms);
          else
            gSyn.silence(0);
          pump(d.n[i].ms);
        }
      }
      continue;
    }

    if (m == MUS_NONE) {
      if (ampOn && !gSyn.busy()) { digitalWrite(PA, LOW); ampOn = false; }
      playing = MUS_NONE; mi1 = mi2 = 0; at1 = at2 = clock = 0;
      continue;
    }

    if (m != playing) { playing = m; mi1 = mi2 = 0; at1 = at2 = clock = 0; }
    if (!ampOn) { digitalWrite(PA, HIGH); delay(6); ampOn = true; }

    // 0 = el combate de gimnasio, 3 = la fanfarria de victoria.
    const MusicTrack &t = MUSIC_TBL[(m == MUS_VICTORY) ? 3 : 0];
    if (clock >= at1) {
      if (mi1 >= t.n1) {                       // loop, o para un one-shot
        if (m == MUS_VICTORY) { gMusic = MUS_NONE; continue; }
        mi1 = 0; at1 = clock;
      }
      const MusicNote &n = t.ch1[mi1++];
      if (n.freq) gSyn.note(0, n.freq, n.duty, n.vol, n.envDir, n.envPeriod, n.ms);
      else gSyn.silence(0);
      at1 = clock + n.ms;
    }
    if (clock >= at2) {
      if (mi2 >= t.n2) { mi2 = 0; at2 = clock; }
      const MusicNote &n = t.ch2[mi2++];
      if (n.freq) gSyn.note(1, n.freq, n.duty, n.vol, n.envDir, n.envPeriod, n.ms);
      else gSyn.silence(1);
      at2 = clock + n.ms;
    }
    uint32_t next = (at1 < at2) ? at1 : at2;
    if (next <= clock) next = clock + 5;
    uint32_t step = next - clock;
    if (step > 60) step = 60;                  // se mantiene sensible a un efecto
    pump(step);
    clock += step;
  }
}

void audioMusic(uint8_t id) { gMusic = (id < 3) ? id : MUS_NONE; }

void audioBegin() {
  // I2S primero: arranca el MCLK que necesita el codec para engancharse
  pinMode(PA, OUTPUT);
  digitalWrite(PA, LOW);   // amp apagado; la tarea lo enciende al reproducir

  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("I2S begin fallo");
    return;
  }
  if (!es8311Init()) { Serial.println("ES8311 no responde (audio off)"); return; }

  Preferences p;
  p.begin("tamapoke", true);
  gOn = p.getBool("snd", true);
  gVol = p.getUChar("vol", 7);
  if (gVol > 10) gVol = 7;
  p.end();

  gReady = true;
  gQ = xQueueCreate(8, sizeof(uint8_t));
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 1, nullptr, 0);
  sfxPlay(SFX_HATCH);  // jingle de arranque (confirma que suena)
}

void sfxPlay(uint8_t id) {
  if (gReady && gOn && gQ) xQueueSend(gQ, &id, 0);  // descarta si la cola esta llena
}

void audioSetVolume(uint8_t v) {
  gVol = v > 10 ? 10 : v;
  Preferences p;
  p.begin("tamapoke", false);
  p.putUChar("vol", gVol);
  p.end();
}
uint8_t audioVolume() { return gVol; }

void audioSetEnabled(bool on) {
  gOn = on;
  Preferences p;
  p.begin("tamapoke", false);
  p.putBool("snd", on);
  p.end();
}
bool audioEnabled() { return gOn; }

#else
// ---------------------------------------------------------------------------
// 1.43 y 2.8" redonda: ninguna de las dos trae codec de audio ni amplificador
// (ni ES8311 ni equivalente en sus esquematicos oficiales). Este archivo es un
// stub: conserva la interfaz de audio.h (para no tener que tocar TamaPoke.ino,
// que llama audioBegin(), sfxPlay(), audioMusic(), etc. en varios puntos) pero
// no reproduce nada.
//
// El volumen/on-off se siguen guardando en Preferences para que la pantalla de
// ajustes (que lee audioVolume()/audioEnabled()) no se comporte de forma rara
// -- simplemente no habra sonido nunca, silenciosamente.
// ---------------------------------------------------------------------------

static bool gOn = false;
static uint8_t gVol = 0;
static bool gPrefsLoaded = false;

static void loadPrefsOnce() {
  if (gPrefsLoaded) return;
  Preferences p;
  p.begin("tamapoke", true);
  gOn = p.getBool("snd", false);   // por defecto OFF: no hay hardware de audio
  gVol = p.getUChar("vol", 7);
  if (gVol > 10) gVol = 7;
  p.end();
  gPrefsLoaded = true;
}

void audioBegin() {
  loadPrefsOnce();
  Serial.println("audio: sin codec en esta placa, audio deshabilitado");
}

void sfxPlay(uint8_t) { /* no-op: sin hardware de audio */ }
void audioMusic(uint8_t) { /* no-op: sin hardware de audio */ }

void audioSetVolume(uint8_t v) {
  gVol = v > 10 ? 10 : v;
  Preferences p;
  p.begin("tamapoke", false);
  p.putUChar("vol", gVol);
  p.end();
}
uint8_t audioVolume() { loadPrefsOnce(); return gVol; }

void audioSetEnabled(bool on) {
  gOn = on;
  Preferences p;
  p.begin("tamapoke", false);
  p.putBool("snd", on);
  p.end();
}
bool audioEnabled() { loadPrefsOnce(); return gOn; }

#endif
