// A revived companion is FROZEN: it does not age, cannot evolve, and cannot be
// lost. Those three are the whole feature, so each is checked directly.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "dex.h"
#include <cstdio>
uint32_t g_seed=11; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
static uint32_t g_ms=0; uint32_t millis(){return g_ms;}
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

int main(){
  Pet p; p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  PartyMon m; m.dex=6; m.level=61; m.shiny=1;
  m.ivAtk=m.ivDef=m.ivSpe=m.ivHp=24; m.trAtk=m.trDef=m.trSpe=35; m.trHp=40;
  m.moves[0]=1; m.moves[1]=2;
  snprintf(m.nick,sizeof(m.nick),"BLAZE");

  p.reviveFrom(m);
  ck(p.speciesId==6 && p.level()==61, "comes back at the level it was banked at");
  ck(p.shiny && !strcmp(p.nick,"BLAZE"), "keeps its shininess and its name");
  ck(p.moves[0]==1 && p.moves[1]==2, "and its moveset");
  // trHp joined trAtk/trDef/trSpe after this function was first written, and
  // only got copied into snapshotForParty() -- reviveFrom() kept silently
  // dropping it until adoptFrom() unified the two field lists.
  ck(p.trHp==40, "and its VIT training");
  ck(p.frozen, "and is marked frozen");

  // it must not age, however long passes
  uint8_t lvl = p.level();
  for (int i=0;i<400;i++){ g_ms += 60000; p.update(g_ms);
    p.fullness=p.joy=p.energy=p.hygiene=100; }
  printf("     after ~400 game-minutes: level %u (was %u)\n", p.level(), lvl);
  ck(p.level()==lvl, "does not age");

  // and cannot be taken away
  p.ageMinutes = 10UL*24*60;
  ck(!p.canFarewellNow(), "is never offered a farewell");
  p.fullness=p.joy=p.energy=p.hygiene=0;
  for (int i=0;i<200;i++){ g_ms += 60000; p.update(g_ms); }
  ck(!p.canRunawayNow(), "cannot run away even when wholly neglected");
  ck(!p.canEvolveNow(), "cannot evolve past the form it was banked in");

  // it survives a reload, still frozen
  Pet q; q.begin();
  ck(q.frozen && q.speciesId==6, "stays frozen across a reload");

  // and a brand new egg is a normal life again
  q.newEgg();
  ck(!q.frozen, "a new egg is not frozen");

  // swapActive(): the OTHER way a banked creature becomes live. Unlike
  // reviveFrom() it is not a companion -- it keeps ageing and can still
  // evolve, farewell, retire or run away from here on. snapshot() is the
  // other half: what the creature being swapped OUT gets banked as.
  Pet r; r.begin();
  if (r.awaitingStarter()) r.chooseStarter(1);
  if (r.isEgg()) r.dbgHatchAs(9, false);
  r.trHp = 12;
  PartyMon out = r.snapshot();
  ck(out.dex==r.speciesId && out.level==r.level() && out.trHp==12,
     "snapshot() describes the live creature, VIT training included");

  PartyMon m2; m2.dex=3; m2.level=30; m2.trHp=18;
  m2.ivAtk=m2.ivDef=m2.ivSpe=m2.ivHp=20;
  snprintf(m2.nick,sizeof(m2.nick),"BULBY");
  r.swapActive(m2);
  ck(r.speciesId==3 && r.level()==30 && r.trHp==18 && !strcmp(r.nick,"BULBY"),
     "swapActive() adopts the incoming creature, VIT training included");
  ck(!r.frozen, "...but stays UNFROZEN, unlike reviveFrom()");
  uint32_t age0 = r.ageMinutes;
  for (int i=0;i<10;i++){ g_ms += 60000; r.update(g_ms); }
  ck(r.ageMinutes > age0, "and keeps ageing afterward");

  // THE EXPLOIT this all exists to close: a starving, neglect-clocked
  // creature swapped out and back must come back exactly as it left, not
  // refilled. Before adoptFrom() restored these fields instead of hardcoding
  // fresh ones, two taps (swap away, swap back) was a free full refill of
  // every care stat AND a free reset of the runaway clock.
  r.dbgRunawayReady();      // fullness=joy=energy=hygiene=0, neglectTicks armed
  r.weight = 40; r.poops = 3; r.careMistakes = 4;
  PartyMon starving = r.snapshot();
  ck(starving.fullness==0 && starving.hygiene==0 && starving.neglectTicks>0,
     "snapshot() captures the real care state, not a fresh one");

  PartyMon fresh; fresh.dex=1; fresh.level=10;
  fresh.ivAtk=fresh.ivDef=fresh.ivSpe=fresh.ivHp=15;
  r.swapActive(fresh);   // swap away to something else first
  r.swapActive(starving);   // ...then swap the starving one back in
  ck(r.fullness==0 && r.joy==0 && r.energy==0 && r.hygiene==0 &&
     r.weight==40 && r.poops==3, "a round-trip swap does NOT refill care stats");
  ck(r.careMistakes==4 && r.canRunawayNow(),
     "...nor reset the slip-up count or the runaway clock");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
