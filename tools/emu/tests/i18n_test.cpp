// Checks the positional STRINGS table: a language row with too few entries is
// zero-padded by the compiler with no diagnostic, which silently shifts every
// string after the gap. Also enforces the ASCII-only rule (the bitmap font has
// no glyphs for accents).
#include "Arduino.h"
#include "Preferences.h"
// linked against the same core as every other suite, so it needs the same
// hardware stubs even though it only exercises the string table
uint32_t g_seed = 1;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
bool wasPressed = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
#include "i18n.h"
#include "dex.h"
#include "moves.h"
#include <cstdio>
#include <cstring>

// STRINGS is file-static, so go through the same accessor the firmware uses.
static const char *LANG[] = { "ES", "EN", "FR", "DE", "IT", "PT" };

int main() {
  int bad = 0;
  for (int l = 0; l < LANG_COUNT; l++)
    for (int s = 0; s < STR_COUNT; s++) {
      setLang((Lang)l);
      const char *v = T((StrId)s);
      if (!v) { printf("NULL  %s index %d\n", LANG[l], s); bad++; continue; }
      for (const unsigned char *p = (const unsigned char *)v; *p; p++)
        if (*p > 0x7F) { printf("NON-ASCII  %s index %d: \"%s\"\n", LANG[l], s, v); bad++; break; }
    }
  printf("%s: %d languages x %d strings\n", bad ? "FAIL" : "PASS", LANG_COUNT, STR_COUNT);

  // DEX_NAMES was hand-written once at DEX_COUNT 386 and never extended
  // through the later expansions -- every entry past it is a raw nullptr in
  // every language. dexName() must fall back rather than hand gfx->print() a
  // null, for every dex the game can actually reach, not just the ones this
  // table happens to cover.
  int dbad = 0;
  for (int l = 0; l < LANG_COUNT; l++) {
    setLang((Lang)l);
    for (int d = 0; d <= DEX_COUNT; d++) {
      const char *v = dexName(d);
      if (!v) { printf("NULL  dexName %s dex %d\n", LANG[l], d); dbad++; }
    }
  }
  printf("%s: dexName never null across %d languages x dex 0-%d\n",
         dbad ? "FAIL" : "PASS", LANG_COUNT, DEX_COUNT);
  bad += dbad;

  // Same shape as DEX_NAMES: MOVE_NAMES only has a FR row (moves.h), so this
  // checks moveName() never hands a null AND that the FR translations are
  // themselves ASCII-only, same rule as STRINGS above.
  int mbad = 0;
  for (int l = 0; l < LANG_COUNT; l++) {
    setLang((Lang)l);
    for (int m = 0; m < MOVE_COUNT; m++) {
      const char *v = moveName(m);
      if (!v) { printf("NULL  moveName %s move %d\n", LANG[l], m); mbad++; continue; }
      for (const unsigned char *p = (const unsigned char *)v; *p; p++)
        if (*p > 0x7F) { printf("NON-ASCII  moveName %s move %d: \"%s\"\n", LANG[l], m, v); mbad++; break; }
    }
  }
  printf("%s: moveName never null and ASCII-only across %d languages x %d moves\n",
         mbad ? "FAIL" : "PASS", LANG_COUNT, MOVE_COUNT);
  bad += mbad;
  return bad ? 1 : 0;
}
