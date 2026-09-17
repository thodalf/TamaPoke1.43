#include "audio.h"
#include "pin_config.h"
#include <Arduino.h>
#include <Preferences.h>

// ---------------------------------------------------------------------------
// NOTA PORTAGE 1.43: la Waveshare 1.43 no trae codec ES8311 ni amplificador
// integrados (a diferencia de la 1.75), asi que no hay a que hardware
// enganchar el I2S. Este archivo es un stub: conserva la interfaz de
// audio.h (para no tener que tocar TamaPoke.ino, que llama audioBegin(),
// sfxPlay(), audioMusic(), etc. en varios puntos) pero no reproduce nada.
//
// El volumen/on-off se siguen guardando en Preferences para que la pantalla
// de ajustes (que lee audioVolume()/audioEnabled()) no se comporte de forma
// rara -- simplemente no habra sonido nunca, silenciosamente.
//
// Si se agrega un modulo I2S externo (DAC + ampli + altavoz) en el futuro,
// este archivo es el unico que hay que reescribir: la interfaz de audio.h
// no cambia.
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
  Serial.println("audio: sin codec en esta placa (1.43), audio deshabilitado");
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
