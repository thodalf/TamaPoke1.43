#pragma once
#include <Arduino.h>
#include <Preferences.h>

// The party: pets that finished their life and were kept, rather than being
// dissolved into a single Pokedex bit like every previous ending did.
//
// Only the two endings the player CHOOSES bank a pet -- farewell and release.
// A runaway does not: it is the game's one punishing outcome, and letting a
// neglected pet come back as a team member would take the sting out of it.
#define PARTY_SLOTS 6
// The box: storage beyond the six that fight. Deliberately a SEPARATE NVS key
// rather than a bigger party blob -- growing that blob would change its stride
// and the length-based migration in begin() cannot tell a stride change from a
// slot-count change, so an existing party would be read back misaligned. A new
// key is purely additive and cannot corrupt anything.
#define BOX_SLOTS 18
#define MOVE_SLOTS 4    // the same four every trainer gets in the real games

// A retired pet. Its level is frozen at the moment it joined; it does not keep
// ageing, and nothing about it can be trained any further.
struct PartyMon {
  int16_t dex = 0;      // Pokedex number, 0 = empty slot
  uint16_t level = 1;   // frozen at the moment it was banked
  uint16_t medals = 0;  // what it earned in life
  uint8_t ivAtk = 0, ivDef = 0, ivSpe = 0, ivHp = 0;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  uint8_t shiny = 0;
  char nick[12] = "";
  // Frozen with everything else: the moves you chose while it was alive are
  // what it fights with forever. 0 = empty slot (MOVE_TBL[0] is the "-" filler).
  // Appended at the END of the struct on purpose -- Party::begin() migrates
  // older, shorter blobs by length, and that only works if nothing moved.
  uint8_t moves[MOVE_SLOTS] = { 0, 0, 0, 0 };
  // trHp joins trAtk/trDef/trSpe above in spirit, but has to live down HERE:
  // it is newer than moves[], and Party::begin()'s length-based migration
  // copies an old (shorter) record straight into the FRONT of this struct,
  // so anything added after this project's last expansion has to stay
  // physically last too, or every existing banked creature's bytes shift.
  uint8_t trHp = 0;
  // Care state at the moment this creature was banked. WITHOUT this, swapping
  // the live pet for a party member (Pet::swapActive) would have nothing real
  // to restore and would quietly refill food/energy/joy/hygiene back to a
  // fresh baseline every time -- a free reset a player could exploit by
  // swapping back and forth instead of actually caring for anything. Same
  // append-only reasoning as trHp above: this has to stay physically last.
  uint8_t fullness = 80, joy = 80, energy = 80, hygiene = 100, weight = 0, poops = 0;
  // Same exploit reasoning for the neglect clock and the slip-up count: without
  // these, swapping a neglected creature out and back would quietly restart its
  // hour-of-total-neglect timer for free, delaying a runaway it had otherwise
  // earned.
  uint8_t neglectTicks = 0, careMistakes = 0, mistakeCooldown = 0;

  bool empty() const { return dex < 1; }
};

class Party {
public:
  PartyMon slots[PARTY_SLOTS];
  PartyMon box[BOX_SLOTS];

  void begin();                 // load from NVS
  uint8_t count() const;
  bool isFull() const { return count() >= PARTY_SLOTS; }
  int firstFree() const;        // index of the first empty slot, -1 if full
  bool add(const PartyMon &m);  // into the first free slot; false if full
  void replaceAt(uint8_t i, const PartyMon &m);
  void releaseAt(uint8_t i);    // free a slot again
  void save();
  uint8_t boxCount() const;
  int boxFirstFree() const;
  bool boxAdd(const PartyMon &m);     // into the first free box slot
  void boxReleaseAt(uint8_t i);
  void boxSave();
  // Swaps a party slot with a box slot. Either may be empty, so this doubles as
  // deposit and withdraw rather than needing three separate operations.
  void swapPartyBox(uint8_t partyIdx, uint8_t boxIdx);

  // combat stats of a party member, same formula as the live pet's
  uint16_t atkOf(const PartyMon &m) const;
  uint16_t defOf(const PartyMon &m) const;
  uint16_t speOf(const PartyMon &m) const;
  uint16_t vitOf(const PartyMon &m) const;
  uint16_t spaOf(const PartyMon &m) const;
  uint16_t spdOf(const PartyMon &m) const;

private:
  Preferences prefs;
};

extern Party party;
