// Host versions of the three hardware modules. The sprite loader is the real
// TPK2/TPTH parser, just reading from a directory instead of the SD card.
#include "Arduino.h"
#include "sdmon.h"
#include "rtcbat.h"
#include "audio.h"
#include "linknow.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <dirent.h>

// Baked in by build.sh from the repo location; --sprites <dir> overrides it.
#ifndef SPRITE_DIR
#define SPRITE_DIR "../sdcard/mons"
#endif
static std::string g_spriteDir = SPRITE_DIR;
void emuSetSpriteDir(const char *d) { g_spriteDir = d; }

bool sdReady = true;
bool sdDirty = false;
// The firmware rescans region art when a pack lands over PUT. There is no card
// here and no PUT path, so the flag never sets and the scan is a no-op -- but both
// symbols are referenced from loop(), so they have to exist to link.
bool sdArtDirty = false;
void sdScanRegionArt(bool) {}
SdThumbs thumbs;

static uint8_t *slurp(const std::string &path, uint32_t *size) {
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) return nullptr;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n <= 0) { fclose(f); return nullptr; }
  uint8_t *b = (uint8_t *)malloc(n);
  if (!b || fread(b, 1, n, f) != (size_t)n) { free(b); fclose(f); return nullptr; }
  fclose(f);
  *size = (uint32_t)n;
  return b;
}

bool PmdMon::load(int16_t dexNum, bool shiny) {
  if (dexNum < 1 || dexNum > 999) return false;
  unload();
  // 64 silently truncated every path on a deeply nested checkout (a Windows
  // repo path alone can run 60+ chars before the filename even starts), so
  // fopen() failed on the truncated remainder -- every sprite "missing" with
  // real files sitting right there. This is host/dev-tool code, not firmware,
  // so there is no reason to keep it MCU-tight.
  char p[256];
  snprintf(p, sizeof(p), "%s/p%s%03u.bin", g_spriteDir.c_str(), shiny ? "s" : "", (unsigned)dexNum);
  uint32_t size = 0;
  blob = slurp(p, &size);
  if (!blob) {
    snprintf(p, sizeof(p), "%s/p%03u.bin", g_spriteDir.c_str(), (unsigned)dexNum);
    blob = slurp(p, &size);
  }
  if (!blob) return false;
  if (size < 7 || memcmp(blob, "TPK2", 4) != 0) { unload(); return false; }

  uint8_t nActs = blob[4];
  memcpy(&palCount, blob + 5, 2);
  if (palCount > 256 || (uint32_t)7 + palCount * 2 > size) { unload(); return false; }
  memcpy(pal, blob + 7, palCount * 2);

  const uint8_t *q = blob + 7 + palCount * 2, *end = blob + size;
  for (uint8_t i = 0; i < nActs && q + 4 <= end; i++) {
    uint8_t id = q[0], w = q[1], h = q[2], nf = q[3];
    q += 4;
    if (id >= PMD_NACTS || nf > 24) { unload(); return false; }
    uint32_t bytes = (uint32_t)nf * 2 + (uint32_t)w * h * nf;
    if (w == 0 || h == 0 || nf == 0 || q + bytes > end) { unload(); return false; }
    PmdAct &a = acts[id];
    a.w = w; a.h = h; a.frames = nf;
    for (uint8_t k = 0; k < nf; k++) { a.ms[k] = q[0] | (q[1] << 8); q += 2; }
    a.data = q;
    q += (uint32_t)w * h * nf;
    uint8_t base = 1;
    for (uint8_t f = 0; f < nf; f++) {
      const uint8_t *fr = a.data + (uint32_t)f * w * h;
      for (int r = h - 1; r >= 0; r--) {
        bool any = false;
        for (int c = 0; c < w && !any; c++) if (fr[r * w + c] != 0xFF) any = true;
        if (any) { if (r + 1 > base) base = r + 1; break; }
      }
    }
    a.base = base;
  }
  dex = dexNum;
  loaded = true;
  return true;
}

void PmdMon::unload() {
  if (blob) { free(blob); blob = nullptr; }
  for (auto &a : acts) { a.w = a.h = a.frames = a.base = 0; a.data = nullptr; }
  loaded = false;
}

bool SdThumbs::load() {
  uint32_t size = 0;
  data = slurp(g_spriteDir + "/thumbs.bin", &size);
  if (!data) { Serial.println("emu: no thumbs.bin"); return false; }
  if (memcmp(data, "TPTH", 4) != 0) { free(data); data = nullptr; return false; }
  memcpy(&count, data + 4, 2);
  loaded = true;
  Serial.printf("emu: thumbnails loaded: %u\n", count);
  return true;
}

const uint8_t *SdThumbs::get(int16_t dex) const {
  if (!loaded || dex < 1 || dex > count) return nullptr;
  uint32_t off;
  memcpy(&off, data + 6 + 4 * (dex - 1), 4);
  return data + off;
}

bool SdMon::load(int16_t, bool) { return false; }   // legacy TPK1 path unused
void SdMon::unload() { if (data) { free(data); data = nullptr; } loaded = false; }
bool sdBegin() {
  Serial.printf("emu: sprites from %s\n", g_spriteDir.c_str());
  return true;
}
bool sdSerialCommand(const String &) { return false; }

// --- RSVP books: read from the sibling "books" dir next to the sprite dir,
// e.g. tools/sdcard/mons -> tools/sdcard/books, so the emulator can exercise
// the reader interactively without a board. Same shape as the real firmware's
// BookReader (sdmon.cpp): no open handle kept between calls, just the name
// and byte offset, re-opened per nextWord() -- host disk I/O is cheap enough
// that this is not worth a second code path.
static std::string bookDir() {
  size_t slash = g_spriteDir.find_last_of('/');
  std::string base = (slash != std::string::npos) ? g_spriteDir.substr(0, slash) : ".";
  return base + "/books";
}

BookReader gBook;

bool BookReader::open(const char *bookName) {
  close();
  std::string path = bookDir() + "/" + bookName + ".txt";
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fclose(f);
  if (n < 0) return false;
  fileSize = (uint32_t)n;
  strncpy(name, bookName, sizeof(name) - 1);
  name[sizeof(name) - 1] = 0;
  bytePos = 0;
  loaded = true;
  return true;
}

void BookReader::close() {
  loaded = false;
  fileSize = bytePos = 0;
  name[0] = 0;
}

void BookReader::seek(uint32_t at) {
  if (!loaded) return;
  bytePos = (at > fileSize) ? fileSize : at;
}

static inline bool isRsvpSpaceHost(int c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

bool BookReader::nextWord(char *out, size_t outCap) {
  out[0] = 0;
  if (!loaded) return false;
  std::string path = bookDir() + "/" + name + ".txt";
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) { loaded = false; return false; }
  fseek(f, (long)bytePos, SEEK_SET);
  bool ok = false;
  for (;;) {
    int c;
    do {
      c = fgetc(f);
      if (c == EOF) break;
      bytePos++;
    } while (isRsvpSpaceHost(c));
    if (c == EOF) break;
    size_t n = 0;
    while (c != EOF && !isRsvpSpaceHost(c)) {
      if (c >= 0x20 && c < 0x7F && n + 1 < outCap) out[n++] = (char)c;
      c = fgetc(f);
      if (c != EOF) bytePos++;
    }
    out[n] = 0;
    if (n > 0) { ok = true; break; }
    if (c == EOF) break;   // EOF right at the end of a dropped-only token
    // else: token was entirely non-ASCII -- loop for the next real word
  }
  fclose(f);
  return ok;
}

uint8_t sdListBooks(char names[][24], uint8_t max) {
  uint8_t n = 0;
  DIR *d = opendir(bookDir().c_str());
  if (!d) return 0;
  struct dirent *e;
  while (n < max && (e = readdir(d))) {
    std::string nm = e->d_name;
    if (nm.size() > 4 && nm.substr(nm.size() - 4) == ".txt") {
      std::string base = nm.substr(0, nm.size() - 4);
      strncpy(names[n], base.c_str(), 23);
      names[n][23] = 0;
      n++;
    }
  }
  closedir(d);
  return n;
}

// --- RTC / battery / PMU ---
static uint32_t g_epoch = 0;
bool rtcBegin() { return true; }
uint32_t rtcEpoch() { return g_epoch + millis() / 1000; }
void rtcSetEpoch(uint32_t e) { g_epoch = e - millis() / 1000; }
bool batBegin() { return true; }
void pmuEnablePanel() {}
int batPercent() { return 87; }
bool batCharging() { return false; }
bool usbPresent() { return true; }
uint32_t batRawMv() { return 4000; }
void pwrSetup() {}
bool pwrShortPressed() { return false; }

// --- audio (silent) ---
void audioBegin() {}
void sfxPlay(uint8_t) {}
// no radio here; the protocol itself is exercised by tests/link_test.cpp
struct Link;
// No radio here at all, which is why lossy_test drives Link directly instead
// of going through this.
bool linkNowBegin(Link *) { return false; }
void linkNowEnd() {}
bool linkNowUp() { return false; }
void linkNowPoll() {}
static LinkNowStats gNoStats;
const LinkNowStats &linkNowStats() { return gNoStats; }
// audio is silent here, but the sketch calls these, so they have to exist
static uint8_t g_emuVol = 7, g_emuMusic = 0;
void audioMusic(uint8_t id) { g_emuMusic = id; }
void audioSetVolume(uint8_t v) { g_emuVol = v > 10 ? 10 : v; }
uint8_t audioVolume() { return g_emuVol; }
void audioSetEnabled(bool on) { (void)on; }
bool audioEnabled() { return true; }

// Why the last run ended. The tests link this file but not main_sdl.cpp, so the
// state lives here and the GUI sets it after reading a simulated crash.
static int g_resetReason = ESP_RST_POWERON;
esp_reset_reason_t emuResetReason() { return g_resetReason; }
void emuSetResetReason(int r) { g_resetReason = r; }
