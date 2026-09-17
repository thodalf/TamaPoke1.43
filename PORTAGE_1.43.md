# Portage vers la Waveshare ESP32-S3-Touch-AMOLED-1.43

Ce fork adapte [TamaPoke](https://github.com/thodalf/TamaPoke) (lui-même fork
de [socquique/TamaPoke](https://github.com/socquique/TamaPoke)), écrit pour
la **1.75**, vers la **1.43**. Adapté : écran, tactile, SD. **Non porté** :
audio (stub, aucun son) et gestion PMU/batterie (stub, pas de %, pas de
bouton PWR dédié) — la 1.43 n'a ni codec ES8311 ni PMU AXP2101.

## Fichiers modifiés

- `pin_config.h` — réécrit entièrement pour le pinout officiel 1.43
- `rtcbat.cpp` / `rtcbat.h` — PMU/AXP2101 retirés, RTC PCF85063 conservé,
  fonctions batterie/PWR en stub (interface intacte pour ne pas toucher au
  reste du firmware)
- `audio.cpp` — stub sans hardware (interface `audio.h` intacte, seul le
  volume/on-off restent stockés en préférences, sans effet)
- `TamaPoke.ino` — driver écran `Arduino_SH8601` (au lieu de `Arduino_CO5300`),
  driver tactile `TouchDrvFT6X36` (au lieu de `TouchDrvCST92xx`), activation
  du rail écran par GPIO `LCD_EN` (remplace `pmuEnablePanel()`), repli en
  polling tactile si `TP_INT` n'est pas câblé
- `tools/emu/Arduino_GFX_Library.h`, `tools/emu/TouchDrvFT6X36.hpp` — stubs
  ajoutés pour que l'émulateur desktop compile toujours

## ⚠️ À vérifier avant de flasher (non confirmé depuis le pinout public)

1. **`TP_INT` / `TP_RESET`** (`pin_config.h`, actuellement `-1`) — pas dans
   le tableau de pins public Waveshare. À confirmer sur le
   [schématique officiel](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43/ESP32-S3-Touch-AMOLED-1.43-Schematic.pdf).
   Tant que c'est `-1`, le tactile fonctionne en polling pur (sans IRQ) —
   ça marche, juste un peu plus de trafic I2C.
2. **Adresse I2C du FT3168** — mise à `0x38` (adresse FocalTech standard) à
   la place de `0x5A` (CST9217). À confirmer si le tactile n'est pas détecté.
3. **Classe SensorLib exacte pour FT3168** — `TouchDrvFT6X36` est ma
   meilleure estimation (famille FocalTech FT62xx/FT3267/FT3168 dans
   SensorLib) mais pas 100% certaine selon la version de la lib. Si ça ne
   compile pas, regarder `Arduino IDE → File → Examples → SensorLib →
   Touch_*` pour le nom exact.
4. **Paramètres `Arduino_SH8601`** — la signature du constructeur est
   recopiée de `Arduino_CO5300` (même forme d'appel dans `Arduino_GFX`),
   mais les 4 derniers paramètres numériques (offsets/rotation panel)
   peuvent différer d'un lot SH8601 à l'autre. Comparer avec l'exemple
   officiel Waveshare pour la 1.43 si l'image sort décalée/coupée.

## Non testé sur matériel

Cette adaptation a été faite par lecture du code source et du pinout public
Waveshare, **sans carte 1.43 sous la main pour compiler/flasher**. Compile
d'abord, corrige les 4 points ci-dessus au besoin, teste étape par étape
(écran seul, puis tactile, puis SD) plutôt que de tout flasher d'un coup.
