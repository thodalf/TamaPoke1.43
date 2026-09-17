// Wild encounters: the capture-chance formula, the species pool (always
// R_COMUN, always region-gated), the btlWild flag never leaking between
// battle types, the one-off win/lose message fix, and a full party-and-box
// capture falling into the exact "choose who" flow a farewell already uses.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "moves.h"
#include "dex.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 77;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render(); void loop(); void battleTap(int16_t, int16_t);
void emuSetTimeScale(uint32_t);
extern Pet pet;
extern bool battleOpen, btlOver, btlWon, btlWild;
extern Combatant btlYou, btlFoe;
extern int8_t btlTrainer;
extern uint8_t btlMsgCount, btlMenu;
extern char btlMsg[6][40];
extern uint32_t btlBallUntil;
extern bool partyOpen, partyPick;
void startWildBattle();
void startTrainerBattle(uint8_t idx, bool hard);
void startBattle(int16_t dex, uint8_t lvl);

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// Geometry mirrors TamaPoke.ino's own BTL_CELL_* macros (release_test.cpp
// does the same for the party grid): a test that restated the layout as
// different numbers would drift from the sketch and prove nothing.
#define BTL_CELL_W 160
#define BTL_CELL_H 44
#define BTL_GRID_X 69
#define BTL_GRID_Y 274
#define BTL_CELL_X(i) (BTL_GRID_X + ((i) % 2) * (BTL_CELL_W + 8))
#define BTL_CELL_Y(i) (BTL_GRID_Y + ((i) / 2) * (BTL_CELL_H + 8))
static void tapCell(int i) { battleTap(BTL_CELL_X(i) + BTL_CELL_W / 2, BTL_CELL_Y(i) + BTL_CELL_H / 2); }

static void fillPartyAndBox() {
  for (int i = 0; i < PARTY_SLOTS; i++) { PartyMon m; m.dex = 10 + i; m.level = 20;
    m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20; party.replaceAt(i, m); }
  for (int i = 0; i < BOX_SLOTS; i++) { PartyMon m; m.dex = 30 + i; m.level = 20;
    m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20; party.box[i] = m; }
  party.save(); party.boxSave();
}
static void emptyPartyAndBox() {
  for (int i = 0; i < PARTY_SLOTS; i++) party.releaseAt(i);
  for (int i = 0; i < BOX_SLOTS; i++) party.boxReleaseAt(i);
}

int main() {
  setup();
  for (int i = 0; i < 4; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(6, false);
  pet.ageMinutes = 40UL * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  emptyPartyAndBox();

  // ---- captureChancePct: pure formula, needs no battle at all
  {
    Combatant f; f.maxHp = 100;
    f.hp = 100; ck(captureChancePct(f) == CATCH_MIN_PCT, "full HP is the hard floor");
    f.hp = 1;   ck(captureChancePct(f) >= CATCH_MAX_PCT - 1, "near-fainted is close to the easy ceiling");
    f.hp = 50;  uint8_t mid = captureChancePct(f);
    ck(mid > CATCH_MIN_PCT && mid < CATCH_MAX_PCT, "half HP sits strictly between the two");
  }

  // ---- the species pool is always R_COMUN
  {
    bool allComun = true;
    for (int i = 0; i < 200; i++) {
      startWildBattle();
      if (DEX_TBL[btlFoe.dex].rarity != R_COMUN) allComun = false;
    }
    ck(allComun, "every wild encounter is a R_COMUN species, across 200 draws");
  }

  // ---- btlWild must not leak between battle types -- the exact concern
  // btlResetCommon() exists to close off
  {
    startBattle(9, 50);
    ck(!btlWild, "the debug BATTLE command is never a wild fight");
    startTrainerBattle(0, false);
    ck(!btlWild, "nor is a gym fight");
    startWildBattle();
    ck(btlWild, "startWildBattle sets it");
    startTrainerBattle(0, false);
    ck(!btlWild, "and a gym fight right after a wild one clears it again");
  }

  // ---- regression: a one-off win (btlTrainer < 0) used to print "You lost..."
  {
    startBattle(9, 1);
    ck(!btlWild && btlTrainer < 0, "a one-off fight, not a gym or wild one");
    btlFoe.hp = 1;
    for (int s = 0; s < MOVE_SLOTS && !btlOver; s++) {
      if (!btlYou.moves[s]) continue;
      btlMenu = 1; btlMsgCount = 0;
      battleTap(BTL_CELL_X(s) + BTL_CELL_W / 2, BTL_CELL_Y(s) + BTL_CELL_H / 2);
    }
    ck(btlOver && btlWon, "a near-fainted one-off foe is beaten");
    // btlSay() queues one line per call (used X, super effective, fainted...),
    // so the win/lose text is whichever line was appended LAST, not btlMsg[0].
    ck(btlMsgCount > 0 && !strcmp(btlMsg[btlMsgCount - 1], "You win!"),
       "and it says WIN, not LOSE (regression for the btlTrainer<0 message bug)");
  }

  // ---- a captured wild creature with party AND box both full falls into
  // the exact same "choose who to replace" flow a farewell already uses
  {
    fillPartyAndBox();
    bool caught = false;
    emuSetTimeScale(3000);
    for (int attempt = 0; attempt < 20 && !caught; attempt++) {
      startWildBattle();
      btlFoe.hp = 1;             // CATCH_MAX_PCT chance: succeeds almost every time
      btlMenu = 0;
      tapCell(2);                // CAPTURE, the wild-only 4-cell grid
      for (int i = 0; i < 3000 && btlBallUntil; i++) loop();
      if (pet.endedKind == CER_CAUGHT) {
        caught = true;
      } else {
        pet.endedKind = CER_NONE;
        battleOpen = false;      // a miss: clean up and retry
      }
    }
    emuSetTimeScale(1);
    ck(caught, "a near-fainted wild foe is eventually captured");
    ck(!battleOpen, "and the battle closes immediately on a catch");
    ck(partyPick, "party+box both full: lands in the existing choose-who screen");
    emptyPartyAndBox();
    partyPick = false; partyOpen = false; pet.endedKind = CER_NONE;
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
