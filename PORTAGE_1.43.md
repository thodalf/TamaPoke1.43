# Portage vers la Waveshare ESP32-S3-Touch-AMOLED-1.43

Ce fork adapte [TamaPoke](https://github.com/thodalf/TamaPoke) (lui-même fork
de [socquique/TamaPoke](https://github.com/socquique/TamaPoke)), écrit pour
la **1.75**, vers la **1.43**. Adapté : écran, tactile, SD. **Non porté** :
audio (stub, aucun son) et gestion PMU/batterie (stub, pas de %, pas de
bouton PWR dédié) — le schéma officiel confirme que la 1.43 n'a ni codec
audio ni PMU AXP2101 (voir § "Vérifié contre le schéma officiel" ci-dessous).

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
  polling tactile (`TP_INT`/`TP_RESET` n'existent pas sur cette carte)
- `tools/emu/Arduino_GFX_Library.h`, `tools/emu/TouchDrvFT6X36.hpp` — stubs
  ajoutés pour que l'émulateur desktop compile toujours

## Vérifié contre le schéma officiel (2026-09-18)

Tous les points auparavant marqués "à vérifier" ont été confirmés par lecture
directe du schéma officiel Waveshare
([PDF](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43/ESP32-S3-Touch-AMOLED-1.43-Schematic.pdf))
et du code source des bibliothèques utilisées :

1. **Tous les pins de `pin_config.h` correspondent exactement au schéma**
   (table GPIO du schéma, page 2) : `OLED_CS`=IO9, `OLED_CLK`=IO10,
   `OLED_SIO0..3`=IO11-14, `OLED_RESET`=IO21, `OLED_EN`=IO42, `IMU_INT`=IO8,
   `RTC_INT`=IO15, `BAT_ADC`=IO4, `SD_CS`=IO38, `SD_MOSI`=IO39, `SD_MISO`=IO40,
   `SD_SCLK`=IO41, I2C `SDA`=IO47/`SCL`=IO48 (bus partagé tactile/IMU/RTC).
2. **`TP_INT`/`TP_RESET` = -1 est correct, pas un placeholder.** Le schéma ne
   fait apparaître AUCUNE ligne INT ou RESET propre au tactile : le connecteur
   `J9` ("LCD", module écran+tactile intégré) n'expose que `TP_SDA`/`TP_SCL`
   vers le bus I2C partagé. Ces pins n'existent tout simplement pas sur cette
   carte — le mode polling pur dans `TamaPoke.ino` est la bonne solution, pas
   un compromis temporaire.
3. **Adresse I2C du FT3168 = 0x38, confirmée dans le code source de
   SensorLib** (`TouchDrvFT6X36.hpp` : `FT3267_SLAVE_ADDRESS` =
   `FT5206_SLAVE_ADDRESS` = `FT6X36_SLAVE_ADDRESS` = `0x38`).
4. **Classe SensorLib = `TouchDrvFT6X36`, confirmée.**
   `TouchDrvFocalTech.hpp` (le header générique FocalTech de SensorLib)
   regroupe toute la famille FT3267/FT5206/FT6X36 — dont le FT3168 fait
   partie — sous cette seule classe ; il n'existe pas de `TouchDrvFT3168` ou
   `TouchDrvFT3267` séparée.
5. **Paramètres `Arduino_SH8601` corrigés : `(0,0,0,0)`, pas `(6,0,0,0)`.**
   Le `(6,0,0,0)` venait du panneau CO5300 de la 1.75 et avait été recopié
   tel quel. La référence croisée qui règle la question est le dépôt LilyGO
   [`T-Display-S3-AMOLED-1.43-1.75`](https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED-1.43-1.75)
   (`examples/GFX/GFX.ino`), qui pilote le même chip SH8601 sur un panneau
   rond équivalent : sa branche `DO0143FAT01` (SH8601) construit le panneau
   SANS offset (`0,0,0,0` par défaut), et c'est sa branche `H0175Y003AM` /
   `DO0143FMST10` (CO5300) qui utilise `6,0,0,0` — exactement la paire qui
   avait été confondue ici.
6. **Pas de codec audio ni de PMU sur cette carte, confirmé.** Le schéma
   officiel n'a AUCUN bloc audio (pas d'ES8311, pas de pins I2S) et le bloc
   alimentation utilise un simple chargeur `ETA6098` + régulateur buck
   `MP1605GTF-Z`, sans AXP2101 ni bouton PWR dédié (seuls `Key1`=BOOT0 et
   `Key2`=RESET existent). Les stubs de `audio.cpp` et `rtcbat.cpp` sont donc
   corrects, pas provisoires.

**Attention au dépôt `waveshareteam/ESP32-S3-Touch-AMOLED-1.43C`** (avec un
"C") trouvé en cherchant du code officiel : c'est une **carte différente**,
sans RTC/IMU/TF card mais avec un codec ES8311 — l'inverse de cette carte.
Son pinout ne s'applique PAS ici ; il a été examiné puis écarté pendant cette
vérification.

## Ce qui reste réellement incertain

- **L'orientation de montage du panneau** (`setMirrorXY(true, true)` dans
  `TamaPoke.ino`, pour un panneau suppposé monté à 180°) est une question
  d'assemblage mécanique, pas de schématique — invérifiable sans la carte en
  main. À corriger au premier flash si l'image sort inversée.
- **Le modèle exact du panel SH8601** (paramètres d'init fins, gamma, etc. au
  -delà des offsets ci-dessus) peut varier d'un lot à l'autre ; comparer avec
  l'écran allumé si les couleurs/contrastes semblent décalés.

## Non testé sur matériel

Cette adaptation a été faite par lecture du schéma officiel et du code source
des bibliothèques, **sans carte 1.43 sous la main pour compiler/flasher**.
Compile d'abord, teste étape par étape (écran seul, puis tactile, puis SD)
plutôt que de tout flasher d'un coup.
